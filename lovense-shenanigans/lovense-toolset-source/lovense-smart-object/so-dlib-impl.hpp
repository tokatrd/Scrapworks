#pragma once

#include <vector>
#include <memory>
#include <future>

#include "json11/json11.hpp"
#include "so-detection-center.hpp"

#include <dlib/image_processing/frontal_face_detector.h>
#include "dlib/image_processing/shape_predictor.h"

namespace dlib
{
	//class shape_predictor;
};

namespace lovense::so
{
	class DlibDetector : public HeadDetector
	{
	public:
		DlibDetector();
		~DlibDetector();

		bool initialize() override;
		void shutdown() override;

		bool is_working() override;

		bool detect_head(cv::Mat frame, int factor, json11::Json& result) override;
	private:
		dlib::frontal_face_detector  face_detector_;
		dlib::shape_predictor        shape_predictor_;

		std::future<int>             init_future_;
		std::atomic<bool>            working_{ false };
	};
}
