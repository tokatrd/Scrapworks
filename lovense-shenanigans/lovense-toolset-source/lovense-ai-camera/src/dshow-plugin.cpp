#include <obs-module.h>
#include <strsafe.h>
#include <strmif.h>
#include <iostream>
#include <obs-frontend-api.h>
#include <util/config-file.h>
#include "json11/json11.hpp"
#include <util/platform.h>
#include <queue>
#include <thread>
#include <string>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <util/dstr.hpp>
#include "OBSVideoAdapter.h"

#ifdef VIRTUALCAM_ENABLED
	#include "virtualcam-guid.h"
#endif
OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("win-dshow", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "Lovense AI Camera";
}

#ifdef _DEBUG
static bool g_AiMode = false;//Debug: Enable face detection by default
static bool g_smoothAngles = true;
#else
//TODO: Distribute messages to thread to avoid affecting detection efficiency
static bool g_AiMode = false;// false by default, Release: Disabled by default, needs to be enabled by extension plugin through protocol
static bool g_smoothAngles = true;
#endif

std::queue<std::string> g_json_queue;
std::thread g_handle_thread;
std::condition_variable g_con;
std::mutex g_queue_metux;
static bool g_atomic_msg_start = true;
static bool g_thread_init = false;
static bool g_scene_has_ai_source = false;
static obs_source_t* g_video_source = nullptr;
static obs_sceneitem_t* g_video_scene_item = nullptr;
//cita: check when obs is finished loading!
static bool g_is_finisheLoading = false;
static bool g_is_firstRuning = true;

static void ResetLovenseScene(bool setcurrent = true);
extern void RegisterDShowSource();
extern void RegisterDShowEncoders();
extern std::string SendhandleEmptyDetectResult();
//v2
extern int LoadModel();
static void CheckAiDshowSource();

void AiSourceDidAddToScene(bool isCreate) {
	if(isCreate)
		g_scene_has_ai_source = true;
	else
	{
		CheckAiDshowSource();
	}
}

void AiSourceDidRemoveToScene() {
	g_scene_has_ai_source = false;
}



#ifdef VIRTUALCAM_ENABLED
extern "C" struct obs_output_info virtualcam_info;

static bool vcam_installed(bool b64)
{
	wchar_t cls_str[CHARS_IN_GUID];
	wchar_t temp[MAX_PATH];
	HKEY key = nullptr;

	StringFromGUID2(CLSID_OBS_VirtualVideo, cls_str, CHARS_IN_GUID);
	StringCbPrintf(temp, sizeof(temp), L"CLSID\\%s", cls_str);

	DWORD flags = KEY_READ;
	flags |= b64 ? KEY_WOW64_64KEY : KEY_WOW64_32KEY;

	LSTATUS status = RegOpenKeyExW(HKEY_CLASSES_ROOT, temp, 0, flags, &key);
	if (status != ERROR_SUCCESS) {
		return false;
	}

	RegCloseKey(key);
	return true;
}
#endif

void StartAIMode(void* data, calldata_t* callData)
{
	if (callData)
	{
		bool value = 0;
		bool ret = calldata_get_bool(callData, "AIMode", &value);
		if (ret)
		{
			g_AiMode = value;
		}
	}
}

void StartSmoothAngles(void* data, calldata_t* callData)
{
	if (callData)
	{
		bool value = 0;
		bool ret = calldata_get_bool(callData, "AIMode", &value);
		if (ret)
		{
			g_AiMode = value;
		}
	}
}
bool BeAIMode(void* data, calldata_t* callData) {
	return false;
}

bool ShouldDetectFace() {
	return g_AiMode & g_scene_has_ai_source;
}

void SetShouldDetectFace(bool bStart)
{
	g_AiMode = bStart;
}

static void WSClientSendMessage(const std::string& json) {
	proc_handler_t* handle = obs_get_proc_handler();
	if (handle)
	{
		calldata_t cData;
		calldata_init(&cData);
		calldata_set_string(&cData, "FaceDetect", json.c_str());
		proc_handler_call(handle, "OnProcRecivCameraData", &cData);
	}
}

void OnAIDetectData(const std::string& json) {

	if (!g_scene_has_ai_source)
		return;
	{
		if (g_atomic_msg_start)
		{
			std::unique_lock<std::mutex> lock(g_queue_metux);
			g_json_queue.push(json);
			g_con.notify_all();
		}
	}
	//TODO: Distribute messages to thread to avoid affecting detection efficiency
	if (!g_thread_init)
	{
		g_thread_init = true;
		g_handle_thread = std::thread([]() -> void {

			while (g_atomic_msg_start)
			{
				{
					std::unique_lock<std::mutex> lock(g_queue_metux);
					if (g_json_queue.empty())
					{
						g_con.wait(lock);
					}
				}
				proc_handler_t* handle = obs_get_proc_handler();
				if (handle && !g_json_queue.empty())
				{
					std::unique_lock<std::mutex> lock(g_queue_metux);
					const std::string tJson = g_json_queue.front();
					g_json_queue.pop();
					lock.unlock();
					WSClientSendMessage(tJson);
#ifdef DEBUG
					blog(LOG_INFO, "[Face Data]:%s", tJson.c_str());
#endif // DEBUG
					
				}
			}
		});
	}
}

void GetAIModeStatus(void* data, calldata_t* callData)
{
	proc_handler_t* handle = obs_get_proc_handler();
	if (handle) {
		calldata_t cData;
		calldata_init(&cData);
		calldata_set_bool(&cData, "AIModeStatus", g_AiMode);
		proc_handler_call(handle, "UpdateAIModeStatus", &cData);
	}
}

/**
	szID: OBS ID representation of physical camera, a long string with "usb" character
*/
static bool CheckIsPhysicalCameraID(const char* szID) {
	if (!szID || strlen(szID) == 0)
		return false;
	std::string strID = szID;
	if (strID.find("usb") != std::string::npos)
		return true;
	return false;
}

obs_source_t* FindCameraSource()
{
	obs_frontend_source_list sceneList = {};
	obs_frontend_get_scenes(&sceneList);

	auto find_dshow_input = [](obs_scene_t* scene, obs_sceneitem_t* item, void* data)->bool {
		obs_source_t* source = obs_sceneitem_get_source(item);
		if (source) {
			const char* d_id = obs_source_get_id(source);
			//TODO:cita add:[Template Scene] Find physical camera source
			if (d_id && strcmp(d_id, "lovense_dshow_input") == 0)
			{
#ifdef WIN32
				obs_data_t* setting = obs_source_get_settings(source);
				if (setting)
				{
					const char* did = obs_data_get_string(setting, "video_device_id");
					if (did) {
						//Only physical cameras, no virtual cameras
						if (CheckIsPhysicalCameraID(did))
						{
							g_video_scene_item = item;
							g_video_source = source;
							obs_data_release(setting);
							return false;
						}
					}
					obs_data_release(setting);
				}
#else
				g_video_source = source;
#endif
			}
		}
		return true;
	};

	g_video_source = nullptr;
	for (size_t i = 0; i < sceneList.sources.num - 1; i++) {
		obs_source_t* scene = sceneList.sources.array[i];
		obs_scene_enum_items(obs_scene_from_source(scene), find_dshow_input, nullptr);
	}

	obs_frontend_source_list_free(&sceneList);
	return g_video_source;
}


static char* get_new_source_name(const char* name)
{
	struct dstr new_name = { 0 };
	int inc = 0;

	dstr_copy(&new_name, name);

	for (;;) {
		obs_source_t* existing_source =
			obs_get_source_by_name(new_name.array);
		if (!existing_source)
			break;

		obs_source_release(existing_source);

		dstr_printf(&new_name, "%s %d", name, ++inc + 1);
	}

	return new_name.array;
}


obs_source_t* AddActiveCameraSource(obs_scene_t* scene,bool &isMachPix)
{
	obs_source_t* oldsource = FindCameraSource();
	if (g_video_scene_item)
	{
		//If camera source is already added, no need to add again
		obs_sceneitem_remove(g_video_scene_item);
		g_video_scene_item = nullptr;
		//return nullptr;
	}

	//Get canvas width and height, typically 16:9 or 4:3
	config_t* config = obs_frontend_get_profile_config();
	int cx = (uint32_t)config_get_uint(config, "Video", "BaseCX");
	int cy = (uint32_t)config_get_uint(config, "Video", "BaseCY");
	bool b_169_output = false;
	bool b_43_output = false;
	if (fabs(cx / (float)cy - 16.0 / 9.0) < std::numeric_limits<float>::epsilon())
		b_169_output = true;
	else if (fabs(cx / (float)cy - 4.0 / 3.0) < std::numeric_limits<float>::epsilon())
		b_43_output = true;

	char buff[20] = { 0 };
#ifdef WIN32
	sprintf_s(buff, sizeof(buff), "%dx%d", cx, cy);
#else
	sprintf(buff, "%dx%d", cx, cy);
#endif
	obs_data_t* setting = obs_data_create();
	obs_data_set_int(setting, "res_type", 1);//Custom resolution
	obs_data_set_bool(setting, "active", true);//Whether to activate
	obs_data_set_bool(setting, "force_create", true);//Theoretically only create one, but source destruction is asynchronous

	//TODO: Source names cannot be the same, otherwise only one can be saved
	const char* new_name = get_new_source_name("Lovense AI Camera");
	obs_source_t* source = obs_source_create("lovense_dshow_input", new_name, setting, nullptr);
	obs_data_release(setting);
	if (!source)
		return nullptr;

	obs_data_t* settings = obs_source_get_settings(source);
	obs_properties_t* properties = obs_source_properties(source);

	std::vector<std::string> vResolution169; //16:9
	std::vector<std::string> vResolution43;  //4:3
	std::string camera_id;
	std::string camera_name;
	bool find_usb_camera = false;
	if (NULL != properties) {
		obs_property_t* property = obs_properties_first(properties);
		while (property) {
			const char* name = obs_property_name(property);
			if (0 == strcmp(name, "video_device_id")) {
				/**
					 Get physical camera ID
				*/
				blog(LOG_INFO, "[Camera] id: = %s", name);
				size_t count = obs_property_list_item_count(property);

				for (size_t i = 0; i < count; i++) {
					camera_name = obs_property_list_item_name(property, i);
					camera_id = obs_property_list_item_string(property, i);
					if (CheckIsPhysicalCameraID(camera_id.c_str()))//Check if it's a Windows USB camera
					{
						find_usb_camera = true;
						break;
					}
				}
				if (!camera_id.empty())
				{
					const char* did = obs_data_get_string(settings, "video_device_id");
					obs_data_set_string(settings, name, camera_id.c_str());
					obs_property_modified(property, settings);
				}
			}
			else if (0 == strcmp(name, "resolution")) {
				/**
					 Get camera resolution, mainly 16:9 and 4:3 two resolutions
				*/
				size_t count = obs_property_list_item_count(property);
				int nx = 0;
				int ny = 0;
				for (size_t i = 0; i < count; i++) {
					const char* res = obs_property_list_item_name(property, i);
					blog(LOG_INFO, "Cur camera solution: %s", res);
					sscanf(res, "%dx%d", &nx, &ny);
					if (fabs(nx / (float)ny - 16.0 / 9.0) < std::numeric_limits<float>::epsilon())
					{
						vResolution169.push_back(res);
					}
					else if (fabs(nx / (float)ny - 4.0 / 3.0) < std::numeric_limits<float>::epsilon())
					{
						vResolution43.push_back(res);
					}
				}
				break;
			}
#ifdef __APPLE__
			else if (0 == strcmp(name, "device"))
			{
				size_t count = obs_property_list_item_count(property);
				for (size_t i = 0; i < count; i++) {
					const char* res = obs_property_list_item_name(property, i);
					const char* value = obs_property_list_item_string(property, i);
					std::string cameraname = res;
					if (cameraname.find("FaceTime") != std::string::npos)//FaceTime
					{
						find_usb_camera = true;
						obs_data_set_string(setting, "device", value);
						break;
					}
				}
			}
#endif
			obs_property_next(&property);
		}
		std::string reset_resolution;
		if (b_169_output && vResolution169.size() > 0) {
			int index = vResolution169.size() / 2;
			reset_resolution = vResolution169.at(index);
			obs_data_set_string(setting, "resolution", reset_resolution.c_str());//Set to canvas resolution
			isMachPix = true;
		}
		else if (b_43_output && vResolution43.size() > 0) {
			int index = vResolution43.size() / 2;
			reset_resolution = vResolution43.at(index);
			obs_data_set_string(setting, "resolution", reset_resolution.c_str());//Set to canvas resolution
			isMachPix = true;
		}
		else {//If neither 16:9 nor 4:3, prioritize 16:9
			if (vResolution169.size() > 0)
			{
				int index = vResolution169.size() / 2;
				reset_resolution = vResolution169.at(index);
				obs_data_set_string(setting, "resolution", reset_resolution.c_str());//Set to canvas resolution
			}
			else if (vResolution43.size() > 0)
			{
				int index = vResolution43.size() / 2;
				reset_resolution = vResolution43.at(index);
				obs_data_set_string(setting, "resolution", reset_resolution.c_str());//Set to canvas resolution
			}
		}

		obs_data_set_bool(setting, "active", true);//Whether to activate

		obs_source_update(source, settings);
		obs_data_release(settings);
	}
	if (find_usb_camera)
	{
		obs_sceneitem_t *item = obs_scene_add(scene, source);
		obs_sceneitem_set_locked(item, true);
	}
	obs_source_release(source);
	return source;
}

static std::wstring string2wstring(std::string str)
{
	wchar_t wszCmd[1000] = { 0 };
	os_utf8_to_wcs(str.c_str(), str.size(), wszCmd, 1000 * 2);
	return std::wstring(wszCmd);
}

//Convert wstring to string
static std::string wstring2string(std::wstring wstr)
{
	std::string result;
	//Get buffer size and allocate space, buffer size is calculated by bytes
	int len = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), wstr.size(), NULL, 0, NULL, NULL);
	char* buffer = new char[len + 1];
	//Convert wide-byte encoding to multi-byte encoding
	WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), wstr.size(), buffer, len, NULL, NULL);
	buffer[len] = '\0';             //Add string terminator
	//Delete buffer and return value
	result.append(buffer);
	delete[] buffer;
	return result;
}

bool lovense_plugins_is_setup()
{
#ifdef DEF_STREAM_MASTER
	return true;
#endif

	char path[512] = { 0 };
	if (os_get_config_path(path, sizeof(path), "obs-studio\\PluginVersion.ini") <= 0)
	{
		blog(LOG_ERROR, "[Lovese] ReadVersionConfig -> obs-studio\\PluginVersion.ini fail !");
		// Configuration file does not exist, no processing needed
		return true;
	}
	std::wstring strAppPath = string2wstring(path);
	TCHAR Result[MAX_PATH] = { 0 };
	GetPrivateProfileString(L"OBS_PLUGIN", L"LovenseAIInit", L"0", Result, MAX_PATH, strAppPath.c_str());
	std::wstring wstrResult = Result;
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
	std::wstring  strAppPath = string2wstring(path);
	WritePrivateProfileString(L"OBS_PLUGIN", L"LovenseAIInit", L"1", strAppPath.c_str());
}


static void ResetLovenseScene(bool setcurrent) {
	obs_scene_t* scene = OBSVideoAdapter::FindLovenseScene();
	bool hasDshow = false;
	bool hasFeedback = false;
	if (scene)
	{
		obs_source_t* source = obs_scene_get_source(scene);
		if (OBSVideoAdapter::CheckSceneAiConfigValid(source,hasDshow,hasFeedback))
		{
			obs_frontend_set_current_scene(source);
			OBSVideoAdapter::MoveFeedbackSourceToTop();
			return;
		}
		else {
			bool isMatchPix = false;
			if (!hasDshow)
			{
				AddActiveCameraSource(scene, isMatchPix);
			}

			if (!hasFeedback)
			{
				OBSVideoAdapter::AddVideoFeedBackSource(scene);
			}
			obs_frontend_set_current_scene(source);
			if (isMatchPix)
				OBSVideoAdapter::TransfromStretchToScreen();
			else
				OBSVideoAdapter::TransfromFixToScreen();
			OBSVideoAdapter::MoveFeedbackSourceToTop();
		}
		//OBSVideoAdapter::RemoveScene(scene);
		return;
	}
	else {
		obs_source_t *source = obs_frontend_get_current_scene();
		obs_source_release(source);
		if (OBSVideoAdapter::CheckSceneAiConfigValid(source, hasDshow, hasFeedback))
		{
			OBSVideoAdapter::MoveFeedbackSourceToTop();
			return;
		}
	}

	obs_scene_t* newScene = OBSVideoAdapter::CreateLovenseScene();
	bool isMatchPix = false;
	AddActiveCameraSource(newScene, isMatchPix);
	OBSVideoAdapter::AddVideoFeedBackSource(newScene);
	if(setcurrent)
		obs_frontend_set_current_scene(obs_scene_get_source(newScene));
	if (isMatchPix)
		OBSVideoAdapter::TransfromStretchToScreen();
	else
		OBSVideoAdapter::TransfromFixToScreen();
	OBSVideoAdapter::MoveFeedbackSourceToTop();
}

void WSResetLovenseScene(void* data, calldata_t* callData)
{
	if (callData)
	{
		g_AiMode = true;
		ResetLovenseScene();
	}
}


static void CheckAiDshowSource() {
	obs_source_t* source = obs_frontend_get_current_scene();
	bool lovenseAiActive = false;
	obs_scene_enum_items(obs_scene_from_source(source), [](obs_scene_t* scene, obs_sceneitem_t* item, void* param)->bool {

		bool* pActive = (bool*)param;
		obs_source_t* source = obs_sceneitem_get_source(item);
#ifdef WIN32
		const char* source_id = obs_source_get_id(source);
		if (strcmp(source_id, "lovense_dshow_input") == 0)
		{
			obs_data_t* setting = obs_source_get_settings(source);
			bool isActive = obs_data_get_bool(setting, "ActivateSuccess");
			obs_data_release(setting);
			if (isActive)
			{
				*pActive = true;
				return false;
			}
		}
#endif
		return true;
		}, &lovenseAiActive);
	obs_source_release(source);
	g_scene_has_ai_source = lovenseAiActive;
	if (!g_scene_has_ai_source)
	{
		std::string emptyData = SendhandleEmptyDetectResult();
		WSClientSendMessage(emptyData);
	}
}

bool obs_module_load(void)
{
	proc_handler_t *handle = obs_get_proc_handler();
	if (handle) {
		proc_handler_add(handle, "void StartAIMode(bool start)", StartAIMode, NULL);
		proc_handler_add(handle, "void GetAIModeStatus()", GetAIModeStatus, NULL);
		proc_handler_add(handle, "void WSResetLovenseScene()", WSResetLovenseScene, NULL);
	}

	// Model needs to be loaded in the main thread
	LoadModel();
	RegisterDShowSource();
	//OBSVideoAdapter::ConnectSourceSignals();

	// Face detection data is forwarded through thread to avoid affecting detection efficiency
	auto callback = [](enum obs_frontend_event event, void* private_data) {
		if (event == OBS_FRONTEND_EVENT_EXIT)
		{
			g_AiMode = false;
			if (g_thread_init)
			{
				g_atomic_msg_start = false;
				{
					std::unique_lock<std::mutex> lock(g_queue_metux);
					g_con.notify_all();
				}
				if (g_handle_thread.joinable())
					g_handle_thread.join();
			}
			OBSVideoAdapter::DisconnectSourceSignals();
			obs_frontend_remove_event_callback((obs_frontend_event_cb)private_data, nullptr);
		}
		else if (event == OBS_FRONTEND_EVENT_SCENE_CHANGED)
		{
			blog(LOG_INFO, "[Debug] scene list change");
		}
		else if (event == OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED)
		{
			blog(LOG_INFO, "[Debug] scene list change");
		}
		else if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_LIST_CHANGED)
		{
			blog(LOG_INFO, "[Debug] scene list change");
		}
		else if (event == OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED)
		{
#ifndef DEF_STREAM_MASTER
			if (g_is_firstRuning)// First startup, activate current camera
			{
				g_is_firstRuning = false;
				obs_source_t* source = obs_frontend_get_current_scene();
				obs_scene_t* scene = obs_scene_from_source(source);
				OBSVideoAdapter::ActiveCurrentSceneCamera(scene);
				obs_source_release(source);
				g_scene_has_ai_source = false;
				CheckAiDshowSource();
				return;
			}
#endif // !DEF_STREAM_MASTER

			

#ifdef DEF_STREAM_MASTER
			CheckAiDshowSource();
			bool detect = OBSVideoAdapter::FindLovenseScene() == nullptr ? false : true;
#else
			bool detect = OBSVideoAdapter::FindLovenseScene()==nullptr ? false : true;
#endif
			if (g_is_finisheLoading && detect)
			{
				obs_source_t* source = obs_frontend_get_current_scene();
				obs_scene_t* scene = obs_scene_from_source(source);
				OBSVideoAdapter::ActiveCurrentSceneCamera(scene);
				obs_source_release(source);
				blog(LOG_INFO, "[Debug] scene selected change");
				g_scene_has_ai_source = false;
				CheckAiDshowSource();
			}

		}
		else if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING)
		{
			g_is_finisheLoading = true;
#define DEF_INIT_LOVENSE false
#if DEF_INIT_LOVENSE
			if (!lovense_plugins_is_setup())// Need to create lovense scene
			{
				ResetLovenseScene(true);
				setup_lovense_plugin_config();
			}
#endif // DEF_INIT_LOENSE

		}
	};
	obs_frontend_add_event_callback(callback, (void*)(obs_frontend_event_cb)callback);
	return true;
}

extern bool release();
void obs_module_unload() {
	g_AiMode = false;
	release();
}
