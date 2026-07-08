#include "obs-frontend-api.h"

#include "json11/json11.hpp"

#include "lovense_error.hpp"
#include "lovense_obs_proc.hpp"

#include "so-common.hpp"
#include "so-helper.hpp"
#include "so-proc-handler.hpp"
#include "so-detection-center.hpp"

namespace lovense::so
{

	void setup_proc_hander()
	{
		proc_handler* handle = obs_get_proc_handler();
		if (handle == nullptr)
		{
			so_error("obs_get_proc_handler failed");
			return;
		}

		proc_handler_add(
			handle,
			"void __cef_lovense_smart_object_GetHeadPoints()",
			[](void* param, calldata_t* data) {

				long long identifier{ -1 };
				calldata_get_int(data, "identifier", &identifier);

				json11::Json* request{ nullptr };
				calldata_get_ptr(data, "request", &request);

				json11::Json* response{ nullptr };
				calldata_get_ptr(data, "response", &response);

				if (request && response) {
					json11::Json resp;
					json11::Json data;
					so::DetectionCenter::instance()->get_head_points(data);

					resp = json11::Json::object{
					   {"module",  (*request)["module"].string_value()},
					   {"func",    (*request)["func"].string_value()},
					   {"data",    data},
					   {"code",    0},
					   {"message", "ok"},
					};

					//proc::forward_js_message(resp.dump(), identifier);
					if (response) {
						*response = resp;
					}
				}
			}, nullptr
		);


		proc_handler_add(
			handle,
			"void __cef_lovense_smart_object_Enable()",
			[](void* param, calldata_t* data) {

				long long identifier{ -1 };
				calldata_get_int(data, "identifier", &identifier);

				json11::Json* request{ nullptr };
				calldata_get_ptr(data, "request", &request);

				json11::Json* response{ nullptr };
				calldata_get_ptr(data, "response", &response);

				if (request && response) {

					auto data = (*request)["data"];

					int  skip_frames    = data["skip_head_frames"].int_value();
					int  code           = helper::enable(skip_frames);
					int  heart_interval = data["heart_interval"].int_value();
					bool enable_face    = data["enable_head"].bool_value();

					//
					if (code == error::lvs_Ok) {
						if (enable_face) {
							DetectionCenter::instance()->enable_head((int)identifier);
						}

						if (heart_interval >= 0) {
							DetectionCenter::instance()->set_heart_interval(heart_interval);
						}
					}

					*response = json11::Json::object{
					   {"module",  (*request)["module"].string_value()},
					   {"func",    (*request)["func"].string_value()},
					   {"data",    data},
					   {"code",    code}
					};

				}
			}, nullptr
		);

		proc_handler_add(
			handle,
			"void __cef_lovense_smart_object_DisableAll()",
			[](void* param, calldata_t* data) {


				json11::Json* request{ nullptr };
				calldata_get_ptr(data, "request", &request);
								
				json11::Json* response{ nullptr };
				calldata_get_ptr(data, "response", &response);

				if (request && response) {

					
					DetectionCenter::instance()->disable_all();

					int code = helper::disable();

					*response = json11::Json::object{
					   {"module",  (*request)["module"].string_value()},
					   {"func",    (*request)["func"].string_value()},
					   {"code",    code}
					};

				}
			}, nullptr
		);

		proc_handler_add(
			handle,
			"void __cef_lovense_smart_object_DisableModule()",
			[](void* param, calldata_t* data) {


				json11::Json* request{ nullptr };
				calldata_get_ptr(data, "request", &request);

				json11::Json* response{ nullptr };
				calldata_get_ptr(data, "response", &response);

				if (request && response) {
					auto data = (*request)["data"];

					bool has_face = data["disable_head"].bool_value();
					if (has_face) {
						DetectionCenter::instance()->disable_head();
					}


					*response = json11::Json::object{
					   {"module",  (*request)["module"].string_value()},
					   {"func",    (*request)["func"].string_value()},
					   {"data",    data},
					   {"code",    error::lvs_Ok}
					};

				}
			}, nullptr
		);

		proc_handler_add(
			handle,
			"void __cef_lovense_smart_object_Heart()",
			[](void* param, calldata_t* data) {


				json11::Json* request{ nullptr };
				calldata_get_ptr(data, "request", &request);

				json11::Json* response{ nullptr };
				calldata_get_ptr(data, "response", &response);

				if (request && response) {

					DetectionCenter::instance()->heart();

					*response = json11::Json::object{
					   {"module",  (*request)["module"].string_value()},
					   {"func",    (*request)["func"].string_value()},
					   {"code",    error::lvs_Ok}
					};

				}
			}, nullptr
		);
	}

}
