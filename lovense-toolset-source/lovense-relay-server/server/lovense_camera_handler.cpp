#include "obs-frontend-api.h"

#include "lovense_obs_common.hpp"
#include "lovense_websocket_common.hpp"
#include "lovense_string_helper.hpp"
#include "lovense_obs_properties.hpp"

#include "rs_common.hpp"
#include "lovense_camera_handler.hpp"


#include <filesystem>

#ifdef _WIN32
#include "libdshowcapture/dshowcapture.hpp"
#endif


namespace camera {
	using namespace lovense;

	static void OnProcRecivCameraData(void* param, calldata_t* data)
	{
		OnProcReceivedData(ws::WsocketType::WS_SOCKET, data, lovense::ws::CLIENT_FROM_CAM, "FaceDetect");
	}

	void setup_proc_handler(proc_handler* handle, ws::WebServerImpl* server)
	{
		if (server->SocketType() == ws::WsocketType::WS_SOCKET)
		{
			static int socket_t = 0;
			proc_handler_add(handle, "void OnProcRecivCameraData(string json)", OnProcRecivCameraData, (void*)&socket_t);
		}		
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////
	
	void StartAIMode(bool bStart, std::string pType)
	{
		calldata_t cData;
		calldata_init(&cData);
		calldata_set_bool(&cData, "AIMode", bStart);
		calldata_set_string(&cData, "type", pType.c_str());
		proc_handler_t* handle = obs_get_proc_handler();
		if (handle)
		{
			proc_handler_call(handle, "StartAIMode", &cData);
		}
		calldata_free(&cData);
	}

	bool GetAIModeStatus(std::string pType)
	{
		bool is_enabled = false;
		calldata_t cData;
		calldata_init(&cData);
		calldata_set_string(&cData, "type", pType.c_str());
		calldata_set_ptr(&cData, "result", &is_enabled);
		proc_handler* handle = obs_get_proc_handler();
		if (handle) {
			proc_handler_call(handle, "GetAIModeStatus", nullptr);
		}
		calldata_free(&cData);
		return is_enabled;
	}

	void CallWSResetLovenseScene(std::string type)
	{
		calldata_t cData;
		calldata_init(&cData);
		proc_handler_t* handle = obs_get_proc_handler();
		if (handle)
		{
			calldata_set_string(&cData, "type", type.c_str());
			proc_handler_call(handle, "WSResetLovenseScene", &cData);
		}
		calldata_free(&cData);
	}

	bool CheckAiConfigValid(std::string  type) {
		//
		calldata_t cData;
		calldata_init(&cData);
		proc_handler_t* handle = obs_get_proc_handler();
		bool valid{ false };
		if (handle)	{
			calldata_set_string(&cData, "type", type.c_str());
			calldata_set_ptr(&cData,    "valid", (void*)(&valid));
			proc_handler_call(handle,   "__lovense_ai_camera_CheckValid", &cData);
		}
		calldata_free(&cData);

		return valid;
	}

	static std::vector<std::string> g_vActiveDevicesID;
	static bool scene_enum_func(obs_scene_t* scene, obs_sceneitem_t* item, void* param)
	{
		bool* pActive = (bool*)param;
		obs_source_t* source = obs_sceneitem_get_source(item);
#ifdef WIN32
		const char* source_id = obs_source_get_id(source);
		if (source_id && (strcmp(source_id, CAMERA_DSHOW_SOURCE_ID) == 0
			|| strcmp(source_id, LOVENSE_AI_CAMERA_SOURCE_ID) == 0))
		{
			obs_data_t* setting = obs_source_get_settings(source);
			bool isActive = obs_data_get_bool(setting, "active");
			const char* deviceId = obs_data_get_string(setting, "video_device_id");
			/*if (isActive == *pActive)
			{
				obs_data_release(setting);
				return true;
			}*/

			//TODO:if device id is active in current scene
			if (deviceId && strlen(deviceId) > 0)
			{
				auto iter = std::find(g_vActiveDevicesID.begin(), g_vActiveDevicesID.end(), deviceId);
				if (iter != g_vActiveDevicesID.end())
				{
					obs_data_release(setting);
					return true;
				}
			}

			obs_data_set_bool(setting, "active", *pActive);
			obs_source_update(source, setting);
			obs_data_release(setting);

			proc_handler_t* proc = obs_source_get_proc_handler(source);
			if (proc)
			{
				calldata_t* data = calldata_create();
				calldata_init(data);
				calldata_set_bool(data, "active", *pActive);
				proc_handler_call(proc, "activate", data);
				calldata_destroy(data);
				if (deviceId)
					g_vActiveDevicesID.push_back(deviceId);
			}
			rs_info("[websocket] active camera:%s", deviceId);

			return true;
		}

		return true;
#endif
	}

	static bool g_isActived = false;
	bool ActiveOBSCamera(bool active)
	{
#ifdef WIN32		
		g_vActiveDevicesID.clear();
		g_isActived = active;
		obs_source_t* source = obs_frontend_get_current_scene();
		OBSScene current_scene = obs_scene_from_source(source);
		obs_scene_enum_items(current_scene, scene_enum_func, &active);
		obs_source_release(source);

		rs_info("[websocket] active camera:%d", active);
		return true;
#else
		return true;
#endif
	}

	int handle_get_camera_list(const json11::Json& data, json11::Json::object& resp, std::string& err_message)
	{
#ifdef WIN32	
		json11::Json::array camera_list;

		std::vector<DShow::VideoDevice> devices;
		DShow::Device::EnumVideoDevices(devices);
		for (const DShow::VideoDevice& device : devices) {

			std::string name;
			try {				
				std::filesystem::path obj{ device.name };
				name = obj.u8string();
			}
			catch (...) {
				name = string_util::wstring2string(device.name);
			}
			
			//std::string name = string_util::wstring2string(device.name);
			std::string path = string_util::wstring2string(device.path);

			//path = "\\\\?\\usb#vid_2e1a&pid_4c02&mi_00#6&14687df2&1&0000#{65e8773d-8f56-11d0-a3b9-00a0c9223196}\\global"
			/*path = string_util::replace(path, "#", "#22");
			path = string_util::replace(path, "#", "#3A");

			CameraDevice device(name + ":" + path); */
			bool is_physic = string_util::start_with(path, "\\\\?\\usb");

			json11::Json::object camera = {
				{"name", name},
				{"path", path},
				{"virtual", !is_physic}
			};
			camera_list.push_back(camera);

			
		}
		resp["cameras"] = camera_list;
		return 0;
#else
		return error::lvs_NotImplemented;
#endif
		
	}
}



