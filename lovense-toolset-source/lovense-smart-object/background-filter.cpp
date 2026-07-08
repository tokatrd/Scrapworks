#include "agora_utils.h"
#include "lvs_greenscreen.h"
#include <obs-module.h>
#include <obs-frontend-api.h>
#ifdef ENABLE_ONNXRUNTIME
#include <cpu_provider_factory.h>
#endif

#if defined(__APPLE__)

#endif

#ifdef __linux__
#include <tensorrt_provider_factory.h>
#endif

#ifdef _WIN32
#ifdef ENABLE_ONNXRUNTIME
#include <dml_provider_factory.h>
#endif
#include <wchar.h>
#endif // _WIN32


#include <numeric>
#include <memory>
#include <exception>
#include <fstream>
#include <new>
#include <mutex>

#include <graphics/image-file.h>

#include "plugin-macros.h"

#include "gs-common.h"
#include "gs-render-media.h"
#include "gs-server-handler.h"

using namespace lovense;

const char* EFFECT_PATH = "effects/lvs_green_screen.effect";



struct lvs_greenscreen_filter {

	HsvColor hsvLow = HsvColor(50, 94, 49);
	XyThreshold xyThd = XyThreshold(0, 0, 0, 0);
	std::string greenscreenType;
	double      greenCapacity{ 0.5 };

	obs_source_t* source;
	gs_texrender_t* texrender;
	gs_stagesurf_t* stagesurface;
	gs_effect_t* effect;

	cv::Mat backgroundMask;
	int maskEveryXFrames = 1;
	int maskEveryXFramesCount = 0;

#ifdef USE_UMAT
	cv::UMat inputBGRA;
#else
	cv::Mat inputBGRA;
#endif

	bool isDisabled{false};

	std::mutex inputBGRALock;
	std::mutex outputBGRALock;

	bool  is_ok{ false };

};

static const char* filter_getname(void* unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("Greenscreen.FilterName");
}

/**                   PROPERTIES                     */

static obs_properties_t* filter_properties(void* data)
{
	obs_properties_t* props = obs_properties_create();

	//obs_properties_add_int_slider(props, "hue", obs_module_text("Hue"), 0, 255, 1);
	//obs_properties_add_int_slider(props, "saturation", obs_module_text("Saturation"), 0, 255, 1);
	//obs_properties_add_int_slider(props, "brightness", obs_module_text("Brightness"), 0, 255, 1);
	obs_properties_add_int_slider(props,   "xstart", obs_module_text("Greenscreen.XStart"), 0, 255, 1);
	obs_properties_add_int_slider(props,   "xend", obs_module_text("Greenscreen.XEnd"), 0, 255, 1);
	obs_properties_add_int_slider(props,   "ystart", obs_module_text("Greenscreen.YStart"), 0, 255, 1);
	obs_properties_add_int_slider(props,   "yend", obs_module_text("Greenscreen.YEnd"), 0, 255, 1);
	obs_properties_add_float_slider(props, "greenCapacity", obs_module_text("Greenscreen.Capacity"), 0.1, 0.9, 0.1);

	obs_property_t* p_greenscreen_type = obs_properties_add_list(props, "greenscreenType",
		obs_module_text("Greenscreen.greenscreenType"),
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);

	obs_property_list_add_string(p_greenscreen_type, obs_module_text("Greenscreen.Static"), gs::GREENSCREEN_STATIC);
	obs_property_list_add_string(p_greenscreen_type, obs_module_text("Greenscreen.Dynamic"), gs::GREENSCREEN_DYNAMIC);

	obs_property_t* p_status =
		obs_properties_add_list(props, "greenscreenStatus", obs_module_text("Greenscreen.Status"),
			OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);

	obs_property_list_add_string(p_status, obs_module_text("Greenscreen.On"), gs::GREENSCREEN_ON);
	obs_property_list_add_string(p_status, obs_module_text("Greenscreen.Off"), gs::GREENSCREEN_OFF);

	UNUSED_PARAMETER(data);
	return props;
}

static void filter_defaults(obs_data_t* settings)
{
	obs_data_set_default_int(settings, "hue", 50);
	obs_data_set_default_int(settings, "saturation", 94);
	obs_data_set_default_int(settings, "brightness", 49);
	obs_data_set_default_int(settings, "xstart", 0);
	obs_data_set_default_int(settings, "xend", 0);
	obs_data_set_default_int(settings, "ystart", 0);
	obs_data_set_default_int(settings, "yend", 0);
	obs_data_set_default_string(settings, "greenscreenType", gs::GREENSCREEN_STATIC);
	obs_data_set_default_string(settings, "greenscreenStatus", gs::GREENSCREEN_ON);
	obs_data_set_default_double(settings, "greenCapacity", 0.5);
}

static void filter_update(void* data, obs_data_t* settings)
{
	struct lvs_greenscreen_filter* tf = reinterpret_cast<lvs_greenscreen_filter*>(data);
	tf->hsvLow.h = (int)obs_data_get_int(settings, "hue");
	tf->hsvLow.s = (int)obs_data_get_int(settings, "saturation");
	tf->hsvLow.v = (int)obs_data_get_int(settings, "brightness");
	tf->xyThd.xMin = (int)obs_data_get_int(settings, "xstart");
	tf->xyThd.xMax = (int)obs_data_get_int(settings, "xend");
	tf->xyThd.yMin = (int)obs_data_get_int(settings, "ystart");
	tf->xyThd.yMax = (int)obs_data_get_int(settings, "yend");
	tf->greenCapacity = (double)obs_data_get_double(settings, "greenCapacity");

	const std::string newGreenscreenType = obs_data_get_string(settings, "greenscreenType");
	tf->greenscreenType = newGreenscreenType;

	LvsGreenScreen::getInstance()->setHsvThreshold(tf->hsvLow);
	LvsGreenScreen::getInstance()->setXyThreshold(tf->xyThd);

	if (gs::GREENSCREEN_DYNAMIC == tf->greenscreenType)
		LvsGreenScreen::getInstance()->setGreenScreenType(1);
	else
		LvsGreenScreen::getInstance()->setGreenScreenType(0);

	const std::string status = obs_data_get_string(settings, "greenscreenStatus");

	LvsGreenScreen::getInstance()->setStatus("On" == status ? true : false);

	if (!LvsGreenScreen::getInstance()->getStatus()) {
		gs::stop_render_media();
	}
	else {
		gs::start_render_media();
	}

	if (AgoraUtils::getInstance()->resetGreenCapacity((float)tf->greenCapacity)) {
		LvsGreenScreen::getInstance()->reset();
		blog(LOG_INFO, "[lovense] greenscreen set GreenCapacity: %.01f", (float)tf->greenCapacity);
	}

	//todo
	gs::ServerHandler::instance()->sync(settings);
}

static void filter_activate(void* data)
{
	struct lvs_greenscreen_filter* tf = reinterpret_cast<lvs_greenscreen_filter*>(data);
	tf->isDisabled = false;
}

static void filter_deactivate(void* data)
{
	struct lvs_greenscreen_filter* tf = reinterpret_cast<lvs_greenscreen_filter*>(data);
	tf->isDisabled = true;
}

/**                   FILTER CORE                     */

static void* filter_create(obs_data_t* settings, obs_source_t* source)
{
	void* data = bmalloc(sizeof(struct lvs_greenscreen_filter));
	struct lvs_greenscreen_filter* tf = new (data) lvs_greenscreen_filter();

	tf->source = source;
	tf->texrender = gs_texrender_create(GS_BGRA, GS_ZS_NONE);

	obs_enter_graphics();

	char* effect_path = obs_module_file(EFFECT_PATH);
	gs_effect_destroy(tf->effect);
	tf->effect = gs_effect_create_from_file(effect_path, NULL);
	bfree(effect_path);

	obs_leave_graphics();

	filter_update(tf, settings);
	AgoraUtils::getInstance()->enableVideo();
	return tf;
}

static void filter_destroy(void* data)
{
	struct lvs_greenscreen_filter* tf = reinterpret_cast<lvs_greenscreen_filter*>(data);

	if (tf) {
		obs_enter_graphics();
		gs_texrender_destroy(tf->texrender);
		if (tf->stagesurface) {
			gs_stagesurface_destroy(tf->stagesurface);
		}
		gs_effect_destroy(tf->effect);
		obs_leave_graphics();
		tf->~lvs_greenscreen_filter();
		bfree(tf);
	}
}

void filter_video_tick(void* data, float seconds)
{
	//auto start = std::chrono::steady_clock::now();

	struct lvs_greenscreen_filter* tf = reinterpret_cast<lvs_greenscreen_filter*>(data);

	if (tf->isDisabled) {
		return;
	}

	if (!obs_source_enabled(tf->source)) {
		return;
	}

	if (tf->inputBGRA.empty()) {
		return;
	}

#if 0
	if (tf->backgroundMask.empty()) {
		// First frame. Initialize the background mask.
		tf->backgroundMask = cv::Mat(imageBGRA.size(), CV_8UC1, cv::Scalar(255));
	}
#endif

	tf->maskEveryXFramesCount++;
	tf->maskEveryXFramesCount %= tf->maskEveryXFrames;

	try {
		if (tf->maskEveryXFramesCount != 0 && tf->is_ok) {
			// We are skipping processing of the mask for this frame.
			// Get the background mask previously generated.
			; // Do nothing
		}
		else
		{
			if (!LvsGreenScreen::getInstance()->isEmpty()) {
#ifdef USE_UMAT
				cv::UMat imageBGRA;
#else
				cv::Mat imageBGRA;
#endif
				{
					std::unique_lock<std::mutex> lock(tf->inputBGRALock, std::try_to_lock);
					if (!lock.owns_lock()) {
						return;
					}
#ifdef USE_UMAT
					cv::cvtColor(tf->inputBGRA, imageBGRA, cv::COLOR_BGRA2BGR);
#else
					imageBGRA = tf->inputBGRA.clone();
#endif
				}

#ifdef USE_UMAT
				cv::UMat merge_dist = LvsGreenScreen::getInstance()->getGreenScreenFrame(imageBGRA);
				cv::UMat u_bgra_dist;
				cv::cvtColor(merge_dist, u_bgra_dist, cv::COLOR_BGR2BGRA);
				u_bgra_dist.copyTo(tf->backgroundMask);
#else
				cv::Mat dist;
				cv::cvtColor(imageBGRA, dist, cv::COLOR_BGRA2BGR);

				cv::Mat merge_dist = LvsGreenScreen::getInstance()->getGreenScreenFrame(dist);
				cv::Mat bgra_dist;
				cv::cvtColor(merge_dist, bgra_dist, cv::COLOR_BGR2BGRA);
				bgra_dist.copyTo(tf->backgroundMask);
				
#endif
				tf->is_ok = true;
			}
		}
	}
	catch (const std::exception& e) {
#ifdef DEBUG
		blog(LOG_ERROR, "[greenscreen]filter_video_tick: %s", e.what());
#else
		(void)e;
#endif
	}

	UNUSED_PARAMETER(seconds);

	// blog(LOG_INFO, "filter_video_tick %d "
    //  	, chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - start).count());
}

static inline void skip_video_filter(struct lvs_greenscreen_filter* tf)
{
	if (tf->source && obs_filter_get_parent(tf->source))
	{
		obs_source_skip_video_filter(tf->source);
	}
}

static void filter_video_render(void* data, gs_effect_t* _effect)
{
	//auto start = chrono::steady_clock::now();
	UNUSED_PARAMETER(_effect);
	struct lvs_greenscreen_filter* tf = reinterpret_cast<lvs_greenscreen_filter*>(data);

	try {
		if (LvsGreenScreen::getInstance()->isEmpty()) {
			skip_video_filter(tf);
			return;
		}

		if (!obs_source_enabled(tf->source)) {
			skip_video_filter(tf);
			return;
		}

		obs_source_t* target = obs_filter_get_target(tf->source);
		if (!target) {
			skip_video_filter(tf);
			return;
		}
		const uint32_t width = obs_source_get_base_width(target);
		const uint32_t height = obs_source_get_base_height(target);
		if (width == 0 || height == 0) {
			skip_video_filter(tf);
			return;
		}
		gs_texrender_reset(tf->texrender);
		if (!gs_texrender_begin(tf->texrender, width, height)) {
			skip_video_filter(tf);
			return;
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
			uint32_t stagesurf_width = gs_stagesurface_get_width(tf->stagesurface);
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
		uint8_t* video_data;
		uint32_t linesize;
		if (!gs_stagesurface_map(tf->stagesurface, &video_data, &linesize)) {
			skip_video_filter(tf);
			return;
		}

		{
			std::lock_guard<std::mutex> lock(tf->inputBGRALock);
#ifdef USE_UMAT
			cv::Mat(height, width, CV_8UC4, video_data, linesize).copyTo(tf->inputBGRA);
#else
			tf->inputBGRA = cv::Mat(height, width, CV_8UC4, video_data, linesize);
#endif

			if (LvsGreenScreen::getInstance()->getStatus()) {
				try {
					AgoraUtils::getInstance()->pushVideoFrame(tf->inputBGRA.clone());
				}
				catch (const cv::Exception& e) {
#ifdef DEBUG
					blog(LOG_ERROR, "[greenscreen]filter_video_render: %s", e.what());
#else
					(void)e;
#endif
				}

			}
		}

		gs_stagesurface_unmap(tf->stagesurface);

		// Output the masked image
		if (!tf->effect) {
			// Effect failed to load, skip rendering
			skip_video_filter(tf);
			return;
		}

		if (!obs_source_process_filter_begin(tf->source, GS_RGBA, OBS_ALLOW_DIRECT_RENDERING)) {
			skip_video_filter(tf);
			return;
		}

		gs_texture_t* alphaTexture = nullptr;
		{
			std::lock_guard<std::mutex> lock(tf->outputBGRALock);
			if (!tf->is_ok) {
				skip_video_filter(tf);
				return;
			}
			alphaTexture = gs_texture_create(tf->backgroundMask.cols, tf->backgroundMask.rows, GS_BGRA, 1,
				(const uint8_t**)&tf->backgroundMask.data, GS_DYNAMIC);
			if (!alphaTexture) {
				blog(LOG_ERROR, "Failed to create alpha texture");
				skip_video_filter(tf);
				return;
			}
		}

		gs_eparam_t* bgImage = gs_effect_get_param_by_name(tf->effect, "bgimage");
		gs_effect_set_texture(bgImage, alphaTexture);

		gs_blend_state_push();
		gs_reset_blend_state();

		const char* techName = "LvsDraw";

		obs_source_process_filter_tech_end(tf->source, tf->effect, 0, 0, techName);

		gs_blend_state_pop();

		gs_texture_destroy(alphaTexture);
	}
	catch (const cv::Exception& e) {
		skip_video_filter(tf);
#ifdef DEBUG
		blog(LOG_ERROR, "[greenscreen]end filter_video_render: %s", e.what());
#else
		(void)e;
#endif
	}
	//blog(LOG_INFO, "filter_video_render %d "
	//	, chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - start).count());
}

struct obs_source_info lvs_greenscreen_filter_info = {
  .id = gs::FILTER_ID,
  .type = OBS_SOURCE_TYPE_FILTER,
  .output_flags = OBS_SOURCE_VIDEO,
  .get_name = filter_getname,
  .create = filter_create,
  .destroy = filter_destroy,
  .get_defaults = filter_defaults,
  .get_properties = filter_properties,
  .update = filter_update,
  .activate = filter_activate,
  .deactivate = filter_deactivate,
  .video_tick = filter_video_tick,
  .video_render = filter_video_render,
};

#if 0
struct lvsFeedBack_data {
	obs_output_t* output;
	volatile bool active;
	volatile bool stopping;
};

static void lvs_destroy(void* data)
{
	gs::shutdown_reader_media();
	bfree(data);
}

static void* lvs_create(obs_data_t* settings, obs_output_t* output)
{
	struct lvsFeedBack_data* lvsFeedbackOutput =
		(struct lvsFeedBack_data*)bzalloc(sizeof(*lvsFeedbackOutput));
	lvsFeedbackOutput->output = output;

	UNUSED_PARAMETER(settings);
	return lvsFeedbackOutput;
}

uint32_t g_output_width = 0;
uint32_t g_output_height = 0;

static bool lvs_start(void* data)
{
	struct lvsFeedBack_data* lvsFeedbackOutput = (struct lvsFeedBack_data*)data;
	g_output_width = obs_output_get_width(lvsFeedbackOutput->output);
	g_output_height = obs_output_get_height(lvsFeedbackOutput->output);

	blog(LOG_INFO, "Lvs feedback output started:%d,%d", g_output_width, g_output_height);
	return true;
}

static void lvs_stop(void* data, uint64_t ts)
{
	blog(LOG_INFO, "Lvs feedback output stopping");
	g_output_width = 0;
	g_output_height = 0;

	gs::stop_render_media();
	UNUSED_PARAMETER(ts);
}


static void lvs_video(void* param, struct video_data* frame)
{
	if (g_output_height > 0 && g_output_width > 0) {
		cv::Mat bgPic = cv::Mat(int(g_output_height * 3 / 2), g_output_width, CV_8UC1, frame->data[0]);

		if (!bgPic.empty()) {
			cv::cvtColor(bgPic, bgPic, cv::COLOR_YUV2BGR_NV12);
			LvsGreenScreen::getInstance()->setbgFrame(
				bgPic(Range(0, int(g_output_height * 5 / 6)), Range(0, int(g_output_width * 2 / 10)))
				.clone());
		}
	}

	UNUSED_PARAMETER(param);
}

struct obs_output_info lvs_feedback_output_info = {
  .id = "lvs_feedback_output",
  .flags = OBS_OUTPUT_VIDEO,
  .get_name = filter_getname,
  .create = lvs_create,
  .destroy = lvs_destroy,
  .start = lvs_start,
  .stop = lvs_stop,
  .raw_video = lvs_video,
};
#endif
