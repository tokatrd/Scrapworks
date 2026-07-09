#pragma once

#include <memory>
#include <thread>

#include "opencv2/core.hpp"
#include "json11/json11.hpp"

namespace lovense::so
{
	class Detector
	{
	public:
		virtual bool initialize() { return false; }
		virtual void shutdown() {}
		virtual bool is_working() { return false; }
	};

	class HeadDetector : public Detector
	{
	public:		
		virtual bool detect_head(cv::Mat frame, int factor, json11::Json& result) = 0;		
	};

	class DetectionCenter
	{
	public:
		static std::shared_ptr<DetectionCenter> instance();

		bool initialize();
		void shutdown();

		void post_rgba(uint8_t* data, uint32_t linesize, size_t frame_w, size_t frame_h);

		void get_head_points(json11::Json& value);

		void set_head_skip_frames(int count);
		void enable_head(int browser_id);
		void disable_head();
		void disable_all();
		void check_enable();

		void set_heart_interval(int interval = 0);
		void heart();
	protected:
		void post(cv::Mat frame);

		void work();
		void handle(cv::Mat frame);

		bool is_skip_frame();
		bool is_enable();

		void notify_disabled(std::string message);
	protected:
		DetectionCenter() = default;

	private:
		int                          factor_{1};
		HeadDetector*                face_detector_{nullptr};
		bool                         enable_thread_{ true };
				

		std::thread                  work_thread_;
		std::mutex                   frame_mutex_;
		std::condition_variable      frame_cv_;
		cv::Mat                      frame_;
		std::atomic_bool             working_;


		std::mutex                   face_result_mutex_;
		json11::Json                 face_result_;
		std::atomic_bool             face_exist_;
		int                          face_skip_frames_{ 0 };
		int                          face_frames_{0};

		std::atomic_bool             enable_all_{ false };
		std::atomic_bool             face_enable_{ false };
		std::atomic_int              head_browser_id_{ -1 };

		std::atomic_int              heart_timestamp_{ 0 };
		std::atomic_int              heart_interval_{ 300 };
	};
}
