#include "Utils.h"

#include "WSRequestHandler.h"

/**
 * Indicates if Studio Mode is currently enabled.
 *
 * @return {boolean} `studio-mode` Indicates if Studio Mode is enabled.
 *
 * @api requests
 * @name GetStudioModeStatus
 * @category studio mode
 * @since 4.1.0
 */
RpcResponse WSRequestHandler::GetStudioModeStatus(const RpcRequest& request) {
	bool previewActive = obs_frontend_preview_program_mode_active();

	OBSDataAutoRelease response = obs_data_create();
	obs_data_set_bool(response, "studio-mode", previewActive);

	return request.success(response);
}

/**
 * Get the name of the currently previewed scene and its list of sources.
 * Will return an `error` if Studio Mode is not enabled.
 *
 * @return {String} `name` The name of the active preview scene.
 * @return {Array<SceneItem>} `sources`
 *
 * @api requests
 * @name GetPreviewScene
 * @category studio mode
 * @since 4.1.0
 */
RpcResponse WSRequestHandler::GetPreviewScene(const RpcRequest& request) {
	if (!obs_frontend_preview_program_mode_active()) {
		return request.failed("studio mode not enabled");
	}

	OBSSourceAutoRelease scene = obs_frontend_get_current_preview_scene();
	OBSDataArrayAutoRelease sceneItems = Utils::GetSceneItems(scene);

	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_string(data, "name", obs_source_get_name(scene));
	obs_data_set_array(data, "sources", sceneItems);

	return request.success(data);
}

/**
 * Set the active preview scene.
 * Will return an `error` if Studio Mode is not enabled.
 *
 * @param {String} `scene-name` The name of the scene to preview.
 *
 * @api requests
 * @name SetPreviewScene
 * @category studio mode
 * @since 4.1.0
 */
RpcResponse WSRequestHandler::SetPreviewScene(const RpcRequest& request) {
	if (!obs_frontend_preview_program_mode_active()) {
		return request.failed("studio mode not enabled");
	}

	if (!request.hasField("scene-name")) {
		return request.failed("missing request parameters");
	}

	const char* scene_name = obs_data_get_string(request.parameters(), "scene-name");
	OBSScene scene = Utils::GetSceneFromNameOrCurrent(scene_name);
	if (!scene) {
		return request.failed("specified scene doesn't exist");
	}

	obs_frontend_set_current_preview_scene(obs_scene_get_source(scene));
	return request.success();
}

/**
 * Transitions the currently previewed scene to the main output.
 * Will return an `error` if Studio Mode is not enabled.
 *
 * @param {Object (optional)} `with-transition` Change the active transition before switching scenes. Defaults to the active transition. 
 * @param {String} `with-transition.name` Name of the transition.
 * @param {int (optional)} `with-transition.duration` Transition duration (in milliseconds).
 *
 * @api requests
 * @name TransitionToProgram
 * @category studio mode
 * @since 4.1.0
 */
RpcResponse WSRequestHandler::TransitionToProgram(const RpcRequest& request) {
	if (!obs_frontend_preview_program_mode_active()) {
		return request.failed("studio mode not enabled");
	}

	if (request.hasField("with-transition")) {
		OBSDataAutoRelease transitionInfo =
			obs_data_get_obj(request.parameters(), "with-transition");

		if (obs_data_has_user_value(transitionInfo, "name")) {
			QString transitionName =
				obs_data_get_string(transitionInfo, "name");
			if (transitionName.isEmpty()) {
				return request.failed("invalid request parameters");
			}

			bool success = Utils::SetTransitionByName(transitionName);
			if (!success) {
				return request.failed("specified transition doesn't exist");
			}
		}

		if (obs_data_has_user_value(transitionInfo, "duration")) {
			int transitionDuration =
				obs_data_get_int(transitionInfo, "duration");
			obs_frontend_set_transition_duration(transitionDuration);
		}
	}

	obs_frontend_preview_program_trigger_transition();
	return request.success();
}

/**
 * Enables Studio Mode.
 *
 * @api requests
 * @name EnableStudioMode
 * @category studio mode
 * @since 4.1.0
 */
RpcResponse WSRequestHandler::EnableStudioMode(const RpcRequest& request) {
	if (obs_frontend_preview_program_mode_active()) {
		return request.failed("studio mode already active");
	}
	obs_queue_task(OBS_TASK_UI, [](void* param) {
		obs_frontend_set_preview_program_mode(true);

		UNUSED_PARAMETER(param);
	}, nullptr, true);
	return request.success();
}

/**
 * Disables Studio Mode.
 *
 * @api requests
 * @name DisableStudioMode
 * @category studio mode
 * @since 4.1.0
 */
RpcResponse WSRequestHandler::DisableStudioMode(const RpcRequest& request) {
	if (!obs_frontend_preview_program_mode_active()) {
		return request.failed("studio mode not active");
	}
	obs_queue_task(OBS_TASK_UI, [](void* param) {
		obs_frontend_set_preview_program_mode(false);

		UNUSED_PARAMETER(param);
	}, nullptr, true);

	return request.success();
}

/**
 * Toggles Studio Mode (depending on the current state of studio mode).
 *
 * @api requests
 * @name ToggleStudioMode
 * @category studio mode
 * @since 4.1.0
 */
RpcResponse WSRequestHandler::ToggleStudioMode(const RpcRequest& request) {
	obs_queue_task(OBS_TASK_UI, [](void* param) {
		bool previewProgramMode = obs_frontend_preview_program_mode_active();
		obs_frontend_set_preview_program_mode(!previewProgramMode);

		UNUSED_PARAMETER(param);
	}, nullptr, true);

	return request.success();
}


RpcResponse WSRequestHandler::ShowOBSApplication(const RpcRequest& request) {

	bool show = obs_data_get_bool(request.parameters(), "show");
	proc_handler_t* handle = obs_get_proc_handler();
	if (handle) {
		calldata_t cData;
		calldata_init(&cData);
		calldata_set_bool(&cData, "Show", show);
		proc_handler_call(handle, "ShowOBSWindows", &cData);
	}
	return request.success();
}

RpcResponse WSRequestHandler::GetOBSApplicationShowStatus(const RpcRequest& request)
{
#ifndef DISABLE_QT
	QMainWindow* mainWin = (QMainWindow*)obs_frontend_get_main_window();
	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_bool(data, "show", !mainWin->isHidden()&& !mainWin->isMinimized());
	return request.success(data);
#else
	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_bool(data, "show", false);
	return request.success(data);
#endif
}

#ifdef WIN32
#define CAMERA_SOURCE_ID "dshow_input"
#elif defined(__APPLE__)
#define CAMERA_SOURCE_ID "av_capture_input"
#else
#define CAMERA_SOURCE_ID "v4l2_input"
#endif
#define AI_CAMERA_SOURCE_ID "lovense_dshow_input"

//TODO: Save activated devices
static std::vector<std::string> g_vActiveDevicesID;
#include <algorithm>
static bool scene_enum_func(obs_scene_t* scene, obs_sceneitem_t* item, void* param)
{
	bool* pActive = (bool*)param;
	obs_source_t* source = obs_sceneitem_get_source(item);
#ifdef WIN32
	const char* source_id = obs_source_get_id(source);
	if (source_id && (strcmp(source_id, CAMERA_SOURCE_ID) == 0 || strcmp(source_id, AI_CAMERA_SOURCE_ID) == 0))
	{
		obs_data_t* setting = obs_source_get_settings(source);
		bool isActive = obs_data_get_bool(setting,"active");
		const char* deviceId = obs_data_get_string(setting, "video_device_id");
		if (isActive == *pActive)
		{
			obs_data_release(setting);
			return true;
		}

		//TODO:if device id is active in current scene
		if (deviceId && strlen(deviceId) > 0)
		{
			auto iter = std::find(g_vActiveDevicesID.begin(),g_vActiveDevicesID.end(),deviceId);
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
		blog(LOG_INFO, "[websocket] active camera:%s", deviceId);

		return true;
	}

	return true;
#endif
}

static bool g_isActived = false;
RpcResponse WSRequestHandler::ActiveOBSCamera(const RpcRequest& request)
{
#ifdef WIN32
	bool active = obs_data_get_bool(request.parameters(), "active");
	//if(active == g_isActived)
		//return request.success();
	g_vActiveDevicesID.clear();
	g_isActived = active;
	obs_source_t *source = obs_frontend_get_current_scene();
	OBSScene current_scene = obs_scene_from_source(source);
	obs_scene_enum_items(current_scene, scene_enum_func, &active);
	obs_source_release(source);

	//obs_frontend_source_list sceneList = {};
	//obs_frontend_get_scenes(&sceneList);
	//obs_data_array_t* scenes = obs_data_array_create();
	//for (size_t i = 0; i < sceneList.sources.num; i++) {
	//	obs_source_t* scene_source = sceneList.sources.array[i];
	//	obs_scene_t* sceneItem = obs_scene_from_source(scene_source);
	//	obs_scene_enum_items(sceneItem, scene_enum_func, &active);
	//}
	//obs_frontend_source_list_free(&sceneList);
	blog(LOG_INFO, "[websocket] active camera:%d", active);
	return request.success();
#else
    return request.success();
#endif
}
