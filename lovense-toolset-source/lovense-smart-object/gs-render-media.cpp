#include "gs-render-media.h"
#include "lvs_greenscreen.h"
#include "obs.hpp"
#include "util/util.hpp"
#include "util/platform.h"
#include "util/dstr.h"
#include "util/threading.h"

#include "base64/base64.hpp"

#include "lovense_obs_proc.hpp"
#include <thread>
#define USE_JS_FRAME_DATA 

namespace lovense::gs
{
#ifdef USE_JS_FRAME_DATA
	std::string                  g_frame_base64_data;
	std::shared_ptr<std::thread> g_frame_handle;
	std::mutex                   g_frame_mutex;
	std::condition_variable      g_frame_cv;
	std::atomic_bool             g_is_stopped{ false };
	std::vector<char>            g_frame_decode;
#endif
	
	static void on_special_effects_frame(const std::string& data)
	{
		if (! LvsGreenScreen::getInstance()->getStatus()) return;

		{
			std::lock_guard lk(g_frame_mutex);
			g_frame_base64_data = data;
		}
		g_frame_cv.notify_one();
	}

#ifndef USE_JS_FRAME_DATA
	class RenderEngineImpl
	{
	public:
		bool start()
		{
			if (obs_output_) {
				return true;
			}

			obs_output_ = obs_output_create("lvs_feedback_output",
				"lvs_feedback_output", nullptr,
				nullptr);
		

			video_t* fd_video = this->get_video();
			if (!fd_video) {
				obs_output_release(obs_output_);
				obs_output_ = nullptr;
				return false;
			}

			obs_output_set_media(obs_output_, fd_video, obs_get_audio());
			bool success = obs_output_start(obs_output_);
			if (!success) {
				stop();
			}

			obs_output_begin_data_capture(obs_output_, 0);

			return success;
		}

		bool active() const
		{
			return obs_output_ && obs_output_active(obs_output_);
		}

		void stop()
		{
			if (obs_output_) {
				obs_output_set_media(obs_output_, nullptr, nullptr);
				obs_output_end_data_capture(obs_output_);
			}

			if (obs_view_) {
				obs_view_remove(obs_view_);
				obs_view_set_source(obs_view_, 0, nullptr);
				obs_video_ = nullptr;
			}
		}

		void shutdown()
		{
			if (obs_output_) {
				obs_output_release(obs_output_);
				obs_output_ = nullptr;
			}

			if (obs_view_) {
				obs_view_destroy(obs_view_);
				obs_view_ = nullptr;
			}
		
		}

		~RenderEngineImpl()
		{
			shutdown();
		}
	private:
		video_t* get_video()
		{
			if (obs_video_)
				return obs_video_;

			if (!obs_view_)
				obs_view_ = obs_view_create();

			obs_source_t* source = nullptr;

			auto rawSource =
				obs_get_source_by_name("Lovense Video Feedback");
			if (!rawSource)
				return obs_video_;

			// TODO: Handle size adjustment
			source_width_  = obs_source_get_base_width(rawSource);
			source_height_ = obs_source_get_base_height(rawSource);

			// Use a scene transform to fit the source size to the canvas
			if (!sourceScene)
				sourceScene = obs_scene_create_private(nullptr);
			source = obs_source_get_ref(obs_scene_get_source(sourceScene));

			if (sourceSceneItem) {
				if (obs_sceneitem_get_source(sourceSceneItem) !=
					rawSource) {
					obs_sceneitem_remove(sourceSceneItem);
					sourceSceneItem = nullptr;
				}
			}
			if (!sourceSceneItem) {
				sourceSceneItem = obs_scene_add(sourceScene, rawSource);
				obs_source_release(rawSource);

				obs_sceneitem_set_bounds_type(sourceSceneItem,
					OBS_BOUNDS_SCALE_INNER);
				obs_sceneitem_set_bounds_alignment(sourceSceneItem,
					OBS_ALIGN_CENTER);

				const struct vec2 size = {
					(float)obs_source_get_width(source),
					(float)obs_source_get_height(source),
				};
				obs_sceneitem_set_bounds(sourceSceneItem, &size);
			}

			auto current = obs_view_get_source(obs_view_, 0);
			if (source != current) {
				obs_view_set_source(obs_view_, 0, source);
			}
			obs_source_release(source);
			obs_source_release(current);

			if (!obs_video_)
				obs_video_ = obs_view_add(obs_view_);
			return obs_video_;
		}
	private:
		obs_view_t*      obs_view_{nullptr};
		video_t*         obs_video_{ nullptr };
		obs_scene_t*     sourceScene{nullptr};
		obs_sceneitem_t* sourceSceneItem{ nullptr };

		obs_output_t*    obs_output_{nullptr};

		//obs_source_get_base_width
		uint32_t         source_width_{ 0 };
		uint32_t         source_height_{ 0 };
	};

	static RenderEngineImpl _impl;
#endif

	static void _thread_on_frame_data()
	{
		while (!g_is_stopped) {

			std::string frame_data;
			{
				std::unique_lock lk(g_frame_mutex);
				g_frame_cv.wait(lk);
				frame_data = std::move(g_frame_base64_data);
			}

			if (g_is_stopped) break;

			//
			g_frame_decode.clear();
			base64_decode(frame_data, g_frame_decode);

			try {
				cv::Mat mat_data = cv::imdecode(g_frame_decode, 1);
				if (!mat_data.empty()) {
					LvsGreenScreen::getInstance()->setbgFrame(mat_data);
				}
			}
			catch (...) {
				std::cout << "netData2Mat decode data error" << std::endl;
			}
		}
	}

	void start_render_media()
	{
#ifdef USE_JS_FRAME_DATA
		proc::add_special_effects(&on_special_effects_frame);
		if (g_frame_handle == nullptr) {
			g_frame_handle = std::make_shared<std::thread>(_thread_on_frame_data);
		}
		
#else
		if (_impl.active()) return;

		_impl.start();
#endif
	}

	void stop_render_media()
	{
#ifdef USE_JS_FRAME_DATA
		proc::remove_special_effects(&on_special_effects_frame);
#else
		_impl.stop();
#endif
	}

	void shutdown_reader_media()
	{
#ifdef USE_JS_FRAME_DATA
		proc::remove_special_effects(&on_special_effects_frame);
		g_is_stopped = true;
		g_frame_cv.notify_all();

		if (g_frame_handle && g_frame_handle->joinable()) {
			g_frame_handle->join();
		}
#else
		_impl.shutdown();
#endif
	}
}
