#include <filesystem>
#include <numeric>
#include <memory>
#include <exception>
#include <fstream>
#include <new>
#include <mutex>

#include <obs-module.h>
#include <obs-frontend-api.h>

#include "lovense_string_helper.hpp"
#include "lovense_camera_source.hpp"
#include "lovense_obs_helper.hpp"
#include "lovense_scene_helper.hpp"
#include "lovense_camera_helper.hpp"
#include "lovense_obs_source.hpp"

#include "so-common.hpp"
#include "so-detection-center.hpp"


namespace lovense::so
{
	struct filter {

		obs_source_t*   source{ nullptr };
		gs_texrender_t* texrender{ nullptr };
		gs_stagesurf_t* stagesurface{ nullptr };

		obs_source_t*   current_scene{ nullptr };

		bool is_disabled{ false };

		void set_disabled()
		{
			is_disabled = true;
		}
	};

	static const char* filter_getname(void* unused)
	{
		UNUSED_PARAMETER(unused);
		return obs_module_text("SmartObject.FilterName");
	}

	/**                   PROPERTIES                     */
	static obs_properties_t* filter_properties(void* data)
	{
		obs_properties_t* props = obs_properties_create();

		obs_properties_add_int_slider(props, "face-skip-frames", obs_module_text("SmartObject.FaceSkipFrames"), 0, 30, 1);
		
		return props;
	}

	static void filter_defaults(obs_data_t* settings)
	{		
		obs_data_set_default_int(settings, "face-skip-frames", 0);
	}

	static void filter_update(void* data, obs_data_t* settings)
	{
		int count = (int)obs_data_get_int(settings, "face-skip-frames");
		so::DetectionCenter::instance()->set_head_skip_frames(count);
	}

	static void filter_activate(void* data)
	{
		struct so::filter* tf = reinterpret_cast<so::filter*>(data);
		tf->is_disabled = false;
	}

	static void filter_deactivate(void* data)
	{
		struct so::filter* tf = reinterpret_cast<so::filter*>(data);
		tf->set_disabled();
	}

	/**                   FILTER CORE                     */
	static void* filter_create(obs_data_t* settings, obs_source_t* source)
	{
		void* data = bmalloc(sizeof(struct so::filter));
		struct so::filter* tf = new (data) so::filter();

		tf->source    = source;
		tf->texrender = gs_texrender_create(GS_BGRA, GS_ZS_NONE);		
		
		filter_update(tf, settings);

		return tf;		
	}

	static void filter_destroy(void* data)
	{
		struct so::filter* tf = reinterpret_cast<so::filter*>(data);

		if (tf) {
			obs_enter_graphics();
			gs_texrender_destroy(tf->texrender);
			if (tf->stagesurface) {
				gs_stagesurface_destroy(tf->stagesurface);
			}
			obs_leave_graphics();
			tf->~filter();
			bfree(tf);

			DetectionCenter::instance()->check_enable();
		}
	}

	void filter_video_tick(void* data, float seconds)
	{	
		UNUSED_PARAMETER(data);
		UNUSED_PARAMETER(seconds);
	}

	static inline void skip_video_filter(struct so::filter* tf)
	{
		if (tf->source && obs_filter_get_parent(tf->source))
		{
			obs_source_skip_video_filter(tf->source);
		}
	}

	//static bool is_current_secen()

	static bool video_render(void* data, gs_effect_t* _effect) {

		//lovense::CostLog coster("filter_video_render");
		struct so::filter* tf = reinterpret_cast<so::filter*>(data);
		try {
			if (tf->is_disabled) {
				return false;
			}

			auto parent = obs_filter_get_parent(tf->source);
			if (parent == nullptr) {
				return false;
			}

			/*{
				ObsSource source;
				auto item = obs_scene_sceneitem_from_source(source.scene(), parent);			
			}*/

			const char* id = obs_source_get_id(parent);
			if (id == nullptr) {
				return false;
			}

			// Only apply to camera sources
			if (!camera_helper::IsDShow(id)) {
				tf->is_disabled = true;
				return false;
			}

			if (!obs_source_enabled(tf->source)) {
				return false;
			}

			obs_source_t* target = obs_filter_get_target(tf->source);
			if (!target) {
				return false;
			}

			const uint32_t width  = obs_source_get_base_width(target);
			const uint32_t height = obs_source_get_base_height(target);
			if (width == 0 || height == 0) {
				return false;
			}

			gs_texrender_reset(tf->texrender);
			if (!gs_texrender_begin(tf->texrender, width, height)) {
				return false;
			}
			struct vec4 background;
			vec4_zero(&background);
			//vec4_set(&background, 255.0f, 255.0f, 0.0f, 0.0f);
			gs_clear(GS_CLEAR_COLOR, &background, 0.0f, 0);
			gs_ortho(0.0f, static_cast<float>(width), 0.0f, static_cast<float>(height), -100.0f, 100.0f);
			gs_blend_state_push();
			gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);
			obs_source_video_render(target);
			gs_blend_state_pop();
			gs_texrender_end(tf->texrender);

			if (tf->stagesurface) {
				uint32_t stagesurf_width  = gs_stagesurface_get_width(tf->stagesurface);
				uint32_t stagesurf_height = gs_stagesurface_get_height(tf->stagesurface);
				if (stagesurf_width != width || stagesurf_height != height) {
					gs_stagesurface_destroy(tf->stagesurface);
					tf->stagesurface = nullptr;
				}
			}

			if (!tf->stagesurface) {
				tf->stagesurface = gs_stagesurface_create(width, height, GS_BGRA);
			}

			gs_stage_texture(tf->stagesurface, gs_texrender_get_texture(tf->texrender));
			uint8_t* video_data{ nullptr };
			uint32_t linesize{ 0 };
			if (!gs_stagesurface_map(tf->stagesurface, &video_data, &linesize)) {
				return false;
			}
			gs_stagesurface_unmap(tf->stagesurface);

			if (video_data == nullptr) {
				return false;
			}

			so::DetectionCenter::instance()->post_rgba(video_data, linesize, width, height);
			return true;
		}
		catch (const std::exception& e) {
#ifdef DEBUG
			so_error("filter_video_render: %s", e.what());
#else
			(void)e;
#endif
		}
		return false;
	}

	static void filter_video_render(void* data, gs_effect_t* _effect)
	{
		struct so::filter* tf = reinterpret_cast<so::filter*>(data);
		if (tf == nullptr) {
			return;
		}

		video_render(data, _effect);

		skip_video_filter(tf);
	}

	void RegisterFilter()
	{
		obs_source_info info = {};
		info.id = so::FILTER_ID;
		info.type = OBS_SOURCE_TYPE_FILTER;
		info.output_flags = OBS_SOURCE_VIDEO;
		info.get_name = filter_getname;
		info.create = filter_create;
		info.destroy = filter_destroy;
		info.get_defaults = filter_defaults;
		info.get_properties = filter_properties;
		info.update = filter_update;
		info.activate = filter_activate;
		info.deactivate = filter_deactivate;
		info.video_tick = filter_video_tick;
		info.video_render = filter_video_render;
		obs_register_source(&info);
	}
}
