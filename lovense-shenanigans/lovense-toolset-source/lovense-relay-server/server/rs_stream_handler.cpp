#include <filesystem>

#include <obs-frontend-api.h>
#include <util/config-file.h>
#include <util/platform.h>

#include "lovense_error.hpp"
#include "lovense_string_helper.hpp"

#include "lovense_websocket_common.hpp"

#include "rs_common.hpp"
#include "rs_stream_handler.hpp"

#include "LovenseUntil.h"

#ifdef _WIN32
#include <Windows.h>
#include <ShlObj.h>
#endif

namespace lovense::rs::stream
{
#ifdef _WIN32	
	static std::atomic<HWND> gFileBrowserHwnd;
	static std::future<void> gRecordExplorerFutrue;
	static std::atomic<bool> gShutdown = false;
#endif

	static bool set_record_config(const char* format, const char* rec_path);
	static bool restore_record_config();
	static std::string get_record_dir();

	static void shutdown();
	static void notify_record_settings();
	static void notify_record_status(int record_status);

	void pack_stream_module(json11::Json::object& root)
	{
		bool is_active = obs_frontend_recording_active();
		bool is_paused = obs_frontend_recording_paused();

		json11::Json::object data;

		if (is_active || is_paused) {
			data["record_status"] = RecordStatus::RECORDING_STARTED;
		}
		else {
			data["record_status"] = RecordStatus::RECORDING_STOPPED;
		}

		data["record_dir"] = get_record_dir();

		root["stream_v"] = 1;
		root["stream"]   = data;
	}

	void handle_event(enum obs_frontend_event event, void* private_data)
	{
		switch (event)
		{
		case OBS_FRONTEND_EVENT_RECORDING_STARTING:
		{
			notify_record_status(RecordStatus::RECORDING_STARTING);
			break;
		}
		case OBS_FRONTEND_EVENT_RECORDING_STARTED:
		{
			notify_record_status(RecordStatus::RECORDING_STARTED);
			break;
		}
		case OBS_FRONTEND_EVENT_RECORDING_STOPPING:
		{
			notify_record_status(RecordStatus::RECORDING_STOPPING);
			break;
		}
		case OBS_FRONTEND_EVENT_RECORDING_PAUSED:
		{
		}
		case OBS_FRONTEND_EVENT_RECORDING_UNPAUSED:
		{
		}
		case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
		{
			restore_record_config();
			notify_record_status(RecordStatus::RECORDING_STOPPED);
			break;
		}
#if (LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(28, 0, 0))
		case OBS_FRONTEND_EVENT_PROFILE_CHANGING:
		{
			notify_record_settings();
			break;
		}
#endif
		case OBS_FRONTEND_EVENT_EXIT:
		{
			stream::shutdown();
			break;
		}
		default:
			break;
		}
	}

	static std::string GetDefaultVideoSavePath()
	{
#ifdef _WIN32		
		wchar_t path_utf16[MAX_PATH];
		char path_utf8[MAX_PATH] = {};

		SHGetFolderPathW(NULL, CSIDL_MYVIDEO, NULL, SHGFP_TYPE_CURRENT,
			path_utf16);

		os_wcs_to_utf8(path_utf16, wcslen(path_utf16), path_utf8, MAX_PATH);
		return std::string(path_utf8);
#else
		//assert(false, "to Implement");
        char* home = getenv("HOME");
        std::filesystem::path ph = (home != nullptr) ? home : "/";
        ph.append("Movies");
		return ph.string();
#endif
	}

	static bool set_record_config(const char* format, const char* rec_path)
	{
		if (!format || !rec_path) {
			return false;
		}

		auto config = obs_frontend_get_profile_config();
		if (!config) {
			rs_warn("set_record_config get config failed");
			return false;
		}

		auto mode = config_get_string(config, "Output", "Mode");
		if (!mode) {
			rs_warn("set_record_config get mode failed");
			return false;
		}

		const char* mode_key{ nullptr };
		const char* path_key{ nullptr };
		if (strcmp(mode, "Simple") == 0) {
			mode_key = "SimpleOutput";
			path_key = "FilePath";
		}
		else if (strcmp(mode, "Advanced") == 0) {
			mode_key = "AdvOut";
			path_key = "RecFilePath";
		}
		else {
			rs_warn("set_record_config unknown mode: %s", mode);
			return false;
		}

		const char* REC_FORMAT_NAME = nullptr;
		if (obs_get_version() <= MAKE_SEMANTIC_VERSION(29, 1, 0))
		{
			REC_FORMAT_NAME = "RecFormat";
		}
		else {
			REC_FORMAT_NAME = "RecFormat2";
		}
		
		const char* fmt = config_get_string(config, mode_key, REC_FORMAT_NAME);
		if ((!fmt) || (strcmp(fmt, format) != 0)) {
			if (fmt) {
				config_set_string(config, "lovense", "RecFormatBackup", fmt);
			}
			config_set_string(config, mode_key, REC_FORMAT_NAME, format);
		}

		bool auto_remux = config_get_bool(config, "Video", "AutoRemux");
		if (auto_remux) {
			config_set_bool(config, "Video",   "AutoRemux", false);
			config_set_bool(config, "lovense", "AutoRemuxBackup", true);
		}

		return true;
	}

	static bool restore_record_config()
	{
		auto config = obs_frontend_get_profile_config();
		if (!config) {
			rs_warn("restore_record_config get config failed");
			return false;
		}

		auto mode = config_get_string(config, "Output", "Mode");
		if (!mode) {
			rs_warn("restore_record_config get Mode failed");
			return false;
		}

		const char* mode_key{ nullptr };
		const char* path_key{ nullptr };
		if (strcmp(mode, "Simple") == 0) {
			mode_key = "SimpleOutput";
			path_key = "FilePath";
		}
		else if (strcmp(mode, "Advanced") == 0) {
			mode_key = "AdvOut";
			path_key = "RecFilePath";
		}
		else {
			rs_warn("restore_record_config unknown mode: %s", mode);
			return false;
		}

		//record format
		{
			const char* REC_FORMAT_NAME = nullptr;
			if (obs_get_version() <= MAKE_SEMANTIC_VERSION(29, 1, 0))
			{
				REC_FORMAT_NAME = "RecFormat";
			}
			else {
				REC_FORMAT_NAME = "RecFormat2";
			}

			const char* pre_rec_format = config_get_string(config, "lovense", "RecFormatBackup");
			if (pre_rec_format) {
				config_set_string(config, mode_key, REC_FORMAT_NAME, pre_rec_format);
			}
		}

		{
			bool auto_remux = config_get_bool(config, "lovense", "AutoRemuxBackup");
			if (auto_remux) {
				//config_set_bool(config, "Video", "AutoRemux", true);			
				obs_queue_task(OBS_TASK_GRAPHICS, [](void*) {
						auto c = obs_frontend_get_profile_config();
						if (c) {
							config_set_bool(c, "Video", "AutoRemux", true);
						}
					},
					nullptr, false);
			}
		}

		config_remove_value(config, "lovense", "RecFormatBackup");
		config_remove_value(config, "lovense", "AutoRemuxBackup");

		return true;
	}


	int handle_start_record(const json11::Json& data, json11::Json::object& resp, std::string& err_message)
	{
		rs_info("handle_start_record");
		//std::filesystem::path path = data["data"]["path"].string_value();
		std::filesystem::path path;
		try {
			path = std::filesystem::path{ get_record_dir() };
#if 0
			if (path.empty()) {
				path = GetDefaultVideoSavePath();
				if (path.empty()) {
					err_message = "record path invalid";
					return error::lvs_ParamsBad;
				}				
			}

			std::filesystem::create_directories(path);
#endif
		}
		catch (const std::filesystem::filesystem_error& err) {
			err_message = string_util::format("[%d]%s", err.code(), err.what());
			return error::lvs_FileSysteError;
		}

		bool is_active = obs_frontend_recording_active();
		bool is_paused = obs_frontend_recording_paused();

		if (is_active || is_paused) {
			return error::lvs_RecordActived;
		}

		bool ok = set_record_config("mp4", path.u8string().c_str());
		if (!ok) {
			return error::lvs_Settings;
		}

		obs_frontend_recording_start();

		return error::lvs_Ok;
	}

	int handle_stop_record(const json11::Json& data, json11::Json::object& resp, std::string& err_message)
	{
		rs_info("handle_stop_record");

		obs_frontend_recording_stop();
		//restore_record_config();

		return error::lvs_Ok;
	}

	static std::string get_record_dir()
	{
		std::string record_path;

		do {
			auto config = obs_frontend_get_profile_config();
			if (!config) {
				break;
			}

			auto mode = config_get_string(config, "Output", "Mode");
			if (!mode) {
				break;
			}

			const char* mode_key{ nullptr };
			const char* path_key{ nullptr };
			if (strcmp(mode, "Simple") == 0) {
				mode_key = "SimpleOutput";
				path_key = "FilePath";
			}
			else if (strcmp(mode, "Advanced") == 0) {
				mode_key = "AdvOut";
				path_key = "RecFilePath";
			}
			else {
				break;
			}

			const char* path = config_get_string(config, mode_key, path_key);
			if (path) {
				record_path = path;
			}
		} while (false);


		if (record_path.empty()) {
			record_path = GetDefaultVideoSavePath();
		}

		return record_path;
	}
		

	int handle_get_record_settings(const json11::Json& data, json11::Json::object& resp, std::string& err_message)
	{
		std::string record_path = get_record_dir();	
		resp["record_dir"]      = json11::Json(record_path);

		return error::lvs_Ok;
	}

#ifdef WIN32
	static std::wstring __default_record_dir;
	int CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
	{
		if (uMsg == BFFM_INITIALIZED)
		{
			::SendMessage(hwnd, BFFM_SETSELECTION, TRUE, lpData);
			DWORD exStyle = GetWindowLong((HWND)hwnd, GWL_EXSTYLE);
			bool is_top = (exStyle & WS_EX_TOPMOST) != 0;
			if (!is_top) {
				::SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
			}
			gFileBrowserHwnd = hwnd;
		}

		return 0;
	}
#endif

	static bool set_record_dir(std::filesystem::path& path)
	{
		std::error_code err_code;
		bool ok = std::filesystem::exists(path, err_code) && std::filesystem::is_directory(path, err_code);
		if (!ok) {
			return false;
		}

		auto config = obs_frontend_get_profile_config();
		if (!config) {
			rs_warn("restore_record_config get config failed");
			return false;
		}

		auto mode = config_get_string(config, "Output", "Mode");
		if (!mode) {
			rs_warn("restore_record_config get Mode failed");
			return false;
		}

		const char* mode_key{ nullptr };
		const char* path_key{ nullptr };
		if (strcmp(mode, "Simple") == 0) {
			mode_key = "SimpleOutput";
			path_key = "FilePath";
		}
		else if (strcmp(mode, "Advanced") == 0) {
			mode_key = "AdvOut";
			path_key = "RecFilePath";
		}
		else {
			rs_warn("restore_record_config unknown mode: %s", mode);
			return false;
		}

		config_set_string(config, mode_key, path_key, path.u8string().c_str());

		return true;
	}

	int handle_open_record_dir(const json11::Json& data, json11::Json::object& resp, std::string& err_message)
	{
#ifdef WIN32
		std::string record_path = get_record_dir();
		            record_path = string_util::replace(record_path, "/", "\\");
		std::string strcmd      = "explorer \"";
		            strcmd      += record_path;
		            strcmd      += "\"";
		DWORD exitcode = 0;
		bool ok = lovense::ws::executeCommandLine(
			string_util::string2wstring(strcmd), exitcode);
		if (!ok || exitcode != 0) {
			return error::lvs_FileSysteError;
		}
#else
		//assert(false, "to Implement");
		//todo Implement
        std::string record_path = get_record_dir();
        std::string str_cmd = "open '" + record_path + "'";
        system(str_cmd.c_str());
#endif
		return error::lvs_Ok;
	}

	int handle_select_record_dir(const json11::Json& data, json11::Json::object& resp, std::string& err_message)
	{
#ifdef WIN32
		if (gShutdown) {
			return error::lvs_Ok;
		}

		std::string record_path = get_record_dir();
		if (!record_path.empty()) {
			try {
				std::filesystem::path record_path_obj = std::filesystem::absolute(record_path);
				__default_record_dir = record_path_obj.wstring();
				
			}
			catch (const std::filesystem::filesystem_error&) {}
		}

		if (record_path.empty()) {
			return error::lvs_Settings;
		}


		if (gFileBrowserHwnd != nullptr) {
			return error::lvs_StatusError;
		}

		gRecordExplorerFutrue =  std::async([](){

			LPITEMIDLIST pidl = NULL;
			SHGetSpecialFolderLocation(NULL, CSIDL_DRIVES, &pidl);

			TCHAR szBuffer[MAX_PATH] = { 0 };
			BROWSEINFO bi;
			ZeroMemory(&bi, sizeof(BROWSEINFO));
			bi.hwndOwner      = (HWND)obs_frontend_get_main_window_handle();
			bi.pszDisplayName = (LPWSTR)__default_record_dir.c_str();
			bi.lpszTitle      = NULL;
			bi.pidlRoot       = pidl;
			bi.ulFlags        = BIF_RETURNONLYFSDIRS
				                | BIF_NEWDIALOGSTYLE | BIF_DONTGOBELOWDOMAIN
				                | BIF_BROWSEFORCOMPUTER
				                ;
			bi.lParam         = (LPARAM)(LPCTSTR)(__default_record_dir.c_str());
			bi.lpfn           = BrowseCallbackProc;

			LPITEMIDLIST idl = SHBrowseForFolder(&bi);
			gFileBrowserHwnd = nullptr;
			if (gShutdown) {
				return;
			}

			if (NULL != idl)
			{
				SHGetPathFromIDList(idl, szBuffer);

				try {
					std::filesystem::path record_path{ szBuffer };
					set_record_dir(record_path);

					notify_record_settings();
				}
				catch (std::filesystem::filesystem_error&) {}
			}
			else {
				
			}

			
		});
		
#else
		//assert(false, "to Implement");
		//todo Implement
#endif

		return error::lvs_Ok;
	}

	
	static void notify_record_status(int record_status)
	{

	}

	static void notify_record_settings()
	{
		
	}

	static void shutdown()
	{
#ifdef WIN32
		gShutdown = true;
		if (gFileBrowserHwnd) {
			::SendMessage(gFileBrowserHwnd.load(), WM_COMMAND, MAKEWPARAM(IDCANCEL, 0), NULL);
		}
		if (gRecordExplorerFutrue.valid()) {
			gRecordExplorerFutrue.wait();
		}		
#endif
	}
}
