#include <obs.hpp>
#include <obs-module.h>
#include <obs-frontend-api.h>

#include "lovense_error.hpp"
#include "lovense_source_helper.hpp"
#include "lovense_obs_source.hpp"
#include "lovense_obs_proc.hpp"
#include "lovense_camera_helper.hpp"

#include "so-common.hpp"
#include "so-helper.hpp"

namespace lovense::so::helper
{
	int  enable(int skip_face_frames)
	{
		if (skip_face_frames < 0) {
			skip_face_frames = 0;
		}
		else if (skip_face_frames > 30) {
			skip_face_frames = 30;
		}

		lovense::ObsSource current_scene;
		OBSSource cam_s = current_scene.find_from_id(lovense::CAMERA_DSHOW_SOURCE_ID);
		if (cam_s == nullptr) {
			return error::lvs_NonCameraDevice;
		}

#if 0
		bool is_feedback_ok = source_helper::check_feedback_exist();
#else
		bool is_feedback_ok = source_helper::ensure_feedback_valid();
#endif
		if (!is_feedback_ok) {
			return error::lvs_FeedbackNotExist;
		}

		lovense::ObsSource cam_source{ cam_s };
		auto filter_s = cam_source.find_filter(so::FILTER_ID);
		if (filter_s == nullptr) {
			auto filter_name = obs_module_text("SmartObject.FilterName");
			OBSDataAutoRelease setting = obs_data_create();
			if (setting) {
				obs_data_set_int(setting, "face-skip-frames", skip_face_frames);
			}

			OBSSourceAutoRelease filter =
				obs_source_create(so::FILTER_ID, filter_name, setting, nullptr);
			if (filter) {
				const char* sourceName = obs_source_get_name(cam_s);
				blog(LOG_INFO,
					"User added filter '%s' (%s) "
					"to source '%s'",
					filter_name, so::FILTER_ID, sourceName);

				obs_source_filter_add(cam_s, filter);
			}
		}
		else {
			OBSDataAutoRelease settings = obs_source_get_settings(filter_s);
			if (settings) {
				obs_data_set_int(settings, "face-skip-frames", skip_face_frames);
				obs_source_update(filter_s, settings);
			}
		}

		current_scene.TransfromSizeToScreen(cam_source.id(), OBS_ALIGN_CENTER);

		//lovense::proc::report_plugin_log("P0501", "open smart object");
		so_info("open smart object: %d", skip_face_frames);

		return error::lvs_Ok;
	}

	int disable()
	{
		obs_frontend_source_list sceneList = {};
		obs_frontend_get_scenes(&sceneList);

		auto find_feedback = [&](obs_scene_t* scene, obs_sceneitem_t* item, void* data)->bool {
			OBSSource source = obs_sceneitem_get_source(item);
			if (source) {
				const char* d_id = obs_source_get_id(source);

				if (d_id && camera_helper::IsDShow(d_id))
				{
					lovense::ObsSource cam_source{ source };
					while (cam_source) {
						OBSSource filter_s = cam_source.find_filter(so::FILTER_ID);
						if (!filter_s) {
							break;
						}
						obs_source_filter_remove(source, filter_s);
					}
				}
			}

			return true;
		};

		for (size_t i = 0; i < sceneList.sources.num; i++) {
			obs_source_t* scene = sceneList.sources.array[i];

			lovense::ObsSource obs_source(scene);
			obs_source.enum_sence_items(find_feedback, nullptr);
		}

		obs_frontend_source_list_free(&sceneList);
		//lovense::proc::report_plugin_log("P0501", "close smart object");
		so_info("close smart object");

		return error::lvs_Ok;
	}
}
