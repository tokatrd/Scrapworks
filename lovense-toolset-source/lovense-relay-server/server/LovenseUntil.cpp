#include "LovenseUntil.h"
#include <vector>
#include <string>
#include <list>
#include <memory>
#include <iostream>
#include <obs.hpp>
#include <obs-frontend-api.h>
#include "util/platform.h"
#include "util/util.hpp"
#include "util/dstr.h"
#include "util/dstr.hpp"


#include "json11/json11.hpp"

#include "lovense_websocket_common.hpp"
#include "lovense_camera_source.hpp"
#include "lovense_util_helper.hpp"
#include "lovense_websocket_helper.hpp"

#include "callback/proc.h"

#include "rs_server.hpp"


#ifdef DEF_TOOLSETS
#include "obs-module.h"
#endif

#ifdef _WIN32
#define DEF_DSHOW_SOURCE_ID "dshow_input"
#else
#define DEF_DSHOW_SOURCE_ID "av_capture_input"
#endif
#define DEF_LOVENSE_DSHOW_SOURCE_ID "lovense_dshow_input"

using namespace std;

static std::vector<std::string> g_tmp_source_id;

std::vector<std::string> GetCameraNameList(obs_source_t* source) {
	std::vector<std::string> names;
	obs_properties_t* properties = obs_source_properties(source);
	obs_property_t* property = obs_properties_first(properties);
	while (property) {
		const char* name = obs_property_name(property);
		if (0 == strcmp(name, "video_device_id")) {
			size_t count = obs_property_list_item_count(property);
			for (size_t i = 0; i < count; i++) {
				const char* camera_name = obs_property_list_item_name(property, i);
				const char* camera_id = obs_property_list_item_string(property, i);
				if (CheckIsPhysicalCameraID(camera_id))
				{
					names.push_back(camera_name);
				}
			}
			break;
		}
		obs_property_next(&property);
	}

	obs_properties_destroy(properties);
	return names;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////

static inline void decode_dstr(struct dstr* str)
{
	dstr_replace(str, "#3A", ":");
	dstr_replace(str, "#22", "#");
}

static inline bool DecodeDeviceDStr(DStr& name, DStr& path,
	const char* device_id)
{
	const char* path_str;

	if (!device_id || !*device_id)
		return false;

	path_str = strchr(device_id, ':');
	if (!path_str)
		return false;

	dstr_copy(path, path_str + 1);
	dstr_copy(name, device_id);

	size_t len = path_str - device_id;
	name->array[len] = 0;
	name->len = len;

	decode_dstr(name);
	decode_dstr(path);

	return true;
}

static bool GetCameraSettingInfo(obs_source_t* source, std::shared_ptr<CameraInfo> pInfo) {
	bool ret = true;
	obs_data_t* setting = obs_source_get_settings(source);
#ifdef WIN32
	const char* deviceId = obs_data_get_string(setting, "video_device_id");

	DStr name, path;
	if (!DecodeDeviceDStr(name, path, deviceId)) {
		return false;
	}
#if 0
	char* szName = dstr_to_mbs(name);
	if (szName) {
		pInfo->deviceName = szName;
		bfree(szName);
	}

	char* szPath = dstr_to_mbs(path);
	if (szPath) {
		pInfo->deviceId = szPath;
		bfree(szPath);
	}
#else
	if (name->array && name->len > 0) {
		pInfo->deviceName = name;
	}

	if (path->array && path->len > 0) {
		pInfo->deviceId = path;
	}
#endif

	int res_type = obs_data_get_int(setting, "res_type");
	pInfo->resType = res_type;
	if (res_type == 1) {
		const char* res = obs_data_get_string(setting, "resolution");
		pInfo->deviceRes = res;
		//frame_interval
		int frame_fps = obs_data_get_int(setting, "frame_interval");
		pInfo->captureFPS = frame_fps;
	}
#else
	const char* name = obs_data_get_string(setting, "device_name");
	if (name)
		pInfo->deviceName = name;

	bool use_preset = obs_data_get_bool(setting, "use_preset");
	if (!use_preset) {
		obs_data_t* framerate = obs_data_get_obj(setting, "frame_rate");
		if (framerate) {
			int numerator = obs_data_get_int(framerate, "numerator");
			pInfo->captureFPS = numerator;
			obs_data_release(framerate);
		}
		const char* resolution = obs_data_get_string(setting, "resolution");
		if (resolution) {
			//"{\"height\":720,\"width\":1280}"
			std::string err;
			json11::Json root = json11::Json::parse(resolution, err);
			if (root.is_object()) {
				int height = root["height"].int_value();
				int width = root["width"].int_value();
				char buff[20] = { 0 };
				sprintf(buff, "%dx%d", width, height);
				pInfo->deviceRes = buff;
			}
		}
	}

#endif
	obs_data_release(setting);
	return ret;
}

std::shared_ptr<CameraInfo> LovenseUntil::GetCurrentSceneCameraInfo()
{
	obs_source_t* current_scene_source = obs_frontend_get_current_scene();
	OBSSource dshowSource;
	if (current_scene_source) {
		obs_scene_t* scene = obs_scene_from_source(current_scene_source);
		obs_scene_enum_items(scene, [](obs_scene_t* scene, obs_sceneitem_t* item, void* param)->bool {
			obs_source* source = obs_sceneitem_get_source(item);
			if (source) {
				const char* source_id = obs_source_get_id(source);
				if (source_id && (strcmp(source_id, DEF_DSHOW_SOURCE_ID) == 0 || strcmp(source_id, DEF_LOVENSE_DSHOW_SOURCE_ID) == 0)) {
					OBSSource* dshow_source = (OBSSource*)param;
					*dshow_source = source;
				}
			}
			return true;
			}, &dshowSource);
		obs_source_release(current_scene_source);
	}

	if (dshowSource) {
		std::shared_ptr<CameraInfo> pInfo = std::make_shared<CameraInfo>();
		if (GetCameraSettingInfo(dshowSource, pInfo))
		{
			return pInfo;
		}
	}

	return nullptr;
}

namespace lovense
{
	namespace ws {
		void DebugLog(const std::string &log)
		{
#ifdef _DEBUG
			blog(LOG_INFO, "[lovense]:%s",log.c_str());
#endif
		}
#ifdef WIN32
		bool  executeCommandLine(std::wstring cmdLine, DWORD& exitCode)
		{
			PROCESS_INFORMATION processInformation = { 0 };
			STARTUPINFO startupInfo = { 0 };
			startupInfo.cb = sizeof(startupInfo);
			int nStrBuffer = cmdLine.size() + 50;


			// Create the process

			BOOL result = CreateProcess(NULL, (LPWSTR)cmdLine.data(),
				NULL, NULL, FALSE,
				NORMAL_PRIORITY_CLASS | CREATE_NO_WINDOW,
				NULL, NULL, &startupInfo, &processInformation);


			if (!result)
			{
				// CreateProcess() failed
				// Get the error from the system
				LPVOID lpMsgBuf;
				DWORD dw = GetLastError();
				FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
					NULL, dw, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, NULL);

				// Display the error

				// Free resources created by the system
				LocalFree(lpMsgBuf);

				// We failed.
				return FALSE;
			}
			else
			{
				// Successfully created the process.  Wait for it to finish.
				WaitForSingleObject(processInformation.hProcess, INFINITE);

				// Get the exit code.
				result = GetExitCodeProcess(processInformation.hProcess, &exitCode);

				// Close the handles.
				CloseHandle(processInformation.hProcess);
				CloseHandle(processInformation.hThread);

				if (!result)
				{
					// Could not get exit code.
					//TRACE(_T("Executed command but couldn't get exit code.\nCommand=%s\n"), cmdLine);
					return FALSE;
				}
				// We succeeded.
				return TRUE;
			}
		}
#endif

#ifdef DEF_TOOLSETS
		static bool CheckPortBinding(unsigned short port)
		{
			std::string message;
			bool use = check_port_binding(port, message);
			blog(LOG_INFO, "[lovense] %s", message.c_str());
			return use;
		}

		unsigned short GetWsAvailablePort(bool reCheck)
		{
			static unsigned short s_tempPort = DEF_TOOLSET_WS_WEBSOCKET_PORT;
			if (reCheck) {
				if (s_tempPort != DEF_TOOLSET_WS_WEBSOCKET_PORT)
					s_tempPort = DEF_TOOLSET_WS_WEBSOCKET_PORT;
				bool in_use = CheckPortBinding(s_tempPort);
				if (in_use) {
					s_tempPort = DEF_TOOLSET_WS_WEBSOCKET_PORT + 10;
					in_use = CheckPortBinding(s_tempPort);
					if (in_use) {
						s_tempPort += 10;
						in_use = CheckPortBinding(s_tempPort);
						if (in_use)
							s_tempPort = 0;
					}
				}
			}
			blog(LOG_INFO, "[lovense]: ws port is = %d", s_tempPort);
			return s_tempPort;
		}
		unsigned short GetWSSAvailablePort(bool reCheck)
		{
			static unsigned short s_tempPort = DEF_TOOLSET_WSS_WEBSOCKET_PORT;
			if (reCheck) {
				if (s_tempPort != DEF_TOOLSET_WSS_WEBSOCKET_PORT)
					s_tempPort = DEF_TOOLSET_WSS_WEBSOCKET_PORT;
				bool in_use = CheckPortBinding(s_tempPort);
				if (in_use) {
					s_tempPort = DEF_TOOLSET_WSS_WEBSOCKET_PORT + 10;
					in_use = CheckPortBinding(s_tempPort);
					if (in_use) {
						s_tempPort += 10;
						in_use = CheckPortBinding(s_tempPort);
						if (in_use)
							s_tempPort = 0;
					}
				}
			}
			blog(LOG_INFO, "[lovense]: wss port is = %d", s_tempPort);
			return s_tempPort;
		}
#endif

		//-------------------------------------------------------------------------------------
		void OnProcReceivedData(ws::WsocketType socket_t, calldata_t* data, int clientType, const char* name)
		{
			const char* value = nullptr;
			bool ret = calldata_get_string(data, name, &value);
			if (ret) {				
				rs::ForwardMessage((ws::WsocketType)socket_t, value, clientType, true);
			}			
		}

	}

	////////////////////////////////////////////////////////////////////////
	bool UpdateOBSRecordStatus(bool bStart)
	{
		bool isActive = obs_frontend_recording_active();
		if (bStart && !isActive)
			obs_frontend_recording_start();
		else if (!bStart && isActive)
			obs_frontend_recording_stop();
		return true;
	}

	std::string GetOBSRecordDirectory()
	{
		config_t *config = obs_frontend_get_profile_config();
		if (config)
		{
			const char* path = config_get_string(config, "AdvOut", "RecFilePath");
			if (path)
				return path;
		}
		return "";
	}

	bool GetOBSRecordStatus()
	{
		bool isActive = obs_frontend_recording_active();
		return isActive;
	}
	////////////////////////////////////////////////////////////////////////
		
}
