#include "UntilHelp.h"
#include <obs-frontend-api.h>
#include <obs.hpp>
#include <util/util.hpp>
#include "lovense_obs_source.hpp"
#include "lovense_obs_common.hpp"
#include "lovense_string_helper.hpp"

#ifdef WIN32
#define CAMERA_SOURCE_ID "dshow_input"
#elif defined(__APPLE__)
#include "macResouce.h"
#include "CJsonObject.hpp"
#define CAMERA_SOURCE_ID "av_capture_input"
#define DEF_DSHOW_SOURCE_ID_2 "av_capture_input_v2"
#else
#include "CJsonObject.hpp"
#define CAMERA_SOURCE_ID "v4l2_input"
#define DEF_DSHOW_SOURCE_ID_2 "v4l2_input"
#endif
#define AI_CAMERA_SOURCE_ID "lovense_dshow_input"
static std::string m_pluginPath;

namespace UntilRes {

	
	void SetPluginPath(std::string path) {
		m_pluginPath = path;
	}

	std::string GetPluginPath() {
		return m_pluginPath;
	}

	std::string GetResourceImage(std::string name) {
#ifdef _WIN32
		std::string path = m_pluginPath;
		path += "/images/";
		path += name;
		return path;
#elif defined(__APPLE__)
        return GetBoundleResoucePath(name);
#else
		// Linux: use plugin path like Windows
		std::string path = m_pluginPath;
		path += "/images/";
		path += name;
		return path;
#endif
	}

	static bool IsRatio16_9(const size_t& x, const size_t& y) {

		if (y <= 0)
			return false;
		if (fabs(x / (float)y - 16.0 / 9.0) < 0.05)
		{
			return true;
		}
		return false;
	}

	static bool IsRatio4_3(const size_t& x, const size_t& y) {

		if (y <= 0)
			return false;
		if (fabs(x / (float)y - 4.0 / 3.0) < 0.05)
		{
			return true;
		}
		return false;
	}

	size_t GetCameraResolution(obs_source_t* source, std::map<Ratio, std::vector<std::string>>& resolution) {
		obs_properties_t* properties = obs_source_properties(source);

		std::vector<std::string> vResolution169; //16:9
		std::vector<std::string> vResolution43;  //4:3
		std::vector<std::string> vResolutionOthers;  //4:3

		if (properties)
		{
			obs_property_t* item = obs_properties_first(properties);
			while (item)
			{
				const char* p_name = obs_property_name(item);
				if (strcmp(p_name, "resolution") == 0)
				{
					size_t count = obs_property_list_item_count(item);
					size_t nx = 0, ny = 0;
					for (int i = 0; i < count; i++)
					{
						const char* szResolution = obs_property_list_item_name(item, i);
						sscanf(szResolution, "%dx%d", (int*)&nx,(int*)&ny);
						if (IsRatio16_9(nx, ny))
						{
							vResolution169.push_back(szResolution);
						}
						else if (IsRatio4_3(nx, ny))
						{
							vResolution43.push_back(szResolution);
						}
						else
							vResolutionOthers.push_back(szResolution);

					}
					break;
				}
				obs_property_next(&item);
			}

			obs_properties_destroy(properties);
		}

		if (vResolution169.size() > 0)
			resolution[RATIO_16_9] = vResolution169;
		if (vResolution43.size() > 0)
			resolution[RATIO_4_3] = vResolution43;
		if(vResolutionOthers.size())
			resolution[RATIO_OTHERS] = vResolutionOthers;
		return resolution.size();
	}

	std::string GetResolutionTypeString(size_t x, size_t y)
	{
		size_t n_x{ 0 }, n_y{ 0 };
		bool normal{ true };
		if (x >= y) {
			n_x = x;
			n_y = y;
		}
		else {
			n_x = y;
			n_y = x;
			normal = false;
		}

		if (IsRatio16_9(n_x, n_y)) {
			return normal ? "16:9" : "9:16";
		}
		else if (IsRatio4_3(n_x, n_y)) {
			return normal ? "4:3" : "3:4";
		}

		return "";
	}

	bool ResetCameraResolution(obs_source_t* source, const std::string& resoulution) {
        OBSDataAutoRelease  setting = obs_source_get_settings(source);
#ifdef _WIN32
		obs_data_set_int(setting, "res_type", 1);
		const char* res = obs_data_get_string(setting, "resolution");
		if (strcmp(res, resoulution.c_str()) == 0)
			return false;
		obs_data_set_string(setting, "resolution", resoulution.c_str());//ÉèÖÃÎª»­²¼µÄ·Ö±æÂ
#else
        bool isPreReset = obs_data_get_bool(setting, "use_preset");
        if(isPreReset){
            //"preset": "AVCaptureSessionPreset3840x2160",
            obs_data_set_bool(setting, "use_preset", false);
        }
      
        OBSDataAutoRelease  oldFrameData = obs_data_get_obj(setting, "frame_rate");
        if(!oldFrameData){
            OBSData frameData = obs_data_create();
            obs_data_set_double(frameData, "denominator", 1.0f);
            obs_data_set_double(frameData, "numerator", 30.0f);
            obs_data_set_obj(setting, "frame_rate", frameData);
        }

        int nx,ny;
        sscanf(resoulution.c_str(), "%dx%d", &nx, &ny);
        neb::CJsonObject resJson;
        resJson.Add("height", ny);
        resJson.Add("width", nx);
        obs_data_set_string(setting, "resolution", resJson.ToString().c_str());
        
#endif
        
		obs_source_update(source, setting);
		return true;
	}

	obs_source_t* FindCurrentDshowSource() {
		obs_source_t *current_scene_source = obs_frontend_get_current_scene();
		obs_scene_t* current_scene = obs_scene_from_source(current_scene_source);
		OBSSource dshow = nullptr;
		obs_scene_enum_items(current_scene, [](obs_scene_t* scene, obs_sceneitem_t* item, void* param)->bool {
			obs_source_t *source = obs_sceneitem_get_source(item);
			const char *source_id = obs_source_get_id(source);

			UntilRes::CameraBaseInfo info = UntilRes::GetCameraSourceBaseInfo(source);
			if (!info.name.empty() &&
				(info.name.compare("Lovense Camera") == 0 ||
					info.name.compare("Lovense Webcam") == 0))
			{
				OBSSource* pSource = (OBSSource*)param;
				*pSource = source;
				return false;
			}		   

			//Standard camera
#ifdef WIN32
			if (strcmp(source_id, CAMERA_SOURCE_ID) == 0) {
#else
			if (strcmp(source_id, CAMERA_SOURCE_ID) == 0 || strcmp(source_id, DEF_DSHOW_SOURCE_ID_2) == 0) {
#endif
				OBSSource* pSource = (OBSSource *)param;
				*pSource = source;
			}



			return true;
		}, & dshow);
		obs_source_release(current_scene_source);
		return dshow;
	}

	CameraBaseInfo GetCameraSourceBaseInfo(obs_source_t* source) {
		CameraBaseInfo info;
		assert(source);
        OBSDataAutoRelease settings = obs_source_get_settings(source);
#ifdef _WIN32
		//ID format:HD Pro Webcam C920:\\\\?\\usb#22vid_046d&pid_082d&mi_00#226&19804bbe&0&0000#22{65
		const char *szDeviceID = obs_data_get_string(settings, "video_device_id");
		if (szDeviceID) {
			std::string strID = szDeviceID;
			size_t pos = strID.find_first_of(':');
			if (pos != std::string::npos) {
				info.name = strID.substr(0,pos);
				info.path = strID;
				const char* res = obs_data_get_string(settings, "resolution");
				info.resolution = res;
			}
		}
#elif defined(__APPLE__)
        const char *name = obs_data_get_string(settings,"device_name");
        if(name)
            info.name = name;
        bool use_preset = obs_data_get_bool(settings, "use_preset");
        if(!use_preset){
            obs_data_t *framerate = obs_data_get_obj(settings,"frame_rate");
            if(framerate){
                int numerator = obs_data_get_int(framerate,"numerator");
                //info. = numerator;
                obs_data_release(framerate);
            }
            const char *resolution = obs_data_get_string(settings,"resolution");
            if(resolution){
                //"{\"height\":720,\"width\":1280}"
                std::string err;
                //json11::Json root = json11::Json::parse(resolution,err);
                neb::CJsonObject root;
                if(root.Parse(resolution)){
                    int height = 0;
                    int width = 0;
                    root.Get("height", height);
                    root.Get("width", width);
                    char buff[20] = {0};
                    sprintf(buff,"%dx%d",width,height);
                    info.resolution = buff;
                }
            }
        }
#else
        // Linux (V4L2)
        const char *device_id = obs_data_get_string(settings, "video_device_id");
        if (device_id) {
            info.path = device_id;
            // Extract device name from path (e.g., /dev/video0 -> video0)
            std::string path(device_id);
            size_t pos = path.find_last_of('/');
            if (pos != std::string::npos) {
                info.name = path.substr(pos + 1);
            }
        }
        const char *resolution = obs_data_get_string(settings, "resolution");
        if (resolution) {
            info.resolution = resolution;
        }
#endif
		return info;
	}

	void TransfromSizeToScreen(obs_scene_t* scene, obs_source_t* source) {
		obs_scene_enum_items(scene, [](obs_scene_t* scene, obs_sceneitem_t* item, void* param)->bool {

			obs_source_t* source = (obs_source_t*)param;
			obs_source_t* itemsource = obs_sceneitem_get_source(item);
			if (source == itemsource) {
				obs_bounds_type boundsType = OBS_BOUNDS_SCALE_INNER;

				obs_video_info ovi;
				obs_get_video_info(&ovi);

				obs_transform_info itemInfo;
				vec2_set(&itemInfo.pos, 0.0f, 0.0f);
				vec2_set(&itemInfo.scale, 1.0f, 1.0f);
				itemInfo.alignment = OBS_ALIGN_LEFT | OBS_ALIGN_TOP;
				itemInfo.rot = 0.0f;

				vec2_set(&itemInfo.bounds, float(ovi.base_width), float(ovi.base_height));

				itemInfo.bounds_type = boundsType;
				itemInfo.bounds_alignment = OBS_ALIGN_CENTER;
				//obs_sceneitem_get_source;
				obs_sceneitem_set_info(item, &itemInfo);
				return false;
			}
			return true;
		}, source);

	}


	void TransfromFixToScreen(obs_scene_t *scene, obs_source_t *source)
	{
		obs_scene_enum_items(scene, [](obs_scene_t* scene, obs_sceneitem_t* item, void* param)->bool {
			
			obs_source_t *source = (obs_source_t*)param;
			obs_source_t* itemsource = obs_sceneitem_get_source(item);
			if (source == itemsource) {
				obs_bounds_type boundsType = OBS_BOUNDS_STRETCH;

				obs_video_info ovi;
				obs_get_video_info(&ovi);

				obs_transform_info itemInfo;
				vec2_set(&itemInfo.pos, 0.0f, 0.0f);
				vec2_set(&itemInfo.scale, 1.0f, 1.0f);
				itemInfo.alignment = OBS_ALIGN_LEFT | OBS_ALIGN_TOP;
				itemInfo.rot = 0.0f;

				vec2_set(&itemInfo.bounds, float(ovi.base_width), float(ovi.base_height));

				itemInfo.bounds_type = boundsType;
				itemInfo.bounds_alignment = OBS_ALIGN_CENTER;
				//obs_sceneitem_get_source;
				obs_sceneitem_set_info(item, &itemInfo);
				return false;
			}
			return true;
		}, source);
	}

	void ActiveCameraDeviceSource(obs_source_t* source) {
		if (!source)
			return;
		proc_handler_t* proc = obs_source_get_proc_handler(source);
		if (proc)
		{
			calldata_t* data = calldata_create();
			calldata_init(data);
			calldata_set_bool(data, "active", true);
			proc_handler_call(proc, "activate", data);
			if (true)
				proc_handler_call(proc, "activate", data);
			calldata_destroy(data);

		}
	}

#if 0
	bool OBSSourceScreenResolution(bool vertical) {
		
		bool success = false;
		obs_source_t* source = UntilRes::FindCurrentDshowSource();
		assert(source);
		if (source) {
			std::map<Ratio, std::vector<std::string>> resMap;
			GetCameraResolution(source, resMap);
			if (resMap.size() > 0)
			{
				std::string resolution;
				if (UntilRes::ResetCameraResolution(source, resolution)) {
					int nx = 0, ny = 0;
					sscanf(resolution.c_str(), "%dx%d", &nx, &ny);
					if (nx > ny) {
						obs_source_t* scenesource = obs_frontend_get_current_scene();
						UntilRes::TransfromFixToScreen(obs_scene_from_source(scenesource), source);
						obs_source_release(scenesource);
					}
					else {
						obs_source_t* scenesource = obs_frontend_get_current_scene();
						UntilRes::TransfromSizeToScreen(obs_scene_from_source(scenesource), source);
						obs_source_release(scenesource);
					}
				}
			}
		}
		return success;
	}
#endif

	LiveMode GetLiveMode() {
		config_t* config = obs_frontend_get_profile_config();
		if (config) {
			uint32_t cx = (uint32_t)config_get_uint(config, "Video", "BaseCX");
			uint32_t cy = (uint32_t)config_get_uint(config, "Video", "BaseCY");
			if (cx > cy)
				return LiveMode_Horizontal;
			else
				return LiveMode_Vertical;
		}
		return LiveMode_Horizontal;
	}

	bool SetLiveMode(LiveMode mode) {
		//The following three modes cannot switch screen orientation
		bool streamming = obs_frontend_streaming_active();
		bool recording = obs_frontend_recording_active();
		bool virtualcaming = obs_frontend_virtualcam_active();
		if (streamming || recording || virtualcaming)
			return false;

		if (mode == GetLiveMode())
		{
			return true;
		}

		/*
		config_set_uint(basicConfig, "Video", "BaseCX", last_cx);
		config_set_uint(basicConfig, "Video", "BaseCY", last_cy);
		config_set_uint(basicConfig, "Video", "OutputCX", last_cx);
		config_set_uint(basicConfig, "Video", "OutputCY", last_cy);
		config_save_safe(basicConfig, "tmp", nullptr);
		obs_frontend_reset_video();
		*/
		uint32_t baseCX, baseCY, OutputCX, OutputCY;
		std::string resoulution;
		switch (mode)
		{
		case UntilRes::LiveMode_Horizontal: {
			baseCX = 1920;
			baseCY = 1080;
			OutputCX = 1280;
			OutputCY = 720;
			resoulution = "1920x1080";
		}break;
		case UntilRes::LiveMode_Vertical: {
			baseCX = 1080;
			baseCY = 1920;
			OutputCX = 720;
			OutputCY = 1280;
			resoulution = "1088x1920";
		}break;
		default:
			break;
		}
		config_t* basicConfig = obs_frontend_get_profile_config();
		if (basicConfig) {
			config_set_uint(basicConfig, "Video", "BaseCX", baseCX);
			config_set_uint(basicConfig, "Video", "BaseCY", baseCY);
			config_set_uint(basicConfig, "Video", "OutputCX", OutputCX);
			config_set_uint(basicConfig, "Video", "OutputCY", OutputCY);
			config_save_safe(basicConfig, "tmp", nullptr);
			obs_frontend_reset_video();
			obs_source_t *source =  FindCurrentDshowSource();
			if (source) {
				ResetCameraResolution(source, resoulution);

#if 1 
				obs_source_t* current_scene_source = obs_frontend_get_current_scene();
				TransfromSizeToScreen(obs_scene_from_source(current_scene_source), source);
				obs_source_release(current_scene_source);
#endif

#if 0
				lovense::ObsSource source_obj;
				if (source_obj) {
					source_obj.TransfromSizeToScreen(nullptr, OBS_ALIGN_LEFT | OBS_ALIGN_TOP);

					const char* source_id = obs_source_get_id(source);
					if (source_id) {
						source_obj.TransfromSizeToScreen(source_id, OBS_ALIGN_CENTER);
					}
				}
#endif
				
			}
		}
		return true;
	}

}
