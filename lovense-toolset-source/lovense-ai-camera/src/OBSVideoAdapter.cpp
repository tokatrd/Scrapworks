#include "OBSVideoAdapter.h"
#include "obs.h"
#include "obs.hpp"
#include "libdshowcapture/dshowcapture.hpp"

#include <obs-frontend-api.h>
#include <util/config-file.h>
#include <util/dstr.hpp>
#include <list>
#include <windows.h> 

using namespace OBSVideoAdapter;


#define DEF_OBS_DESHOW_ID "dshow_input"
#define DEF_LOVENSE_DESHOW_ID "lovense_dshow_input"
#ifdef DEF_STREAM_MASTER
#define DEF_FEEDBACK_SOURCE_ID "lovense_video_freedback"
#else
#define DEF_FEEDBACK_SOURCE_ID "Lovese_browser_source"
#endif

bool OBSVideoAdapter::IsLovenseAICameraSource( obs_source_t* source)
{
	bool ret = false;
	const char *source_id = obs_source_get_id(source);
	if (source_id) {
		if (strcmp(source_id, DEF_LOVENSE_DESHOW_ID) == 0)
			return true;
	}

	return ret;
}

static bool IsDshowSource(const char *id){
	return (strcmp(id,DEF_OBS_DESHOW_ID) == 0 || strcmp(id,DEF_LOVENSE_DESHOW_ID) == 0);
}

static bool IsRatio16_9(const size_t& x, const size_t& y) {

	if (y <= 0)
		return false;
	if (fabs(x / (float)y - 16.0 / 9.0) < std::numeric_limits<float>::epsilon())
	{
		return true;
	}
	return false;
};

static bool IsRatio4_3(const size_t& x, const size_t& y){

	if (y <= 0)
		return false;
	if (fabs(x / (float)y - 4.0 / 3.0) < std::numeric_limits<float>::epsilon())
	{
		return true;
	}
	return false;
};


bool OBSVideoAdapter::GetCameraResolution( obs_source_t*source, std::map<Ratio, std::vector<std::string>>& resolution)
{
	bool ret = false;
	obs_properties_t* properties = obs_source_properties(source);

	std::vector<std::string> vResolution169; //16:9
	std::vector<std::string> vResolution43;  //4:3
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
					sscanf(szResolution, "%dx%d", &nx, &ny);
					if (IsRatio16_9(nx, ny))
					{
						vResolution169.push_back(szResolution);
					}
					else if (IsRatio4_3(nx, ny))
					{
						vResolution43.push_back(szResolution);
					}
				}
				break;
			}
			obs_property_next(&item);
		}
	}
	
	if(vResolution169.size() > 0)
		resolution[RATIO_16_9] = vResolution169;
	if(vResolution43.size() > 0)
		resolution[RATIO_4_3] = vResolution43;
	return ret;
}

bool OBSVideoAdapter::ResetCameraResolution( obs_source_t* source, const std::string& resoulution)
{
	bool ret = false;
	obs_data_t* setting = obs_source_get_settings(source);
	obs_data_set_int(setting, "res_type", 1);
	obs_data_set_string(setting, "resolution", resoulution.c_str());//����Ϊ�����ķֱ���
	obs_source_update(source, setting);
	obs_data_release(setting);
	return ret;
}

void OBSVideoAdapter::SetCameraResolutionMatchCanvas( obs_source_t* source) {
	
	std::map< Ratio, std::vector< std::string>> mapRes;
	GetCameraResolution(source, mapRes);
	if (mapRes.size() > 0)
	{
		config_t* config = obs_frontend_get_profile_config();
		uint32_t nx = config_get_uint(config, "Video", "BaseCX");
		uint32_t ny = config_get_uint(config, "Video", "BaseCY");

		Ratio baseRatio = RATIO_16_9;
		if (IsRatio16_9(nx, ny))
			baseRatio = RATIO_16_9;
		else if (IsRatio4_3(nx, ny))
			baseRatio = RATIO_4_3;
	    std::vector <std::string>& machLists = mapRes[baseRatio];
		if (machLists.size() > 2)
		{
			ResetCameraResolution(source, machLists.at(1));//���õڶ�ƥ��ֱ��ʣ���һ�ֱ���̫��
		}
		else if(machLists.size() == 1)
			ResetCameraResolution(source, machLists.at(0));
	}
}

template <typename T> T* calldata_get_pointer(const calldata_t* data, const char* name) {
	void* ptr = nullptr;
	calldata_get_ptr(data, name, &ptr);
	return reinterpret_cast<T*>(ptr);
}


static void OnSourceCreate(void* param, calldata_t* data) {
	OBSSource source = calldata_get_pointer<obs_source_t>(data, "source");
	if (!source) {
		return;
	}
	const char* source_id = obs_source_get_id(source);
	if (source_id && strcmp(source_id, DEF_LOVENSE_DESHOW_ID) == 0)
	{
		//cita: ֻ��OBS������ʱ�����÷ֱ�����Ч��ͨ��UI����Դ��Ч
		//SetCameraResolutionMatchCanvas(source);
		//OBSVideoAdapter::MoveFeedbackSourceToTop();
	}
}

static void OnSourceRemove(void* param, calldata_t* data) {
	OBSSource source = calldata_get_pointer<obs_source_t>(data, "source");
	if (!source) {
		return;
	}
	const char* source_id = obs_source_get_id(source);
	if (source_id && strcmp(source_id, DEF_LOVENSE_DESHOW_ID) == 0)
	{
		//cita: ֻ��OBS������ʱ�����÷ֱ�����Ч��ͨ��UI����Դ��Ч
		//SetCameraResolutionMatchCanvas(source);
		//OBSVideoAdapter::MoveFeedbackSourceToTop();
	}
}


void OBSVideoAdapter::ConnectSourceSignals()
{
	signal_handler_t* coreSignalHandler = obs_get_signal_handler();
	if (coreSignalHandler) {
		signal_handler_connect(coreSignalHandler, "source_create", OnSourceCreate, nullptr);
		//"source_remove"ֻ��OBS�رյ�ʱ��ŵ���
		signal_handler_connect(coreSignalHandler, "source_destroy", OnSourceRemove, nullptr);
	}
}

void OBSVideoAdapter::DisconnectSourceSignals()
{
	signal_handler_t* coreSignalHandler = obs_get_signal_handler();
	if (coreSignalHandler) {
		signal_handler_disconnect(coreSignalHandler, "source_create", OnSourceCreate, nullptr);
		signal_handler_disconnect(coreSignalHandler, "source_destroy", OnSourceRemove, nullptr);

	}
}

void EnumCameraDevice(const std::wstring& device_name, std::map<Ratio, std::vector<std::string>>& resolution)
{
	std::vector<DShow::VideoDevice> vdevices;
	DShow::OBSDevice::EnumVideoDevices(vdevices);


	for (const DShow::VideoDevice& item : vdevices) {
		if (item.name.compare(device_name) == 0)
		{
			std::vector<std::string> vResolution169; //16:9
			std::vector<std::string> vResolution43;  //4:3
			for (const DShow::VideoInfo& info : item.caps) {
				char szRes[20] = { 0 };
				if (IsRatio16_9(info.maxCX, info.maxCY))
				{
					sprintf_s(szRes, sizeof(szRes), "%dx%d", info.maxCX, info.maxCY);
					vResolution169.push_back(szRes);
				}
				else if (IsRatio4_3(info.maxCX, info.maxCY))
				{
					sprintf_s(szRes, sizeof(szRes), "%dx%d", info.maxCX, info.maxCY);
					vResolution43.push_back(szRes);
				}

				if (IsRatio16_9(info.minCX, info.minCY))
				{
					sprintf_s(szRes, sizeof(szRes), "%dx%d", info.minCX, info.minCY);
					vResolution169.push_back(szRes);
				}
				else if (IsRatio4_3(info.minCX, info.minCY))
				{
					sprintf_s(szRes, sizeof(szRes), "%dx%d", info.minCX, info.minCY);
					vResolution43.push_back(szRes);
				}

			}
		}
	}
}

static bool vcam_CenterAlignSelectedItems(obs_scene_t* scene, obs_sceneitem_t* item, void* param)
{

	obs_source_t* source = obs_sceneitem_get_source(item);
	if (!source)
		return true;
	else {
		const char* szID = obs_source_get_id(source);
		if (strcmp(szID, DEF_LOVENSE_DESHOW_ID) != 0)
		{
			return true;
		}
	}
	obs_bounds_type boundsType = *reinterpret_cast<obs_bounds_type*>(param);

	//if (obs_sceneitem_is_group(item))
		//obs_sceneitem_group_enum_items(item, CenterAlignSelectedItems, param);

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

	obs_sceneitem_set_info(item, &itemInfo);

	UNUSED_PARAMETER(scene);
	return true;
}

void OBSVideoAdapter::TransfromStretchToScreen()
{
	obs_source_t* source = obs_frontend_get_current_scene();
	obs_scene_t* current_scene = obs_scene_from_source(source);
	obs_bounds_type boundsType = OBS_BOUNDS_SCALE_INNER; 
	obs_scene_enum_items(current_scene, vcam_CenterAlignSelectedItems, &boundsType);
	obs_source_release(source);
}

void OBSVideoAdapter::TransfromFixToScreen()
{
	obs_source_t* source = obs_frontend_get_current_scene();
	obs_scene_t* current_scene = obs_scene_from_source(source);
	obs_bounds_type boundsType = OBS_BOUNDS_STRETCH; 
	obs_scene_enum_items(current_scene, vcam_CenterAlignSelectedItems, &boundsType);
	obs_source_release(source);
}


void OBSVideoAdapter::MoveFeedbackSourceToTop()
{
	obs_source_t* source = obs_frontend_get_current_scene();
	obs_scene_t* current_scene = obs_scene_from_source(source);
	obs_sceneitem_t* item = nullptr;
	auto ItemEnumFunc = [](obs_scene_t* scene, obs_sceneitem_t* item, void* param)->bool {
		obs_source_t* source = obs_sceneitem_get_source(item);
		if (!source)
			return true;
		else {
			const char* szID = obs_source_get_id(source);
			if (strcmp(szID, DEF_FEEDBACK_SOURCE_ID) == 0)
			{
				obs_sceneitem_set_order(item, OBS_ORDER_MOVE_TOP);
				return false;
			}
		}
		return true;
	};
	obs_scene_enum_items(current_scene, ItemEnumFunc, &item);
	obs_source_release(source);
}

obs_scene_t *OBSVideoAdapter::FindLovenseScene(){
	obs_frontend_source_list sceneList = {};
	obs_frontend_get_scenes(&sceneList);
	obs_scene_t* scene = nullptr;
	for (size_t i = 0; i < sceneList.sources.num; i++) {
		obs_source_t* scene_source = sceneList.sources.array[i];
		obs_data_t* setting = obs_source_get_settings(scene_source);
		if (setting) {
			const char * szType = obs_data_get_string(setting, "CustomType");
			obs_data_release(setting);
			if (strcmp(szType, "Lovense") == 0)
			{
				scene = obs_scene_from_source(scene_source);
				break;
			}
		}
	}
	obs_frontend_source_list_free(&sceneList);
	return scene;
}

static std::string GetNewSceneName(const char* baseName)
{
	if (!baseName || strlen(baseName) == 0)
		return "";
	obs_source_t* scene_source = nullptr;
	std::string strRealName = baseName;
	int i = 1;
	while ((scene_source = obs_get_source_by_name(strRealName.c_str()))) {
		obs_source_release(scene_source);
		strRealName = baseName;
		strRealName += " (";
		strRealName += std::to_string(i++);
		strRealName += ")";
	}
	return strRealName;
}

obs_scene_t *OBSVideoAdapter::CreateLovenseScene()
{
	obs_scene_t *newscene = nullptr;
	std::string scenename = GetNewSceneName("Face Filter");
	if (scenename.empty())
		return newscene;
	newscene = obs_scene_create(scenename.c_str());
	if (newscene)
	{
		obs_source_t* source = obs_scene_get_source(newscene);
		if (source)
		{
			obs_data_t* setting = obs_source_get_settings(source);
			if (!setting)
				setting = obs_data_create();
			obs_data_set_string(setting, "CustomType", "Lovense");
			obs_data_release(setting);
			obs_source_release(source);
		}
	}
	return newscene;
}


static bool CheckIsPhysicalCameraID(const char* szID) {
	if (!szID || strlen(szID) == 0)
		return false;
	std::string strID = szID;
	if (strID.find("usb") != std::string::npos)
		return true;
	return false;
}

static inline void encode_dstr(struct dstr* str)
{
	dstr_replace(str, "#", "#22");
	dstr_replace(str, ":", "#3A");
}

// Get physical camera list and device IDs
std::vector<std::string> OBSVideoAdapter::GetPhysicalCameraList()
{
	// Method 1: Use interface to get devices
	std::vector<std::string> devices;
	std::vector<DShow::VideoDevice> videoDevices;
	DShow::OBSDevice::EnumVideoDevices(videoDevices);
	for (auto& item : videoDevices)
	{
		DStr name, path, device_id;
		dstr_from_wcs(name, item.name.c_str());
		dstr_from_wcs(path, item.path.c_str());
		encode_dstr(path);
		dstr_copy_dstr(device_id, name);
		encode_dstr(device_id);
		dstr_cat(device_id, ":");
		dstr_cat_dstr(device_id, path);
		if (CheckIsPhysicalCameraID(device_id))
		{
			devices.push_back(std::string(device_id));
		}
	}

	
	return devices;
}

static bool scene_enum_func(obs_scene_t* scene, obs_sceneitem_t* item, void* param)
{
	bool* pActive = (bool*)param;
	obs_source_t* source = obs_sceneitem_get_source(item);
#ifdef WIN32
	const char* source_id = obs_source_get_id(source);
	if (source_id && (strcmp(source_id, DEF_OBS_DESHOW_ID) == 0 || strcmp(source_id, DEF_LOVENSE_DESHOW_ID) == 0))
	{
		obs_data_t* setting = obs_source_get_settings(source);
		//obs_data_set_bool(setting, "active", *pActive);
		//obs_data_set_bool(setting, "synchronous_activate", true);
		bool isActive = obs_data_get_bool(setting,"active");
		obs_data_set_bool(setting, "deactivate_when_not_showing", true);
		obs_source_update(source, setting);
		if (*pActive)
		{
			obs_data_set_bool(setting, "synchronous_activate", true);
			obs_source_update(source, setting);
			obs_source_inc_showing(source);
		}
		else
			obs_source_dec_showing(source);
		obs_data_release(setting);

		proc_handler_t* proc = obs_source_get_proc_handler(source);
		if (proc)
		{
			calldata_t* data = calldata_create();
			calldata_init(data);
			calldata_set_bool(data, "active", *pActive);
			proc_handler_call(proc, "activate", data);
			calldata_destroy(data);
		}
		
		return false;
	}
#endif
}
bool OBSVideoAdapter::ActiveCurrentSceneCamera(obs_scene_t* current_scene)
{
	bool hasDshow = HasDshowSource(current_scene);
#ifdef DEF_STREAM_MASTER1
	if (!hasDshow)
		return false;
#endif

	bool ret = false;
	obs_frontend_source_list sceneList = {};
	obs_frontend_get_scenes(&sceneList);
	bool active = false;
	for (size_t i = 0; i < sceneList.sources.num; i++) {
		obs_source_t* scene_source = sceneList.sources.array[i];
		obs_scene_t* sceneItem = obs_scene_from_source(scene_source);
		if (sceneItem != current_scene)
			obs_scene_enum_items(sceneItem, scene_enum_func, &active);
	}
	obs_frontend_source_list_free(&sceneList);

	if (!hasDshow)
		return ret;

	bool lovenseAiActive = false;
	obs_scene_enum_items(current_scene, [](obs_scene_t* scene, obs_sceneitem_t* item, void* param)->bool {
		
		bool* pActive = (bool*)param;
		obs_source_t* source = obs_sceneitem_get_source(item);
#ifdef WIN32
		const char* source_id = obs_source_get_id(source);
		if (strcmp(source_id, DEF_LOVENSE_DESHOW_ID) == 0)
		{
			obs_data_t* setting = obs_source_get_settings(source);
			bool isActive = obs_data_get_bool(setting,"ActivateSuccess");
			obs_data_release(setting);
			if (isActive)
			{
				*pActive = true;
				return false;
			}
		}
#endif
		return true;
	},&lovenseAiActive);

	if (!lovenseAiActive)
	{
		Sleep(800);
		active = true;
		obs_scene_enum_items(current_scene, scene_enum_func, &active);
	}
	return ret;
}

bool OBSVideoAdapter::RemoveScene(obs_scene_t* scene) {
	bool ret = false;
	if (!scene)
		return ret;

	obs_source_t* source = obs_scene_get_source(scene);
	if (source)
	{
		obs_source_remove(source);//ֱ���Ƴ�
	}
	return ret;
}

bool scene_item_enumter(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	int ret = false;
	obs_source_t* source = obs_sceneitem_get_source(item);
	if (source) {
		const char* sid = obs_source_get_id(source);
		if (strcmp(sid, DEF_OBS_DESHOW_ID) == 0 || strcmp(sid, DEF_LOVENSE_DESHOW_ID) == 0)
		{
			obs_data_t * setting = obs_source_get_settings(source);
			if (setting)
			{
				bool isActive =  obs_data_get_bool(setting, "active");
			}
		}
	}

	return true;
}

std::vector<std::string> OBSVideoAdapter::GetNotActivedCameraList()
{
	std::vector<std::string> devices;

	obs_frontend_source_list sceneList = {};
	obs_frontend_get_scenes(&sceneList);
	obs_data_array_t* scenes = obs_data_array_create();
	obs_scene_t* scene = nullptr;
	for (size_t i = 0; i < sceneList.sources.num; i++) {
		obs_source_t *scene_source = sceneList.sources.array[i];
		obs_scene_enum_items(obs_scene_from_source(scene_source), scene_item_enumter,nullptr);
	}
	obs_frontend_source_list_free(&sceneList);
	return devices;
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

obs_source_t* OBSVideoAdapter::AddVideoFeedBackSource(obs_scene_t* scene)
{
	obs_source_t* source = obs_source_create(DEF_FEEDBACK_SOURCE_ID, get_new_source_name("Lovense Video Feedback"), nullptr, nullptr);
	//������������
	//obs_source_set_monitoring_type(source, OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT);
	obs_sceneitem_t *item = obs_scene_add(scene, source);
	obs_sceneitem_set_locked(item, true);
	obs_source_release(source);
	return source;
}

static std::vector<std::string> g_tmp_source_id;
bool OBSVideoAdapter::CheckSceneAiConfigValid(obs_source_t* scenesource, bool& hasDshow, bool& hasFeedback) {

	bool validconfig = false;
	g_tmp_source_id.clear();
	obs_source_enum_active_sources(scenesource, [](obs_source_t* parent,
		obs_source_t* child, void* param)->void {
			const char* id = obs_source_get_id(child);
			g_tmp_source_id.push_back(id);
		}, nullptr);

	if (g_tmp_source_id.empty())
		return false;

	//check contain 
    std::list<std::string> config = { "lovense_dshow_input",DEF_FEEDBACK_SOURCE_ID };
	for (auto& item : g_tmp_source_id)
	{
		if (config.front() == item)
			config.pop_front();
	}
	if (config.empty())
		validconfig = true;


	//check sort:��Դ������˳�򲻶ԣ���Ҫ����
	for (auto& item : g_tmp_source_id)
	{
		if (strcmp(item.c_str(), "lovense_dshow_input") == 0)
			hasDshow = true;
		if (strcmp(item.c_str(), DEF_FEEDBACK_SOURCE_ID) == 0)
			hasFeedback = true;
	}

	validconfig = hasDshow && hasFeedback;
	return validconfig;
}

bool OBSVideoAdapter::HasDshowSource(obs_scene_t* scene)
{
	bool ret = false;
	obs_scene_enum_items(scene, [](obs_scene_t* scene,
		obs_sceneitem_t* item, void* param)->bool {
			obs_source_t* source = obs_sceneitem_get_source(item);
			const char* id = obs_source_get_id(source);
			if (strcmp(id, DEF_LOVENSE_DESHOW_ID) == 0 || strcmp(id, DEF_OBS_DESHOW_ID) == 0)
			{
				bool* result = (bool*)param;
				*result = true;
				return false;
			}
			return true;
		}, &ret);

	return ret;
}