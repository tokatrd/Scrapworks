/******************************************************************************
 Copyright (C) 2014 by John R. Bradley <jrb@turrettech.com>
 Copyright (C) 2018 by Hugh Bailey ("Jim") <jim@obsproject.com>

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ******************************************************************************/

#include <obs-frontend-api.h>
#include <util/threading.h>
#include <util/platform.h>
#include <util/util.hpp>
#include <util/dstr.hpp>
#include <obs-module.h>
#include <obs.hpp>
#include <functional>
#include <sstream>
#include <thread>
#include <mutex>
#include <util/util.hpp>

#include "obs-browser-source.hpp"
#include "browser-scheme.hpp"
#include "browser-app.hpp"
#include "browser-version.h"
#include "browser-config.h"

#include "json11/json11.hpp"
#include "obs-websocket-api/obs-websocket-api.h"
#include "cef-headers.hpp"

#include "lovense_common.hpp"
#include "lovense_obs_common.hpp"
#include "browser-proc-handler.hpp"
#include "lovense_obs_source.hpp"
#include "lovense_source_helper.hpp"

#ifdef _WIN32
#include <util/windows/ComPtr.hpp>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <d3d11.h>
#else
#include "signal-restore.hpp"
#endif

#ifdef __APPLE__
#include "mac/CPluginsUpdate.hpp"
#endif

#ifdef ENABLE_BROWSER_QT_LOOP
#include <QApplication>
#include <QThread>
#endif

#include "lovense_obs_common.hpp"
#include "lovense_obs_proc.hpp"
#if defined(DEF_TOOLSETS)
#include "lovense_source_helper.hpp"
#include "lovense_util_helper.hpp"

#ifdef _WIN32
    #include "toolset/lovense_toolset_custom.hpp"
#endif
using namespace lovense;
#endif

OBS_DECLARE_MODULE()
#ifndef DEF_TOOLSETS
OBS_MODULE_USE_DEFAULT_LOCALE("obs-browser", "en-US")
#else
#ifdef _WIN32
OBS_MODULE_USE_DEFAULT_LOCALE("lovense-obs-toolset", "en-US")
#else
OBS_MODULE_USE_DEFAULT_LOCALE("obs-browser", "en-US")
#endif
#endif
MODULE_EXPORT const char *obs_module_description(void)
{
	return "CEF-based web browser source & panels";
}

using namespace std;
using namespace json11;

static thread manager_thread;
static bool manager_initialized = false;
#if defined(DEF_TOOLSETS) && defined(_WIN32)
os_event_t* cef_init_event = nullptr;
#endif
os_event_t *cef_started_event = nullptr;

#if defined(_WIN32)
static int adapterCount = 0;
#endif
static std::wstring deviceId;

bool hwaccel = false;

/* ========================================================================= */

#ifdef ENABLE_BROWSER_QT_LOOP
extern MessageObject messageObject;
#endif

class BrowserTask : public CefTask {
public:
	std::function<void()> task;

	inline BrowserTask(std::function<void()> task_) : task(task_) {}
	virtual void Execute() override
	{
#ifdef ENABLE_BROWSER_QT_LOOP
		/* you have to put the tasks on the Qt event queue after this
		 * call otherwise the CEF message pump may stop functioning
		 * correctly, it's only supposed to take 10ms max */
		QMetaObject::invokeMethod(&messageObject, "ExecuteTask",
					  Qt::QueuedConnection,
					  Q_ARG(MessageTask, task));
#else
		task();
#endif
	}

	IMPLEMENT_REFCOUNTING(BrowserTask);
};

bool QueueCEFTask(std::function<void()> task)
{
	return CefPostTask(TID_UI,
			   CefRefPtr<BrowserTask>(new BrowserTask(task)));
}

/* ========================================================================= */

static const char *default_css = "\
body { \
background-color: rgba(0, 0, 0, 0); \
margin: 0px auto; \
overflow: hidden; \
}";

#if 1 //lovense
static void ReplaceChar(char *pStr, char cDest, char cSrc)
{
	char *pTmp = pStr;
	while ('\0' != *pTmp) {
		if (*pTmp == cSrc)
			*pTmp = cDest;
		pTmp++;
	}
}

static std::string FilePathToHtmlPath(const char *file)
{
	if (!file || strlen(file) == 0)
		return "";
	std::string url = file;
	url = CefURIEncode(url, false);
#ifdef _WIN32
	size_t slash = url.find("%2F");
	size_t colon = url.find("%3A");

	if (slash != std::string::npos && colon != std::string::npos &&
	    colon < slash)
		url.replace(colon, 3, ":");
#endif
	while (url.find("%5C") != std::string::npos)
		url.replace(url.find("%5C"), 3, "/");

	while (url.find("%2F") != std::string::npos)
		url.replace(url.find("%2F"), 3, "/");
	return url;
}

#ifdef DEF_TOOLSETS
static void lovense_browser_source_get_defaults(obs_data_t* settings);
void toolset_source_get_defaults(obs_data_t* settings)
{
	lovense_browser_source_get_defaults(settings);
}
#endif

static void lovense_browser_source_get_defaults(obs_data_t* settings)
{
	char path[512] = {0};
	uint32_t cx = 800;
	uint32_t cy = 600;
	config_t *config = obs_frontend_get_profile_config();
	if (config) {
		cx = (uint32_t)config_get_uint(config, "Video", "BaseCX");
		cy = (uint32_t)config_get_uint(config, "Video", "BaseCY");
		blog(LOG_INFO, "[lovense] cx = %u ,cy = %u", cx, cy);
	}

	obs_data_set_default_int(settings, "width", cx);
	obs_data_set_default_int(settings, "height", cy);
	obs_data_set_default_int(settings, "fps", 30);
#ifdef ENABLE_BROWSER_SHARED_TEXTURE
	obs_data_set_default_bool(settings, "fps_custom", false);
#else
	obs_data_set_default_bool(settings, "fps_custom", true);
#endif

	obs_data_set_default_bool(settings, "shutdown", false);
	obs_data_set_default_bool(settings, "restart_when_active", false);
	obs_data_set_default_string(settings, "css", default_css);

	obs_data_set_default_bool(settings, "is_local_file", true);
	obs_data_set_default_bool(settings, "reroute_audio", true);

	
	std::string toURLFile;
    char szProPath[1024] = {0};
#ifdef WIN32
#ifndef DEF_TOOLSETS
	const char *htmlFile = "..\\..\\..\\dist\\index.html";
	char *absHtmlFile = os_get_abs_path_ptr(htmlFile);
	if (absHtmlFile) {
		toURLFile = FilePathToHtmlPath(absHtmlFile);
		bfree(absHtmlFile);
	} else
	{
		blog(LOG_INFO, "[lovense] file path empty!");
		return;
	}
    sprintf_s(szProPath, "file:///%s?port=%d", toURLFile.c_str(), lovense::DEF_WS_WEBSOCKET_PORT);
#else
	char szLocalFile[512] = { 0 };
	if (os_get_config_path(szLocalFile, sizeof(szLocalFile),
		"obs-studio\\dist\\index.html") <= 0)
		return;

	toURLFile = FilePathToHtmlPath(szLocalFile);

	auto port = lovense::proc::get_ws_server_port();
	sprintf_s(szProPath, "file:///%s?port=%d", toURLFile.c_str(), port);

	ReplaceChar(szProPath, '/', '\\');
	blog(LOG_INFO, "[lovense feedback] html = %s\n", szProPath);

	obs_data_set_default_bool(settings, "is_local_file", false);
	obs_data_set_default_string(settings, "url", szProPath);
#endif
#else
    char szLocalFile[512] = { 0 };
    std::string distFile = "lovense/dist/index.html";
    uint64_t port = lovense::DEF_WS_WEBSOCKET_PORT;
#ifdef DEF_TOOLSETS
    distFile = "obs-studio/dist/index.html";
    auto profile_config = obs_frontend_get_profile_config();
    if (profile_config) {
        port = config_get_int(profile_config, "lovense", "wsport");
        if(port == 0)
            port = lovense::DEF_TOOLSET_WS_WEBSOCKET_PORT;
    }
    else
        port = lovense::DEF_TOOLSET_WS_WEBSOCKET_PORT;
#endif
    if (os_get_config_path(szLocalFile, sizeof(szLocalFile), distFile.c_str()) <= 0){
        blog(LOG_INFO, "[lovense] file path empty!");
        return;
    }
    toURLFile = FilePathToHtmlPath(szLocalFile);
    sprintf(szProPath, "file://%s?port=%llu", toURLFile.c_str(), port);
#endif

        
	ReplaceChar(szProPath, '/', '\\');
	blog(LOG_INFO, "[lovense]  html = %s\n", szProPath);

	obs_data_set_default_bool(settings, "is_local_file", false);
	obs_data_set_default_string(settings, "url", szProPath);
}
#endif

static void browser_source_get_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "url",
				    "https://obsproject.com/browser-source");
	obs_data_set_default_int(settings, "width", 800);
	obs_data_set_default_int(settings, "height", 600);

	obs_data_set_default_int(settings, "fps", 30);
#ifdef ENABLE_BROWSER_SHARED_TEXTURE
	obs_data_set_default_bool(settings, "fps_custom", false);
#else
	obs_data_set_default_bool(settings, "fps_custom", true);
#endif
	obs_data_set_default_bool(settings, "shutdown", false);
	obs_data_set_default_bool(settings, "restart_when_active", false);
	obs_data_set_default_int(settings, "webpage_control_level",
				 (int)DEFAULT_CONTROL_LEVEL);
	obs_data_set_default_string(settings, "css", default_css);
	obs_data_set_default_bool(settings, "reroute_audio", false);
}

static bool is_local_file_modified(obs_properties_t *props, obs_property_t *,
				   obs_data_t *settings)
{
	bool enabled = obs_data_get_bool(settings, "is_local_file");
	obs_property_t *url = obs_properties_get(props, "url");
	obs_property_t *local_file = obs_properties_get(props, "local_file");
	obs_property_set_visible(url, !enabled);
	obs_property_set_visible(local_file, enabled);

	return true;
}

static bool is_fps_custom(obs_properties_t *props, obs_property_t *,
			  obs_data_t *settings)
{
	bool enabled = obs_data_get_bool(settings, "fps_custom");
	obs_property_t *fps = obs_properties_get(props, "fps");
	obs_property_set_visible(fps, enabled);

	return true;
}

static obs_properties_t* lovense_source_get_properties(void* data)
{
	obs_properties_t* props = obs_properties_create();
	BrowserSource* bs = static_cast<BrowserSource*>(data);

	obs_properties_add_button(
		props, "refreshnocache", obs_module_text("RefreshNoCache"),
		[](obs_properties_t*, obs_property_t*, void* data) {
			static_cast<BrowserSource*>(data)->Refresh();
			return false;
		});
	return props;
}

static obs_properties_t *browser_source_get_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();
	BrowserSource *bs = static_cast<BrowserSource *>(data);
	DStr path;

	obs_properties_set_flags(props, OBS_PROPERTIES_DEFER_UPDATE);
	obs_property_t *prop = obs_properties_add_bool(
		props, "is_local_file", obs_module_text("LocalFile"));

	if (bs && !bs->url.empty()) {
		const char *slash;

		dstr_copy(path, bs->url.c_str());
		dstr_replace(path, "\\", "/");
		slash = strrchr(path->array, '/');
		if (slash)
			dstr_resize(path, slash - path->array + 1);
	}

	obs_property_set_modified_callback(prop, is_local_file_modified);
	obs_properties_add_path(props, "local_file",
				obs_module_text("LocalFile"), OBS_PATH_FILE,
				"*.*", path->array);
	obs_properties_add_text(props, "url", obs_module_text("URL"),
				OBS_TEXT_DEFAULT);

	obs_properties_add_int(props, "width", obs_module_text("Width"), 1,
			       4096, 1);
	obs_properties_add_int(props, "height", obs_module_text("Height"), 1,
			       4096, 1);

	obs_properties_add_bool(props, "reroute_audio",
				obs_module_text("RerouteAudio"));

	obs_property_t *fps_set = obs_properties_add_bool(
		props, "fps_custom", obs_module_text("CustomFrameRate"));
	obs_property_set_modified_callback(fps_set, is_fps_custom);

#ifndef ENABLE_BROWSER_SHARED_TEXTURE
	obs_property_set_enabled(fps_set, false);
#endif

	obs_properties_add_int(props, "fps", obs_module_text("FPS"), 1, 60, 1);

	obs_property_t *p = obs_properties_add_text(
		props, "css", obs_module_text("CSS"), OBS_TEXT_MULTILINE);
	obs_property_text_set_monospace(p, true);
	obs_properties_add_bool(props, "shutdown",
				obs_module_text("ShutdownSourceNotVisible"));
	obs_properties_add_bool(props, "restart_when_active",
				obs_module_text("RefreshBrowserActive"));

	obs_property_t *controlLevel = obs_properties_add_list(
		props, "webpage_control_level",
		obs_module_text("WebpageControlLevel"), OBS_COMBO_TYPE_LIST,
		OBS_COMBO_FORMAT_INT);

	obs_property_list_add_int(
		controlLevel, obs_module_text("WebpageControlLevel.Level.None"),
		(int)ControlLevel::None);
	obs_property_list_add_int(
		controlLevel,
		obs_module_text("WebpageControlLevel.Level.ReadObs"),
		(int)ControlLevel::ReadObs);
	obs_property_list_add_int(
		controlLevel,
		obs_module_text("WebpageControlLevel.Level.ReadUser"),
		(int)ControlLevel::ReadUser);
	obs_property_list_add_int(
		controlLevel,
		obs_module_text("WebpageControlLevel.Level.Basic"),
		(int)ControlLevel::Basic);
	obs_property_list_add_int(
		controlLevel,
		obs_module_text("WebpageControlLevel.Level.Advanced"),
		(int)ControlLevel::Advanced);
	obs_property_list_add_int(
		controlLevel, obs_module_text("WebpageControlLevel.Level.All"),
		(int)ControlLevel::All);

	obs_properties_add_button(
		props, "refreshnocache", obs_module_text("RefreshNoCache"),
		[](obs_properties_t *, obs_property_t *, void *data) {
			static_cast<BrowserSource *>(data)->Refresh();
			return false;
		});
	return props;
}

static void missing_file_callback(void *src, const char *new_path, void *data)
{
	BrowserSource *bs = static_cast<BrowserSource *>(src);

	if (bs) {
		obs_source_t *source = bs->source;
		OBSDataAutoRelease settings = obs_source_get_settings(source);
		obs_data_set_string(settings, "local_file", new_path);
		obs_source_update(source, settings);
	}

	UNUSED_PARAMETER(data);
}

static obs_missing_files_t *browser_source_missingfiles(void *data)
{
	BrowserSource *bs = static_cast<BrowserSource *>(data);
	obs_missing_files_t *files = obs_missing_files_create();

	if (bs) {
		obs_source_t *source = bs->source;
		OBSDataAutoRelease settings = obs_source_get_settings(source);

		bool enabled = obs_data_get_bool(settings, "is_local_file");
		const char *path = obs_data_get_string(settings, "local_file");

		if (enabled && strcmp(path, "") != 0) {
			if (!os_file_exists(path)) {
				obs_missing_file_t *file =
					obs_missing_file_create(
						path, missing_file_callback,
						OBS_MISSING_FILE_SOURCE,
						bs->source, NULL);

				obs_missing_files_add_file(files, file);
			}
		}
	}

	return files;
}

static CefRefPtr<BrowserApp> app;

static void BrowserInit(void)
{
	string path = obs_get_module_binary_path(obs_current_module());
	path = path.substr(0, path.find_last_of('/') + 1);
#if defined(DEF_TOOLSETS) && defined(_WIN32)
	path += "//Lovense-browser-page";
#else
	path += "//obs-browser-page";
#endif

#ifdef _WIN32
	path += ".exe";
	CefMainArgs args;
#else
	/* On non-windows platforms, ie macOS, we'll want to pass thru flags to
	 * CEF */
	struct obs_cmdline_args cmdline_args = obs_get_cmdline_args();
	CefMainArgs args(cmdline_args.argc, cmdline_args.argv);
#endif

	BPtr<char> conf_path = obs_module_config_path("");
	os_mkdir(conf_path);

	CefSettings settings;
	settings.log_severity = LOGSEVERITY_DISABLE;
	BPtr<char> log_path = obs_module_config_path("debug.log");
	BPtr<char> log_path_abs = os_get_abs_path_ptr(log_path);
	CefString(&settings.log_file) = log_path_abs;
	settings.windowless_rendering_enabled = true;
	settings.no_sandbox = true;
	//add wystan
#ifndef DEF_TOOLSETS
	settings.remote_debugging_port = 9090;
#else
	settings.remote_debugging_port = 9089;
#endif

	uint32_t obs_ver = obs_get_version();
	uint32_t obs_maj = obs_ver >> 24;
	uint32_t obs_min = (obs_ver >> 16) & 0xFF;
	uint32_t obs_pat = obs_ver & 0xFFFF;

	/* This allows servers the ability to determine that browser panels and
	 * browser sources are coming from OBS. */
	std::stringstream prod_ver;
	prod_ver << "Chrome/";
	prod_ver << std::to_string(cef_version_info(4)) << "."
		 << std::to_string(cef_version_info(5)) << "."
		 << std::to_string(cef_version_info(6)) << "."
		 << std::to_string(cef_version_info(7));
	prod_ver << " OBS/";
	prod_ver << std::to_string(obs_maj) << "." << std::to_string(obs_min)
		 << "." << std::to_string(obs_pat);

#if CHROME_VERSION_BUILD >= 4472
	CefString(&settings.user_agent_product) = prod_ver.str();
#else
	CefString(&settings.product_version) = prod_ver.str();
#endif

#ifdef ENABLE_BROWSER_QT_LOOP
	settings.external_message_pump = true;
	settings.multi_threaded_message_loop = false;
#endif

#if !defined(_WIN32) && !defined(__APPLE__)
	// Override locale path from OBS binary path to plugin binary path
	string locales = obs_get_module_binary_path(obs_current_module());
	locales = locales.substr(0, locales.find_last_of('/') + 1);
	locales += "locales";
	BPtr<char> abs_locales = os_get_abs_path_ptr(locales.c_str());
	CefString(&settings.locales_dir_path) = abs_locales;
#endif

	std::string obs_locale = obs_get_locale();
	std::string accepted_languages;
	if (obs_locale != "en-US") {
		accepted_languages = obs_locale;
		accepted_languages += ",";
		accepted_languages += "en-US,en";
	} else {
		accepted_languages = "en-US,en";
	}

	BPtr<char> conf_path_abs = os_get_abs_path_ptr(conf_path);
	CefString(&settings.locale) = obs_get_locale();
	CefString(&settings.accept_language_list) = accepted_languages;
	CefString(&settings.cache_path) = conf_path_abs;
#if !defined(__APPLE__) || defined(ENABLE_BROWSER_LEGACY)
	char *abs_path = os_get_abs_path_ptr(path.c_str());
	CefString(&settings.browser_subprocess_path) = abs_path;
	bfree(abs_path);
#endif

	bool tex_sharing_avail = false;

#ifdef ENABLE_BROWSER_SHARED_TEXTURE
	if (hwaccel) {
		obs_enter_graphics();
		hwaccel = tex_sharing_avail = gs_shared_texture_available();
		obs_leave_graphics();
	}
#endif

	app = new BrowserApp(tex_sharing_avail);

#ifdef _WIN32
	CefExecuteProcess(args, app, nullptr);
#endif

#if !defined(_WIN32)
	BackupSignalHandlers();
	CefInitialize(args, settings, app, nullptr);
	RestoreSignalHandlers();
#elif (CHROME_VERSION_BUILD > 3770)
	CefInitialize(args, settings, app, nullptr);
#else
	/* Massive (but amazing) hack to prevent chromium from modifying our
	 * process tokens and permissions, which caused us problems with winrt,
	 * used with window capture.  Note, the structure internally is just
	 * two pointers normally.  If it causes problems with future versions
	 * we'll just switch back to the static library but I doubt we'll need
	 * to. */
	uintptr_t zeroed_memory_lol[32] = {};
	CefInitialize(args, settings, app, zeroed_memory_lol);
#endif

#if !ENABLE_LOCAL_FILE_URL_SCHEME
	/* Register http://absolute/ scheme handler for older
	 * CEF builds which do not support file:// URLs */
	CefRegisterSchemeHandlerFactory("http", "absolute",
					new BrowserSchemeHandlerFactory());
#endif
	os_event_signal(cef_started_event);
}

static void BrowserShutdown(void)
{
#ifdef ENABLE_BROWSER_QT_LOOP
	while (messageObject.ExecuteNextBrowserTask())
		;
	CefDoMessageLoopWork();
#endif
	CefShutdown();
	app = nullptr;
}

#ifndef ENABLE_BROWSER_QT_LOOP

#if 0
#ifdef _WIN32 //lovense
static HANDLE g_init_mutex = nullptr;
#endif
#endif

#if defined(DEF_TOOLSETS) && defined(_WIN32)

namespace __ {

	struct QCef {
		virtual ~QCef() {}

		virtual bool init_browser(void) = 0;
		virtual bool initialized(void) = 0;
		virtual bool wait_for_browser_init(void) = 0;
	};

	static inline void* get_browser_lib()
	{
		// Disable panels on Wayland for now
		bool isWayland = false;
#ifdef ENABLE_WAYLAND
		isWayland = obs_get_nix_platform() == OBS_NIX_PLATFORM_WAYLAND;
#endif
		if (isWayland)
			return nullptr;

		obs_module_t* browserModule = obs_get_module("obs-browser");

		if (!browserModule)
			return nullptr;

		return obs_get_module_lib(browserModule);
	}

	static inline QCef* obs_browser_init_panel(void)
	{
		void* lib = get_browser_lib();
		QCef* (*create_qcef)(void) = nullptr;

		if (!lib)
			return nullptr;

		create_qcef =
			(decltype(create_qcef))os_dlsym(lib, "obs_browser_create_qcef");

		if (!create_qcef)
			return nullptr;

		return create_qcef();
	}

	static bool browser_cef_init()
	{
		auto _cef = __::obs_browser_init_panel();
		if (_cef) {
			_cef->init_browser();			
			_cef->wait_for_browser_init();
			delete _cef;
			_cef = nullptr;
			return true;
		}

		return false;
	}
}
#endif

static void BrowserManagerThread(void)
{
#if defined(DEF_TOOLSETS) && defined(_WIN32)
	os_event_wait(cef_init_event);

	bool ok = __::browser_cef_init();
	blog(LOG_INFO, "[lovense feedback] browser_cef_init: %d", ok);
	if (!ok) {
		if (nullptr != source_helper::find("browser_source"))
		{
			blog(LOG_INFO, "[lovense feedback] exist browser_source");

			auto _cef = __::obs_browser_init_panel();
			if (_cef) {
				bool ok = _cef->init_browser();
				blog(LOG_INFO, "[lovense feedback] wait for browser_source");
				ok = _cef->wait_for_browser_init();
				delete _cef;
				_cef = nullptr;
			}
			else {
				int cnt = 0;
				while (++cnt <= 10) {
					auto strProcessName = L"obs-browser-page.exe";
					auto id = FindProcessId([&](wchar_t path[MAX_PATH], wchar_t* name)->bool {

						auto len = wcslen(strProcessName);
						auto name_len = wcslen(name);
						if (name_len == len && _wcsnicmp(name, strProcessName, len) == 0)
						{
							//L"Stream Master\\LovenseOBS\\obs-plugins\\64bit\\obs-browser-page.exe"
							std::wstring wpath{ path };
							return (wpath.rfind(L"Stream Master") == std::wstring::npos);
						}

						return false;

						});

					if (id != 0) {
						break;
					}
					std::this_thread::sleep_for(std::chrono::seconds(1));
				}
			}
		}
		else {
			blog(LOG_INFO, "[lovense feedback] not exist browser_source. to init");
		}
	}	
#endif

	blog(LOG_INFO, "[lovense feedback] to init cef");
	BrowserInit();
	CefRunMessageLoop();
	BrowserShutdown();
}
#endif

#if 0
static bool proc_cef_initialize()
{
	proc_handler_t* handle = obs_get_proc_handler();
	if (handle)
	{
		bool ini_ok{ false };

		calldata_t data;
		calldata_init(&data);
		calldata_set_ptr(&data, "init", &ini_ok);

		proc_handler_call(handle, "__lovense_cef_initialize", &data);
	
		return ini_ok;
	}

	return false;
}
#endif

extern "C" EXPORT void obs_browser_initialize(void)
{
	if (!os_atomic_set_bool(&manager_initialized, true)) {
#ifdef ENABLE_BROWSER_QT_LOOP
		BrowserInit();
#else
		manager_thread = thread(BrowserManagerThread);
#endif
	}
}

#ifndef DEF_TOOLSETS
void RegisterBrowserSource()
{
	struct obs_source_info info = {};
	info.id = "browser_source";
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_AUDIO |
		OBS_SOURCE_CUSTOM_DRAW | OBS_SOURCE_INTERACTION |
		OBS_SOURCE_DO_NOT_DUPLICATE | OBS_SOURCE_SRGB;
	info.get_properties = browser_source_get_properties;
	info.get_defaults = browser_source_get_defaults;
	info.icon_type = OBS_ICON_TYPE_BROWSER;

	info.get_name = [](void*) { return obs_module_text("BrowserSource"); };
	info.create = [](obs_data_t* settings, obs_source_t* source) -> void* {
		obs_browser_initialize();
		return new BrowserSource(settings, source);
	};
	info.destroy = [](void* data) {
		static_cast<BrowserSource*>(data)->Destroy();
	};
	info.missing_files = browser_source_missingfiles;
	info.update = [](void* data, obs_data_t* settings) {
		static_cast<BrowserSource*>(data)->Update(settings);
	};
	info.get_width = [](void* data) {
		return (uint32_t) static_cast<BrowserSource*>(data)->width;
	};
	info.get_height = [](void* data) {
		return (uint32_t) static_cast<BrowserSource*>(data)->height;
	};
	info.video_tick = [](void* data, float) {
		static_cast<BrowserSource*>(data)->Tick();
	};
	info.video_render = [](void* data, gs_effect_t*) {
		static_cast<BrowserSource*>(data)->Render();
	};
#if CHROME_VERSION_BUILD < 4103
	info.audio_mix = [](void* data, uint64_t* ts_out,
		struct audio_output_data* audio_output,
		size_t channels, size_t sample_rate) {
			return static_cast<BrowserSource*>(data)->AudioMix(
				ts_out, audio_output, channels, sample_rate);
	};
	info.enum_active_sources = [](void* data, obs_source_enum_proc_t cb,
		void* param) {
			static_cast<BrowserSource*>(data)->EnumAudioStreams(cb, param);
	};
#endif
	info.mouse_click = [](void* data, const struct obs_mouse_event* event,
		int32_t type, bool mouse_up,
		uint32_t click_count) {
			static_cast<BrowserSource*>(data)->SendMouseClick(
				event, type, mouse_up, click_count);
	};
	info.mouse_move = [](void* data, const struct obs_mouse_event* event,
		bool mouse_leave) {
			static_cast<BrowserSource*>(data)->SendMouseMove(event,
				mouse_leave);
	};
	info.mouse_wheel = [](void* data, const struct obs_mouse_event* event,
		int x_delta, int y_delta) {
			static_cast<BrowserSource*>(data)->SendMouseWheel(
				event, x_delta, y_delta);
	};
	info.focus = [](void* data, bool focus) {
		static_cast<BrowserSource*>(data)->SendFocus(focus);
	};
	info.key_click = [](void* data, const struct obs_key_event* event,
		bool key_up) {
			static_cast<BrowserSource*>(data)->SendKeyClick(event, key_up);
	};
	info.show = [](void* data) {
		static_cast<BrowserSource*>(data)->SetShowing(true);
	};
	info.hide = [](void* data) {
		static_cast<BrowserSource*>(data)->SetShowing(false);
	};
	info.activate = [](void* data) {
		BrowserSource* bs = static_cast<BrowserSource*>(data);
		if (bs->restart)
			bs->Refresh();
		bs->SetActive(true);
	};
	info.deactivate = [](void* data) {
		static_cast<BrowserSource*>(data)->SetActive(false);
	};

	obs_register_source(&info);
}
#endif // !DEF_TOOLSETS

void RegisterLovenseVideoFeedback()
{
	struct obs_source_info info = {};
#ifdef DEF_TOOLSETS
    info.id = "Lovese_browser_source";
#else
	info.id = "lovense_video_freedback";
#endif
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_AUDIO |
			    OBS_SOURCE_CUSTOM_DRAW | /*OBS_SOURCE_INTERACTION |*/
			    OBS_SOURCE_DO_NOT_DUPLICATE | OBS_SOURCE_SRGB;

	info.get_properties = lovense_source_get_properties;
	info.get_defaults   = lovense_browser_source_get_defaults;
	
	info.icon_type = OBS_ICON_TYPE_BROWSER;

	info.get_name = [](void *) { return obs_module_text("Lovense Video Feedback"); };
	info.create = [](obs_data_t *settings, obs_source_t *source) -> void * {
		obs_browser_initialize();
		obs_source_set_monitoring_type(source, OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT);
		return new BrowserSource(settings, source);
	};
	info.destroy = [](void *data) {
		static_cast<BrowserSource *>(data)->Destroy();
	};
	info.missing_files = browser_source_missingfiles;
	info.update = [](void *data, obs_data_t *settings) {
		static_cast<BrowserSource *>(data)->Update(settings, true);
	};
	info.get_width = [](void *data) {
		return (uint32_t) static_cast<BrowserSource *>(data)->width;
	};
	info.get_height = [](void *data) {
		return (uint32_t) static_cast<BrowserSource *>(data)->height;
	};
	info.video_tick = [](void *data, float) {
		static_cast<BrowserSource *>(data)->Tick();
	};
	info.video_render = [](void *data, gs_effect_t *) {
		static_cast<BrowserSource *>(data)->Render();
	};
#if CHROME_VERSION_BUILD < 4103
	info.audio_mix = [](void *data, uint64_t *ts_out,
			    struct audio_output_data *audio_output,
			    size_t channels, size_t sample_rate) {
		return static_cast<BrowserSource *>(data)->AudioMix(
			ts_out, audio_output, channels, sample_rate);
	};
	info.enum_active_sources = [](void *data, obs_source_enum_proc_t cb,
				      void *param) {
		static_cast<BrowserSource *>(data)->EnumAudioStreams(cb, param);
	};
#endif
	info.mouse_click = [](void *data, const struct obs_mouse_event *event,
			      int32_t type, bool mouse_up,
			      uint32_t click_count) {
		static_cast<BrowserSource *>(data)->SendMouseClick(
			event, type, mouse_up, click_count);
	};
	info.mouse_move = [](void *data, const struct obs_mouse_event *event,
			     bool mouse_leave) {
		static_cast<BrowserSource *>(data)->SendMouseMove(event,
								  mouse_leave);
	};
	info.mouse_wheel = [](void *data, const struct obs_mouse_event *event,
			      int x_delta, int y_delta) {
		static_cast<BrowserSource *>(data)->SendMouseWheel(
			event, x_delta, y_delta);
	};
	info.focus = [](void *data, bool focus) {
		static_cast<BrowserSource *>(data)->SendFocus(focus);
	};
	info.key_click = [](void *data, const struct obs_key_event *event,
			    bool key_up) {
		static_cast<BrowserSource *>(data)->SendKeyClick(event, key_up);
	};
	info.show = [](void *data) {
		static_cast<BrowserSource *>(data)->SetShowing(true);
	};
	info.hide = [](void *data) {
		static_cast<BrowserSource *>(data)->SetShowing(false);
	};
	info.activate = [](void *data) {
		BrowserSource *bs = static_cast<BrowserSource *>(data);
		if (bs->restart)
			bs->Refresh();
		bs->SetActive(true);
	};
	info.deactivate = [](void *data) {
		static_cast<BrowserSource *>(data)->SetActive(false);
	};

	obs_register_source(&info);
}

#ifdef DEF_TOOLSETS
static bool g_is_find_browser_source = false;
bool func_scene_enum_call(obs_scene_t* scene, obs_sceneitem_t* item, void*param)
{
    bool ret = true;
    obs_source_t *source = obs_sceneitem_get_source(item);
    if (source)
    {
        const char *name =  obs_source_get_id(source);
        if (name)
        {
            if (strcmp(name, "Lovese_browser_source") == 0)
            {
                g_is_find_browser_source = true;
                return false;
            }
        }
    }
    return ret;
}
#endif

extern void DispatchJSEvent(std::string eventName, std::string jsonString,
			    BrowserSource *browser = nullptr);


#if 0 //This logic has been moved to lovense-relay-server plugin
static void reset_current_scene_feedback_srgb()
{
	OBSSourceAutoRelease source = obs_frontend_get_current_scene();
	OBSScene scene = obs_scene_from_source(source);
	if (scene) {
		obs_scene_enum_items(scene, [](obs_scene_t* scene, obs_sceneitem_t* item, void* param)->bool {

			OBSSource item_source = obs_sceneitem_get_source(item);
			auto id = obs_source_get_id(item_source);
			if (!id || strcmp(id, lovense::FEEDBACK_SOURCE_ID) != 0) {
				return true;
			}

			auto blending_method = obs_sceneitem_get_blending_method(item);
			if (blending_method != OBS_BLEND_METHOD_SRGB_OFF)
			{
				obs_sceneitem_set_blending_method(item, OBS_BLEND_METHOD_SRGB_OFF);
			}

			return true;

		}, nullptr);
	}
}
#endif

static void handle_obs_frontend_event(enum obs_frontend_event event, void * data)
{
	switch (event) {            
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
	{
#ifdef DEF_TOOLSETS

    #ifdef _WIN32
		os_event_signal(cef_init_event);
	#else
		char path[512] = { 0 };
		if (os_get_config_path(path, sizeof(path), "obs-studio") <= 0)
		{
			blog(LOG_ERROR, "[Lovese] os_get_config_path locale dist path is  fail ");
			return;
		}

		if (shouldAddSourceToScene(path))
		{
			//Release
			obs_source_t * current_sense_source = obs_frontend_get_current_scene();
			obs_scene_t *current_scene = obs_scene_from_source(current_sense_source);
			obs_scene_enum_items(current_scene, func_scene_enum_call, nullptr);
			if (current_scene && !g_is_find_browser_source)
			{
				obs_data_t * setting = obs_data_create();
				browser_source_get_defaults(setting);
				obs_source_t * brower_source = obs_source_create("Lovese_browser_source", "Lovense Video Feedback", setting, NULL);
				if (brower_source)
				{
					obs_scene_add(current_scene, brower_source);
					obs_source_release(brower_source);
					obs_data_release(setting);
				}
			}
			obs_source_release(current_sense_source);
			setupSourceHaveAdded(path);
		}
	#endif
#endif
	}break;
	case OBS_FRONTEND_EVENT_STREAMING_STARTING:
		DispatchJSEvent("obsStreamingStarting", "");
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STARTED:
		DispatchJSEvent("obsStreamingStarted", "");
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STOPPING:
		DispatchJSEvent("obsStreamingStopping", "");
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
		DispatchJSEvent("obsStreamingStopped", "");
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STARTING:
		DispatchJSEvent("obsRecordingStarting", "");
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STARTED:
		DispatchJSEvent("obsRecordingStarted", "");
		break;
	case OBS_FRONTEND_EVENT_RECORDING_PAUSED:
		DispatchJSEvent("obsRecordingPaused", "");
		break;
	case OBS_FRONTEND_EVENT_RECORDING_UNPAUSED:
		DispatchJSEvent("obsRecordingUnpaused", "");
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STOPPING:
		DispatchJSEvent("obsRecordingStopping", "");
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
		DispatchJSEvent("obsRecordingStopped", "");
		break;
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTING:
		DispatchJSEvent("obsReplaybufferStarting", "");
		break;
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTED:
		DispatchJSEvent("obsReplaybufferStarted", "");
		break;
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_SAVED:
		DispatchJSEvent("obsReplaybufferSaved", "");
		break;
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPING:
		DispatchJSEvent("obsReplaybufferStopping", "");
		break;
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED:
		DispatchJSEvent("obsReplaybufferStopped", "");
		break;
	case OBS_FRONTEND_EVENT_VIRTUALCAM_STARTED:
		DispatchJSEvent("obsVirtualcamStarted", "");
		break;
	case OBS_FRONTEND_EVENT_VIRTUALCAM_STOPPED:
		DispatchJSEvent("obsVirtualcamStopped", "");
		break;
	case OBS_FRONTEND_EVENT_SCENE_CHANGED: {

		//reset_current_scene_feedback_srgb();

		OBSSourceAutoRelease source = obs_frontend_get_current_scene();

		if (!source)
			break;

		const char *name = obs_source_get_name(source);
		if (!name)
			break;

		Json json = Json::object{
			{"name", name},
			{"width", (int)obs_source_get_width(source)},
			{"height", (int)obs_source_get_height(source)}};

		DispatchJSEvent("obsSceneChanged", json.dump());
		break;
	}
    case OBS_FRONTEND_EVENT_EXIT:{
        //TODO: by cita remove proc
        //_procDestory();
        DispatchJSEvent("obsExit", "");
    }
    break;
	default:
		break;
	}
#if defined(DEF_TOOLSETS) && defined(_WIN32)
	toolset::on_obs_frontend_event(event, data);
#endif

}

#ifdef _WIN32
static inline void EnumAdapterCount()
{
	ComPtr<IDXGIFactory1> factory;
	ComPtr<IDXGIAdapter1> adapter;
	HRESULT hr;
	UINT i = 0;

	hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void **)&factory);
	if (FAILED(hr))
		return;

	while (factory->EnumAdapters1(i++, &adapter) == S_OK) {
		DXGI_ADAPTER_DESC desc;

		hr = adapter->GetDesc(&desc);
		if (FAILED(hr))
			continue;

		if (i == 1)
			deviceId = desc.Description;

		/* ignore Microsoft's 'basic' renderer' */
		if (desc.VendorId == 0x1414 && desc.DeviceId == 0x8c)
			continue;

		adapterCount++;
	}
}
#endif

#ifdef ENABLE_BROWSER_SHARED_TEXTURE
#ifdef _WIN32
static const wchar_t *blacklisted_devices[] = {
	L"Intel", L"Microsoft", L"Radeon HD 8850M", L"Radeon HD 7660", nullptr};

static inline bool is_intel(const std::wstring &str)
{
	return wstrstri(str.c_str(), L"Intel") != 0;
}

static void check_hwaccel_support(void)
{
	/* do not use hardware acceleration if a blacklisted device is the
	 * default and on 2 or more adapters */
	const wchar_t **device = blacklisted_devices;

	if (adapterCount >= 2 || !is_intel(deviceId)) {
		while (*device) {
			if (!!wstrstri(deviceId.c_str(), *device)) {
				hwaccel = false;
				blog(LOG_INFO, "[obs-browser]: "
					       "Blacklisted device "
					       "detected, "
					       "disabling browser "
					       "source hardware "
					       "acceleration.");
				break;
			}
			device++;
		}
	}
}
#elif defined(__APPLE__)
extern bool atLeast10_15(void);

static void check_hwaccel_support(void)
{
	if (!atLeast10_15()) {
		blog(LOG_INFO,
		     "[obs-browser]: OS version older than 10.15 Disabling hwaccel");
		hwaccel = false;
	}
	return;
}
#else
static void check_hwaccel_support(void)
{
	return;
}
#endif
#endif


static void handle_obs_save_event(obs_data_t* save_data, bool saving, void* private_data)
{
	if (!saving) return ;

	if (obs_video_active()) {
		return;
	}

	config_t* config = obs_frontend_get_profile_config();
	if (!config) {
		return;
	}

	struct Context
	{
		int  width{ 0 };
		int  height{ 0 };
		bool update{ false };
	};
	Context context;
	context.width  = config_get_uint(config, "Video", "BaseCX");
	context.height = config_get_uint(config, "Video", "BaseCY");

	struct obs_frontend_source_list scenes = {};
	obs_frontend_get_scenes(&scenes);

	for (size_t i = 0; i < scenes.sources.num; i++) {
		OBSSource source = scenes.sources.array[i];
		OBSScene  scene = obs_scene_from_source(source);

		obs_scene_enum_items(scene, [](obs_scene_t*, obs_sceneitem_t* item, void* param)->bool {

			OBSSource source_item = obs_sceneitem_get_source(item);
			if (!source_item) {
				return true;
			}

			auto source_item_id = obs_source_get_id(source_item);
			if ((!source_item_id) || (0 != strcmp(source_item_id, lovense::FEEDBACK_SOURCE_ID))) {
				return true;
			}

			Context* context_ptr       = reinterpret_cast<Context*>(param);
			OBSDataAutoRelease setting = obs_source_get_settings(source_item);

			if (setting) {
				int lWidth  = obs_data_get_int(setting, "width");
				int lHeight = obs_data_get_int(setting, "height");

				if (context_ptr->width != lWidth || context_ptr->height != lHeight)
				{
					obs_data_set_int(setting, "width",  context_ptr->width);
					obs_data_set_int(setting, "height", context_ptr->height);

					obs_source_update(source_item, setting);

					context_ptr->update = true;
				}
			}

			if (context_ptr->update) {

				obs_video_info ovi;
				obs_get_video_info(&ovi);

				obs_transform_info itemInfo;
				vec2_set(&itemInfo.pos, 0.0f, 0.0f);
				vec2_set(&itemInfo.scale, 1.0f, 1.0f);
				itemInfo.alignment = OBS_ALIGN_LEFT | OBS_ALIGN_TOP;
				itemInfo.rot = 0.0f;

				vec2_set(&itemInfo.bounds, float(ovi.base_width), float(ovi.base_height));

				itemInfo.bounds_type      = OBS_BOUNDS_SCALE_INNER;
				itemInfo.bounds_alignment = OBS_ALIGN_CENTER;

				obs_sceneitem_set_info(item, &itemInfo);
			}

			return true;

		}, &context);
	}
	obs_frontend_source_list_free(&scenes);
}

bool obs_module_load(void)
{
#if defined(DEF_TOOLSETS) && defined(_WIN32)
	blog(LOG_INFO, "[lovense]: Version %s", OBS_BROWSER_VERSION_STRING);

	if (!toolset::initialize()) {
		return false;
	}
#endif


#if defined(__APPLE__) && defined(_DEBUG)
    //return false;//TODO: by cita not load obs-browser for debug
#endif
#ifdef ENABLE_BROWSER_QT_LOOP
	qRegisterMetaType<MessageTask>("MessageTask");
#endif

	os_event_init(&cef_started_event, OS_EVENT_TYPE_MANUAL);
#if defined(DEF_TOOLSETS) && defined(_WIN32)
	os_event_init(&cef_init_event, OS_EVENT_TYPE_MANUAL);
#endif

#ifdef _WIN32
	/* CefEnableHighDPISupport doesn't do anything on OS other than Windows. Would also crash macOS at this point as CEF is not directly linked */
	CefEnableHighDPISupport();
	EnumAdapterCount();
#else
#if defined(__APPLE__) && !defined(ENABLE_BROWSER_LEGACY)
	/* Load CEF at runtime as required on macOS */
	CefScopedLibraryLoader library_loader;
	if (!library_loader.LoadInMain())
		return false;
#endif
#endif

	blog(LOG_INFO, "[obs-browser]: Version %s", OBS_BROWSER_VERSION_STRING);
	blog(LOG_INFO,
	     "[obs-browser]: CEF Version %i.%i.%i.%i (runtime), %s (compiled)",
	     cef_version_info(4), cef_version_info(5), cef_version_info(6),
	     cef_version_info(7), CEF_VERSION);

	lovense::bs::setup_proc_hander();

#ifndef DEF_TOOLSETS
	RegisterBrowserSource();
#endif
	RegisterLovenseVideoFeedback();
	obs_frontend_add_event_callback(handle_obs_frontend_event, nullptr);
	obs_frontend_add_save_callback(handle_obs_save_event, nullptr);

#ifdef ENABLE_BROWSER_SHARED_TEXTURE
	OBSDataAutoRelease private_data = obs_get_private_data();
	hwaccel = obs_data_get_bool(private_data, "BrowserHWAccel");

	if (hwaccel) {
		check_hwaccel_support();
	}
#endif

#if defined(__APPLE__) && CHROME_VERSION_BUILD < 4183
	// Make sure CEF malloc hijacking happens early in the process
	obs_browser_initialize();
#endif

	return true;
}

void obs_module_post_load(void)
{
	auto vendor = obs_websocket_register_vendor("obs-browser");
	if (!vendor)
		return;

	auto emit_event_request_cb = [](obs_data_t *request_data, obs_data_t *,
					void *) {
		const char *event_name =
			obs_data_get_string(request_data, "event_name");
		if (!event_name)
			return;

		OBSDataAutoRelease event_data =
			obs_data_get_obj(request_data, "event_data");
		const char *event_data_string =
			event_data ? obs_data_get_json(event_data) : "{}";

		DispatchJSEvent(event_name, event_data_string, nullptr);
	};

	if (!obs_websocket_vendor_register_request(
		    vendor, "emit_event", emit_event_request_cb, nullptr))
		blog(LOG_WARNING,
		     "[obs-browser]: Failed to register obs-websocket request emit_event");
}

void obs_module_unload(void)
{
	blog(LOG_INFO, "lovense::feedback unload");

	obs_frontend_remove_save_callback(handle_obs_save_event, nullptr);
	obs_frontend_remove_event_callback(handle_obs_frontend_event, nullptr);

#ifdef ENABLE_BROWSER_QT_LOOP
	BrowserShutdown();
#else
#if 0
#ifdef _WIN32 //lovense
	if (g_init_mutex) {
		CloseHandle(g_init_mutex);
		g_init_mutex = nullptr;
	}
#endif
#endif

	if (manager_thread.joinable()) {
		while (!QueueCEFTask([]() { CefQuitMessageLoop(); }))
			os_sleep_ms(5);

		manager_thread.join();
	}
#endif

	os_event_destroy(cef_started_event);

#if defined(DEF_TOOLSETS) && defined(_WIN32)
	os_event_destroy(cef_init_event);
#endif
}
