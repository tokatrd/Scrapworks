#include <memory>
#include <mutex>
#include <filesystem>

#include <dlib/opencv.h> 
#include <dlib/image_processing/frontal_face_detector.h>
#include <dlib/image_processing/render_face_detections.h>
#include <dlib/image_processing.h>
#include <dlib/gui_widgets.h>
#include <dlib/image_io.h>

#include "obs-module.h"

#include "lovense_error.hpp"
#include "lovense_obs_helper.hpp"

#include "so-common.hpp"
#include "so-dlib-impl.hpp"

namespace lovense::so
{
	DlibDetector::DlibDetector()
	{
	}

	DlibDetector::~DlibDetector()
	{
	}

	bool DlibDetector::initialize()
	{	
		init_future_ = std::async(std::launch::async, [&]() {
			int code{0};
			try {
				char* effect_path = obs_module_file("model/shape_predictor_68_face_landmarks.dat");				
				if (effect_path) {
					face_detector_ = dlib::get_frontal_face_detector();
					dlib::deserialize(effect_path) >> shape_predictor_;
					if ((shape_predictor_.num_parts() > 0)) {
						code = (int)error::lvs_Ok;
					}
					else {
						so_warn("dlib shape_predictor_68_face_landmarks not valid");
						code = (int)error::lvs_ParamsBad;
					}
				}
				else {
					so_warn("dlib shape_predictor_68_face_landmarks not exist");
					code    = (int)error::lvs_ReadFile;
				}				
			}
			catch (const std::exception& e) {
				so_warn("dlib load shape_predictor_68_face_landmarks failed: %s", e.what());
				code     = (int)error::lvs_Exception;
			}

			if (code == (int)error::lvs_Ok) {
				working_ = true;
			}

			return code;			
		});
		
		return true;
	}

	void DlibDetector::shutdown()
	{
		init_future_.wait();

		//todo
	}

	bool DlibDetector::is_working()
	{
		return working_;
	}

	bool DlibDetector::detect_head(cv::Mat frame, int factor, json11::Json& result)
	{
		if (!working_) return false;

		//lovense::CostLog coster("detect_head");

		dlib::cv_image<dlib::bgr_pixel> img(frame);
		std::vector<dlib::rectangle> dets = face_detector_(img);

		// Now we will go ask the shape_predictor to tell us the pose of
		// each face we detected.
		//std::vector<dlib::full_object_detection> shapes;
		//const rgb_pixel color = rgb_pixel(0, 255, 0);
		//std::vector<image_window::overlay_line> head_lines;
		//std::vector<std::vector<dlib::point>> all_points;
		json11::Json::array  all_json_points;
		for (unsigned long j = 0; j < dets.size(); ++j)
		{
			dlib::full_object_detection shape = shape_predictor_(img, dets[j]);
			int count = shape.num_parts();
			if (count != 68) {
				continue;
			}
			json11::Json::array json_points;
			//shapes.push_back(shape);
			/*
			*  The output parameter is shapes, which contains multiple shapes, each shape has 68 points, specifically:
				0~17: Chin
				17~22: Left eyebrow
				22~27: Right eyebrow
				27~31: Nose bridge
				31~36: Nose tip
				36~42: Left eye
				42~48: Right eye
				48~67: Lips (Upper lip: 48~55+60~65, Lower lip: 48+54~61+64~67)
			*/
			for (int i = 0; i < (int)shape.num_parts(); i++) {
				json_points.emplace_back(json11::Json::array{ (int)shape.part(i).x(), (int)shape.part(i).y() });
			}
			
			//Calculate 16 coordinate points on the top of the head
			const auto& it_start  = shape.part(0);
			const auto& it_end    = shape.part(16);
			dlib::point cent_chin = { (it_start.x() + it_end.x()) / 2, (it_start.y() + it_end.y()) / 2 };

			dlib::point prev_point = {
				cent_chin.x() - (shape.part(0).x() - cent_chin.x()), cent_chin.y() - (shape.part(0).y() - cent_chin.y())
			};
			//points.push_back(prev_point);
			json_points.emplace_back(json11::Json::array{ (int)prev_point.x(), (int)prev_point.y() });

			for (auto i = 1; i < 15; ++i) {
				dlib::point cur_point = {
				 cent_chin.x() - (shape.part(i).x() - cent_chin.x()), cent_chin.y() - (shape.part(i).y() - cent_chin.y())
				};
				
				prev_point = cur_point;
				//points.emplace_back(std::move(prev_point));
				json_points.emplace_back(json11::Json::array{ (int)prev_point.x(), (int)prev_point.y() });
			}

			//all_points.emplace_back(std::move(points));
			all_json_points.emplace_back(std::move(json_points));
		}

		//todo json
		result = json11::Json::object{
			{"heads", std::move(all_json_points)},			
			{"width", frame.cols},
			{"height", frame.rows},
		};

		return all_json_points.size() > 0;
	}
}
