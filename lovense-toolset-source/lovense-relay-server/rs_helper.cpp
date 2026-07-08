#include <random>
#include <mutex>

#include "obs.hpp"
#include "util/platform.h"
#include "util/util.hpp"
#include "util/dstr.h"
#include "util/dstr.hpp"
#include "obs-frontend-api.h"

#include "lovense_error.hpp"
#include "lovense_util_helper.hpp"
#include "lovense_obs_proc.hpp"
#include "lovense_obs_source.hpp"
#include "lovense_obs_scene.hpp"
#include "lovense_obs_properties.hpp"
#include "lovense_obs_helper.hpp"

#include "rs_common.hpp"
#include "rs_helper.hpp"

#if (defined(DEF_TOOLSETS)&& defined(_WIN32))
#include <shlobj_core.h>
#endif

#if defined(__APPLE__) && defined(DEF_TOOLSETS)
extern std::string UntilGetDistVersion();
extern std::string UntilGetToolsetVersion();
#endif

namespace lovense
{
	namespace rs
	{
		std::string get_app_token()
		{
			static std::string token;
			if (!token.empty())
			{
				return token;
			}

			token = GetComputerPCID();
			if (token == "000000000")
			{
				token = "";

				std::random_device rd;					
				std::default_random_engine random(rd());

				for (int i = 0; i < 32; i++) {
					char tmp = random() % 36;	
					if (tmp < 10) {			
						tmp += '0';
					}
					else {				
						tmp -= 10;
						tmp += 'a';
					}
					token += tmp;
				}			
			}
			
#ifdef DEF_TOOLSETS
			std::string obs_version;
			const char* c_version = obs_get_version_string();
			if (c_version) {
				obs_version = c_version;
			}
			token = "obs_" + obs_version + "_" + token;
#else
			std::string version = GetStreamMasterVersion();
			token = "stream_master_" + version + "_" + token;
#endif
			//
			token = lovense::Md5String(token);
			return token;
		}

#if (defined(DEF_TOOLSETS)&& defined(_WIN32))
		bool get_obs_plugin_config_path(std::filesystem::path& path)
		{
			try {
				wchar_t path_utf16[MAX_PATH];
				if (S_OK != SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, path_utf16)) {
					return false;
				}

				path = std::filesystem::path(path_utf16) / "obs-studio" / "PluginVersion.ini";
				return true;
			}
			catch (...) {}

			path.clear();
			return false;
		}
		
		bool get_obs_program_data_path(std::filesystem::path& path)
		{
			try {
				wchar_t path_utf16[MAX_PATH];
				if (S_OK != SHGetFolderPathW(NULL, CSIDL_COMMON_APPDATA, NULL, SHGFP_TYPE_CURRENT, path_utf16)) {
					return false;
				}

				path = std::filesystem::path(path_utf16) / "obs-studio";
				return true;
			}
			catch (...) {}

			path.clear();
			return false;
		}

		void update_version_info()
		{
			//Synchronize current toolset version to user directory version
			//Multiple users may update toolset when other users are present
			//dist synchronization is implemented in video-feedback
			try {
				std::filesystem::path data_path;
				if (!get_obs_program_data_path(data_path)) {
					return;
				}

				data_path = data_path / "lovense-video-freedback" / "PluginVersion.ini";

				std::filesystem::path config_path;
				if (!get_obs_plugin_config_path(config_path)) {
					return;
				}

				wchar_t last_version[MAX_PATH] = { 0 };
				GetPrivateProfileString(L"OBS_PLUGIN", L"version", L"", last_version, MAX_PATH, data_path.wstring().c_str());
				if (wcslen(last_version) == 0) {
					return;
				}

				wchar_t config_version[MAX_PATH] = { 0 };
				GetPrivateProfileString(L"OBS_PLUGIN", L"version", L"", config_version, MAX_PATH, config_path.wstring().c_str());

				if (_wcsicmp(last_version, config_version) == 0) {
					return;
				}

				WritePrivateProfileString(L"OBS_PLUGIN", L"version", last_version, config_path.wstring().c_str());
			}
			catch (...) {
			}
		}
#endif

		bool get_version(std::string& dist_version, std::string& toolset_version, bool ignore_cache)
		{
			static std::mutex  __mutext;
			static std::string __dist_version;
			static std::string __toolset_version;

			std::lock_guard<std::mutex> lk{ __mutext };

			if ((!__dist_version.empty()) && (!ignore_cache)) {
				dist_version    = __dist_version;
				toolset_version = __toolset_version.empty() ? __dist_version : __toolset_version;
				return true;
			}		

#if defined(DEF_TOOLSETS)

#ifdef __APPLE__
			__dist_version    = UntilGetDistVersion();
			__toolset_version = UntilGetToolsetVersion();		
#else
			try {
				//Synchronize the latest toolset version information
				update_version_info();

				std::filesystem::path config_path;
				if (!get_obs_plugin_config_path(config_path)) {
					return false;
				}

				if (!std::filesystem::exists(config_path)) {
					return false;
				}

				wchar_t dist_version[MAX_PATH] = { 0 };
				GetPrivateProfileString(L"DIST", L"version", L"", dist_version, MAX_PATH, config_path.wstring().c_str());
				if (wcslen(dist_version) > 0) {
					__dist_version = string_util::wstring2string(dist_version);
				}

				wchar_t toolset_version[MAX_PATH] = { 0 };
				GetPrivateProfileString(L"OBS_PLUGIN", L"version", L"", toolset_version, MAX_PATH, config_path.wstring().c_str());
				if (wcslen(toolset_version) > 0) {
					__toolset_version = string_util::wstring2string(toolset_version);
				}
			}
			catch (...) {}
#endif

#else
			__dist_version    = lovense::GetDistHtmlVersion();
			__toolset_version = __dist_version;
#endif
			dist_version    = __dist_version;
			toolset_version = __toolset_version;
			return true;
		}

#if 0
		std::string GetDistHtmlVersion()
		{
#if defined(DEF_TOOLSETS)
            
			static std::string plugin_version;
			if (!plugin_version.empty())
				return plugin_version;
#ifdef __APPLE__
            
            plugin_version = UntilGetDistVersion();
            return plugin_version;
#else
            
			char szPath[MAX_PATH] = { 0 };
			if (os_get_config_path(szPath, MAX_PATH, "obs-studio/PluginVersion.ini") > 0)
			{
				ConfigFile pluginConfig;
				int errorcode = pluginConfig.Open(szPath, CONFIG_OPEN_ALWAYS);
				if (errorcode != CONFIG_SUCCESS) {
					return "";
				}
				plugin_version = config_get_string(pluginConfig, "DIST", "version");
				pluginConfig.Close();

			}
			return plugin_version;
#endif
#else
			return lovense::GetDistHtmlVersion();
#endif
		}
#endif

		void report_obs_info()
		{
			const char* version = obs_get_version_string();
			if (version)
			{
				struct obs_frontend_source_list scenes = {};
				obs_frontend_get_scenes(&scenes);

				int secene_cnt = scenes.sources.num;

				int sources_cnt{ 0 };
				for (size_t i = 0; i < secene_cnt; i++) {
					obs_source_t* source = scenes.sources.array[i];

					OBSScene scene = obs_scene_from_source(source);
					if (scene) {
						obs_scene_enum_items(scene, [](obs_scene_t* scene, obs_sceneitem_t* item, void* param)->bool {
							int* cnt = reinterpret_cast<int*>(param);
							*cnt += 1;
							return true;
							}, (void*)(&sources_cnt));
					}
				}
				obs_frontend_source_list_free(&scenes);

				proc::report_log(
					"Server", "W0101", "obs version: %s scenes: %d source: %d", version, secene_cnt, sources_cnt);
			}
		}

		static const std::vector<std::string>  __lovense_plugins = {
			"lovense",
			"obs-multi-rtmp",
			"Lovense_Update",
			"lovese_plugin_x64",
#ifndef DEF_TOOLSETS
			"logi_obs_plugin_x64"
#endif // ! _DEF_TOOLSETS			
		};

		void report_extra_modules()
		{
			//---------------------------------------
			// lovense plugins
			//"obs-multi-rtmp.dll",
			//"lovense-video-freedback.dll",
			//"lovense-Ai-camera.dll",
			//"obs-multi-rtmp-qt6.dll",
			//"lovense-webcam.dll",
			std::vector<obs_module_t*> all_ext_modules;
			obs_enum_modules([](void* param, obs_module_t* mod) {

				auto file = obs_get_module_file_name(mod);
				if (file == nullptr) {
					return;
				}

				std::string file_str(file);
				if (is_obs_system_module(mod))
				{
					return;
				}

				auto name = obs_get_module_name(mod);
				if (name != nullptr) {
					std::string auth_str = std::string(name);
					if (auth_str.rfind("lovense", 0) == 0) {
						return;
					}
				}

				auto autho = obs_get_module_author(mod);
				if (autho) {
					std::string auth_str = std::string(autho);
					if (auth_str == "lovense") {
						return;
					}
				}

				for (const auto& plugin : __lovense_plugins)
				{
					if (file_str.rfind(plugin, 0) == 0) {
						return;
					}
				}

				std::vector<obs_module_t*>* module_result_ptr = (std::vector<obs_module_t*>*)param;
				module_result_ptr->push_back(mod);


				}, &all_ext_modules);

			if (all_ext_modules.empty()) {
				return;
			}

			std::string logs = "extra:";
			for (auto it : all_ext_modules)
			{
				auto file = obs_get_module_file_name(it);				
				logs += file;
				logs += ",";
			}

			proc::call_BiLog("system", "S0003", logs.c_str());
		}

		int select_camera(std::string camera_name)
		{
			if (camera_name.empty())
				return (int)error::Code::lvs_ParamsBad;

			{
				CameraDevice device(camera_name);
				if (device && device.is_virtual_device())
				{
					return (int)error::Code::lvs_NotSupportVirtual;
				}
			}

			blog(LOG_INFO, "select camera: %s", camera_name.c_str());

			CameraSource current_source;
			auto source = current_source.FindSource();
			if (!source) {
				return (int)error::Code::lvs_SouceNull;
			}

			CameraProperties obs_properties(source);
			if (!obs_properties) {
				return (int)error::Code::lvs_PropertiesNull;
			}

			CameraDevice device;
			if (!obs_properties.get_device(camera_name, device))
			{
				return (int)error::Code::lvs_NonCameraDevice;
			}

			if (device.is_virtual_device())
			{
				return (int)error::Code::lvs_NotSupportVirtual;
			}

			auto ratio_strategy = obs_properties.get_resolution_strategy();
			lovense::ObsCameraSettings settings(source);
			if (device.device_id() != settings.get_device_id())
			{
				settings.set_device_id(device.device_id());
				settings.update();

				lovense::DshowSource source_obj(source);
                source_obj.AdaptResolutionRadio();
			}

			return 0;
		}


		static bool process_profilter_inited_text(void* context, profiler_snapshot_entry_t* entry)
		{
			auto min_time = profiler_snapshot_entry_min_time(entry);
			auto max_time = profiler_snapshot_entry_max_time(entry);

			//More than 10ms
			if (min_time == max_time && min_time >= 10000) {

				auto name = profiler_snapshot_entry_name(entry);
				auto text_ptr = reinterpret_cast<std::string*>(context);

				*text_ptr += string_util::format("%s:%lld;", name, min_time);;

				profiler_snapshot_enumerate_children(entry, [](void* data,
					profiler_snapshot_entry_t* child_entry) {

						return process_profilter_inited_text(data, child_entry);
					}, context);

				return true;
			}

			return true;
		}

		static bool check_need_report_profilter(void* context, profiler_snapshot_entry_t* entry)
		{
			auto min_time = profiler_snapshot_entry_min_time(entry);
			auto max_time = profiler_snapshot_entry_max_time(entry);

			if (min_time == max_time) {

				auto is_report_ptr = reinterpret_cast<bool*>(context);
				//Report related time consumption information if loading exceeds 8s
				if (min_time > 8000000)
				{
					*is_report_ptr = true;
				}

				return false;
			}

			return true;
		}

		void report_profilter_inited()
		{
			static auto SnapshotRelease = [](profiler_snapshot_t* snap) {
				profile_snapshot_free(snap);
			};

			using ProfilerSnapshot =
				std::unique_ptr<profiler_snapshot_t, decltype(SnapshotRelease)>;

			ProfilerSnapshot snapshort = ProfilerSnapshot{ profile_snapshot_create(), SnapshotRelease };

			bool is_report{ false };
			profiler_snapshot_enumerate_roots(snapshort.get(), check_need_report_profilter, &is_report);

			if (is_report) {
				std::string text;
				profiler_snapshot_enumerate_roots(snapshort.get(), process_profilter_inited_text, &text);
				if (!text.empty()) {
					proc::report_log("start", "R0101", text.c_str());
					//rs_info("R0101 text:%s", text.c_str());
				}
			}
			
		}
	}
}
