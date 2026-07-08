#include <memory>
#include <mutex>

#include "opencv2/imgproc.hpp"

#include "lovense_error.hpp"
#include "lovense_obs_proc.hpp"
#include "lovense_obs_source.hpp"
#include "lovense_obs_common.hpp"

#include "so-common.hpp"
#include "so-detection-center.hpp"
#include "so-dlib-impl.hpp"

namespace lovense::so
{
	std::shared_ptr<DetectionCenter> DetectionCenter::instance()
	{
		static std::once_flag                   flag;
		static std::shared_ptr<DetectionCenter> singleton = nullptr;
		std::call_once(flag, [&] {
			singleton = std::shared_ptr<DetectionCenter>(new DetectionCenter());
			});
		return singleton;
	}

	bool DetectionCenter::initialize()
	{
		face_detector_ = new DlibDetector();

		bool ok = face_detector_->initialize();
		if (!ok) {
			so_warn("DlibDetector initialize failed");
			return false;
		}

		if (enable_thread_) {
			working_     = true;
			work_thread_ = std::thread(std::bind(&DetectionCenter::work, this));
		}		

		return true;
	}

	void DetectionCenter::shutdown()
	{
		if (enable_thread_) {
			working_ = false;
			frame_cv_.notify_all();
			if (work_thread_.joinable()) {
				work_thread_.join();
			}
		}

		if (face_detector_) {
			face_detector_->shutdown();
			delete face_detector_;
			face_detector_ = nullptr;
		}
	}

	void DetectionCenter::get_head_points(json11::Json & value)
	{	
		{
			std::lock_guard<std::mutex> lk{ face_result_mutex_ };
			value = face_result_;
		}

	}

	void DetectionCenter::set_head_skip_frames(int count)
	{
		face_skip_frames_ = count;

		json11::Json data = json11::Json::object{
			{"skip_head_frames", count }
		};

		//todo
		json11::Json notify = json11::Json::object{
				   {"module",  so::MODULE_NAME},
				   {"func",    "SyncSettings"},
				   {"code",    error::lvs_Ok},
				   {"data",    data},

		};

		proc::forward_js_message(notify.dump());
	}

	void DetectionCenter::enable_head(int browser_id)
	{
		face_enable_     = true;
		enable_all_      = true;
		heart_timestamp_ = time(nullptr);
		head_browser_id_ = browser_id;
	}

	void DetectionCenter::disable_head()
	{
		face_enable_     = false;
		enable_all_      = false;
	}

	void DetectionCenter::disable_all()
	{
		face_enable_     = false;
	}

	void DetectionCenter::check_enable()
	{
		bool enable = is_enable();
		if (! enable) return;

		lovense::ObsSource current_scene;
		OBSSource cam_s = current_scene.find_from_id(lovense::CAMERA_DSHOW_SOURCE_ID);
		if (cam_s == nullptr) {
			enable = false;
		}
		else {
			lovense::ObsSource cam_source{ cam_s };
			auto filter_s = cam_source.find_filter(so::FILTER_ID);
			if (filter_s == nullptr) {
				enable = false;
			}
		}

		if (!enable) {
			enable_all_ = enable;
			this->notify_disabled("filter removed");
		}	
	}

	void DetectionCenter::notify_disabled(std::string message)
	{
		//
		json11::Json notify = json11::Json::object{
				   {"module",  so::MODULE_NAME},
				   {"func",    "DisableAll"},
				   {"code",    error::lvs_Ok},
				   {"message", message}
		};

		proc::forward_js_message(notify.dump());
	}

	bool DetectionCenter::is_skip_frame()
	{
		if (face_skip_frames_ <= 0) return false;
#if 0
		if (!face_exist_) {
			return false;
		}
#endif

		if (++face_frames_ >= face_skip_frames_) {
			face_frames_ = 0;
			return false;
		}

		return true;
	}

	bool DetectionCenter::is_enable()
	{
		if (!enable_all_) return false;

		return face_enable_ && face_detector_ && (face_detector_->is_working());
	}

	void DetectionCenter::post_rgba(uint8_t* data, uint32_t linesize, size_t frame_w, size_t frame_h)
	{
		if ((!is_enable()) || is_skip_frame()) {
			return; 
		}

		cv::Mat bgra_frame = cv::Mat(frame_h, frame_w, CV_8UC4, data, linesize);
		cv::Mat small_frame;
		cv::Mat bgr_frame;

		enum {
			MAX_COUNT = 800,
		};

		int value   = std::max(frame_w, frame_h);
		int out_val = value % MAX_COUNT;
		factor_     = value / MAX_COUNT + (out_val > 0 ? 1 : 0);

		cv::resize(bgra_frame, small_frame, cv::Size(), 1.0 / factor_, 1.0 / factor_);
		cv::cvtColor(small_frame, bgr_frame, cv::COLOR_BGRA2BGR);

		this->post(bgr_frame);
	}

	void DetectionCenter::post(cv::Mat frame)
	{
		if (heart_interval_ > 0 && std::abs(time(nullptr) - heart_timestamp_) > heart_interval_) {
			enable_all_ = false;

			notify_disabled("heart stopped");
			return;
		}

		if (enable_thread_)
		{
			std::lock_guard lk(frame_mutex_);
			frame_ = frame;
			frame_cv_.notify_one();
		}
		else {
			this->handle(frame);
		}		
	}

	void DetectionCenter::work()
	{
		while (working_) {

			cv::Mat frame;
			{
				std::unique_lock lk(frame_mutex_);
				frame_cv_.wait(lk);
				frame = std::move(frame_);
			}			

			if (!working_) break;

			try {
				this->handle(frame);
			}
			catch (const std::exception& e) {
				printf("DetectionCenter::work exception: %s", e.what());
			}
			catch (...) {
				//todo
			}
		}
	}

	void DetectionCenter::handle(cv::Mat frame)
	{
		try {
			if (frame.empty()) return;
			json11::Json result;
			face_exist_  = face_detector_->detect_head(frame, factor_, result);
			//
#if 0
			{
				std::lock_guard<std::mutex> lk{ face_result_mutex_ };
				face_result_ = std::move(result);
			}
#else
			json11::Json message = json11::Json::object{
					   {"module",  so::MODULE_NAME},
					   {"func",    "HeadPoints"},
					   {"data",    result},
					   {"code",    0},					   
			};

			proc::forward_js_message(message.dump(), head_browser_id_);
#endif
			
		}
		catch (const std::exception& /*e*/) {
			//todo
		}
		
	}

	void DetectionCenter::set_heart_interval(int interval)
	{
		if (interval > 0) {
			heart_interval_ = interval + 3;
		}
		so_info("set_heart_interval: %d", interval);
	}

	void DetectionCenter::heart()
	{
		heart_timestamp_ = time(nullptr);
	}
}
