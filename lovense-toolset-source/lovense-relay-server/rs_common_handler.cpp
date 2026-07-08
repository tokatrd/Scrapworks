#include "obs.hpp"
#include "obs-frontend-api.h"
#include "util/dstr.hpp"
#include "util/platform.h"

#include "lovense_common.hpp"
#include "lovense_obs_common.hpp"

#include "rs_common_handler.hpp"

#if 0
#include "lovense_lib_proxy.hpp"
extern lovense::ObsLibProxy obs_lib_proxy;
#endif

namespace lovense
{
	namespace rs
	{
		
		//---------------------------------------------------------------------------------------------
		template <typename T> T* calldata_get_pointer(const calldata_t* data, const char* name) {
			void* ptr = nullptr;
			calldata_get_ptr(data, name, &ptr);
			return reinterpret_cast<T*>(ptr);
		}

		static void _proceSceneItemAdd(void* param, calldata_t* data) {
			UNUSED_PARAMETER(param);

			obs_scene_t* scene1 = nullptr;
			calldata_get_ptr(data, "scene", &scene1);
			if (!scene1)
				return;
			obs_sceneitem_t* sceneItem = nullptr;
			calldata_get_ptr(data, "item", &sceneItem);
			if (!sceneItem) {
				return;
			}
			obs_source_t* source = obs_sceneitem_get_source(sceneItem);
			if (!source)
				return;


			const char* id = obs_source_get_id(source);
			if (id && strcmp(id, lovense::FEEDBACK_SOURCE_ID) == 0) {
#if 0
				//New feedback source, set default sRGB off
				auto blending_method = obs_lib_proxy.obs_sceneitem_get_blending_method_impl(sceneItem);
				if (blending_method != OBS_BLEND_METHOD_SRGB_OFF)
				{
					obs_lib_proxy.obs_sceneitem_set_blending_method_impl(sceneItem, OBS_BLEND_METHOD_SRGB_OFF);
				}
#endif
				return;
			}

			obs_source_t *cur_source = obs_frontend_get_current_scene();
			obs_scene_t* scene = obs_scene_from_source(cur_source);
            obs_source_release(cur_source);
			obs_scene_enum_items(scene, [](obs_scene_t* scene, obs_sceneitem_t* item, void* param)->bool {
				UNUSED_PARAMETER(scene);
				UNUSED_PARAMETER(param);
				auto item_source = obs_sceneitem_get_source(item);
				const char* id = obs_source_get_id(item_source);
				if (strcmp(id, lovense::FEEDBACK_SOURCE_ID) == 0) {
					obs_sceneitem_set_order(item, OBS_ORDER_MOVE_TOP);
					return false;
				}

				return true;

				}, nullptr);
		}

		static void _procSourceCreate(void* param, calldata_t* data) {
			UNUSED_PARAMETER(param);
			obs_source_t *source = calldata_get_pointer<obs_source_t>(data, "source");
			if (!source) {
				return;
			}
			obs_source_type type = obs_source_get_type(source);
			if (type != OBS_SOURCE_TYPE_SCENE)
				return;
			signal_handler_t* sh = obs_source_get_signal_handler(source);
			if (sh)
				signal_handler_connect(sh, "item_add", _proceSceneItemAdd, nullptr);
		}

		static void _procSourceDestroy(void* param, calldata_t* data) {
			UNUSED_PARAMETER(param);
            obs_source_t *source = calldata_get_pointer<obs_source_t>(data, "source");
			if (!source) {
				return;
			}
			obs_source_type type = obs_source_get_type(source);
			if (type != OBS_SOURCE_TYPE_SCENE)
				return;
			signal_handler_t* sh = obs_source_get_signal_handler(source);
			if (sh)
				signal_handler_disconnect(sh, "item_add", _proceSceneItemAdd, nullptr);
		}

		void register_top_feedback_event() {
			obs_enum_sources([](void* param, obs_source_t* source) {
				UNUSED_PARAMETER(param);
				obs_source_type type = obs_source_get_type(source);
				if (type == OBS_SOURCE_TYPE_SCENE) {
					signal_handler_t* sh = obs_source_get_signal_handler(source);
					if (sh)
						signal_handler_connect(sh, "item_add", _proceSceneItemAdd, nullptr);
				}
				return true;
				}, nullptr);


			signal_handler_t* coreSignalHandler = obs_get_signal_handler();
			if (coreSignalHandler) {
				signal_handler_connect(coreSignalHandler, "source_create", _procSourceCreate, nullptr);
				signal_handler_connect(coreSignalHandler, "source_destroy", _procSourceDestroy, nullptr);
			}
		}

		void release_top_feedback_event() {
			signal_handler_t* coreSignalHandler = obs_get_signal_handler();
			if (coreSignalHandler) {
                signal_handler_disconnect(coreSignalHandler, "source_create", _procSourceCreate, nullptr);
				signal_handler_disconnect(coreSignalHandler, "source_destroy", _procSourceDestroy, nullptr);
			}

			// Disconnect from signals of all existing sources
			obs_enum_sources([](void* param, obs_source_t* source) {
				UNUSED_PARAMETER(param);
				obs_source_type type = obs_source_get_type(source);
				if (type == OBS_SOURCE_TYPE_SCENE) {
					signal_handler_t* sh = obs_source_get_signal_handler(source);
					if (sh)
						signal_handler_disconnect(sh, "item_add", _proceSceneItemAdd, nullptr);
				}
				return true;
				}, nullptr);
		}

#if 0
		void reset_current_scene_feedback_srgb()
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

					auto blending_method = obs_lib_proxy.obs_sceneitem_get_blending_method_impl(item);
					if (blending_method != OBS_BLEND_METHOD_SRGB_OFF)
					{
						obs_lib_proxy.obs_sceneitem_set_blending_method_impl(item, OBS_BLEND_METHOD_SRGB_OFF);
					}								

					return true;

					}, nullptr);
			}
		}
#endif
	}
}
