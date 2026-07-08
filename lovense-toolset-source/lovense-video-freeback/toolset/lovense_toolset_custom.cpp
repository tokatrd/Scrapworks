#include "util/dstr.hpp"
#include "util/platform.h"

#include <process.h> 

#include "lovense_obs_common.hpp"
#include "lovense_toolset_updater.hpp"
#include "lovense_toolset_custom.hpp"

#include "cef-headers.hpp"
#include "include/cef_api_hash.h"

extern void toolset_source_get_defaults(obs_data_t* settings);

namespace lovense::toolset {

	using namespace std;

	bool initialize()
	{
		if (!is_valid_obs())
		{
			HWND handle = (HWND)obs_frontend_get_main_window_handle();
			::MessageBox(handle, L"Your OBS version is too low. If you want to continue to use Lovense OBS Toolset, please upgrade your OBS version first!", L"Lovense OBS Toolset", MB_OK | MB_ICONWARNING);
			return false;
		}

		const char* api_hash = cef_api_hash(0);
		if (strcmp(api_hash, CEF_API_HASH_PLATFORM)) {
			// The libcef API hash does not match the current header API hash.
			llog(LOG_ERROR, "[lovense-toolset]cef version error. lib api_hash:%s need: %s", api_hash, CEF_API_HASH_PLATFORM);
			return false;
		}

		startCopyFile();

		char szLocalFile[512] = { 0 };
		if (os_get_config_path(szLocalFile, sizeof(szLocalFile),
			"obs-studio\\dist\\index.html") <= 0) {
			llog(LOG_ERROR, "get dist index.html path failed");
			return false;
		}
			

		if (!os_file_exists(szLocalFile)) {
			llog(LOG_ERROR, "dist index.html file missing");
			return false;
		}
			

		//GetMO
		wchar_t szPaht[256] = { 0 };
		GetModuleFileName(NULL, szPaht, 256);
		std::wstring wstrPath = szPaht;
		wstrPath = wstrPath.substr(0, wstrPath.find_last_of(L'\\') + 1);
#ifdef _WIN64
		wstrPath += L"..\\..\\obs-plugins\\64bit\\lovese_plugin_x64.dll";
#else
		wstrPath += L"..\\..\\obs-plugins\\32bit\\lovese_plugin_x86.dll";
#endif
		if (PathFileExists(wstrPath.c_str()))
		{
			blog(LOG_INFO, "[lovense]: lovense feedback has been installed!");
			return false;
		}

		return true;
	}

	void on_load_end()
	{
#if 0 //20230330 Update logic moved to lovense-relay-server execution
		char path[512] = { 0 };
		if (os_get_config_path(path, sizeof(path), "obs-studio/dist") <= 0)
			blog(LOG_ERROR, "[Lovese] os_get_config_path locale dist path is  fail ");
		if (!os_mkdir(path))
			blog(LOG_ERROR, "[Lovese] os_mkdir locale dist path is  fail ");
		_beginthread(UpdateFile, 0, nullptr);
#endif
	}

	static bool g_is_find_browser_source = false;
	bool func_scene_enum_call(obs_scene_t* scene, obs_sceneitem_t* item, void* param)
	{
		bool ret = true;
		obs_source_t* source = obs_sceneitem_get_source(item);
		if (source)
		{
			const char* name = obs_source_get_id(source);
			if (name)
			{
				if (strcmp(name, lovense::FEEDBACK_SOURCE_ID) == 0)
				{
					g_is_find_browser_source = true;
					return false;
				}
			}
		}
		return ret;
	}
	

	void on_obs_frontend_event(enum obs_frontend_event event, void*)
	{
		switch (event) {

		case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		{
			if (!lovense_plugins_is_setup())
			{
				obs_source_t* current_sense_source = obs_frontend_get_current_scene();
				obs_scene_t* current_scene = obs_scene_from_source(current_sense_source);

				obs_scene_enum_items(current_scene, func_scene_enum_call, nullptr);
				if (current_scene && !g_is_find_browser_source)
				{

					obs_data_t* setting = obs_data_create();
					toolset_source_get_defaults(setting);
					obs_source_t* brower_source = obs_source_create(lovense::FEEDBACK_SOURCE_ID, "Lovense Video Feedback", setting, NULL);
					if (brower_source)
					{
						obs_sceneitem_t* item = obs_scene_add(current_scene, brower_source);
						obs_sceneitem_set_locked(item, true);
						obs_source_release(brower_source);
						obs_data_release(setting);
					}
				}
				if (current_sense_source)
					obs_source_release(current_sense_source);
				setup_lovense_plugin_config();
			}
		}break;
		default:
			break;
		}
	}

	//----------------------------------------------------------------------------------------
	void start_server()
	{
		//SmartCamModel::StartServerModel();
	}

	void stop_server()
	{
		//SmartCamModel::StopServerModel();
	}
	//----------------------------------------------------------------------------------------

	bool is_valid_obs()
	{
		uint32_t obs_ver = obs_get_version();
		uint32_t obs_maj = obs_ver >> 24;
		return obs_maj >= 24;
	}

	wstring string2wstring(string str)
	{

		wchar_t wszCmd[1000] = { 0 };
		os_utf8_to_wcs(str.c_str(), str.size(), wszCmd, 1000 * 2);
		return std::wstring(wszCmd);
		wstring result;
		//Get buffer size and allocate space, buffer size calculated by characters
		int len = MultiByteToWideChar(CP_ACP, 0, str.c_str(), str.size(), NULL, 0);
		TCHAR* buffer = new TCHAR[len + 1];
		//Convert multi-byte encoding to wide-byte encoding
		MultiByteToWideChar(CP_ACP, 0, str.c_str(), str.size(), buffer, len);
		buffer[len] = '\0';             //Add string terminator
		//Delete buffer and return value
		result.append(buffer);
		delete[] buffer;
		return result;
	}

	//Convert wstring to string
	string wstring2string(wstring wstr)
	{
		string result;
		//Get buffer size and allocate space, buffer size calculated by bytes
		int len = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), wstr.size(), NULL, 0, NULL, NULL);
		char* buffer = new char[len + 1];
		//Convert wide-byte encoding to multi-byte encoding
		WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), wstr.size(), buffer, len, NULL, NULL);
		buffer[len] = '\0';
		//Delete buffer and return value
		result.append(buffer);
		delete[] buffer;
		return result;
	}

	//-----------------------------------------------------------------------
	void ReplaceChar(char* pStr, char cDest, char cSrc)
	{
		char* pTmp = pStr;
		while ('\0' != *pTmp) {
			if (*pTmp == cSrc)
				*pTmp = cDest;
			pTmp++;
		}
	}

	std::string FilePathToHtmlPath(const char* file)
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

	//-----------------------------------------------------
	bool lovense_plugins_is_setup()
	{
		char path[512] = { 0 };
		if (os_get_config_path(path, sizeof(path), "obs-studio\\PluginVersion.ini") <= 0)
		{
			blog(LOG_ERROR, "[Lovese] ReadVersionConfig -> obs-studio\\PluginVersion.ini fail !");
			//Configuration file does not exist, no processing needed
			return true;
		}
		wstring strAppPath = string2wstring(path);
		TCHAR Result[MAX_PATH] = { 0 };
		GetPrivateProfileString(L"OBS_PLUGIN", L"LovenseInit", L"0", Result, MAX_PATH, strAppPath.c_str());
		wstring wstrResult = Result;
		if (wstrResult == L"0")
		{
			return false;
		}
		else
			return true;
	}

	void setup_lovense_plugin_config()
	{
		char path[512] = { 0 };
		if (os_get_config_path(path, sizeof(path), "obs-studio\\PluginVersion.ini") <= 0)
		{
			blog(LOG_ERROR, "[Lovese] WriteVersionConfig -> obs-studio\\PluginVersion.ini fail !");
		}
		wstring  strAppPath = string2wstring(path);
		WritePrivateProfileString(L"OBS_PLUGIN", L"LovenseInit", L"1", strAppPath.c_str());
	}
}

