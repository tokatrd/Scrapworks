#ifndef __WEBSOCKET_LOVENSE_CAMERA_HANDLER_HPP__
#define __WEBSOCKET_LOVENSE_CAMERA_HANDLER_HPP__

#include <memory>

#include "lovense_error.hpp"
#include "lovense_proc_call.hpp"
#include "json11/json11.hpp"
#include "json/CJsonObject.hpp"

#include "LovenseUntil.h"
#include "lovense_websocket_protocol.hpp"
#include "lovense_websocket_helper.hpp"

#include "rs_helper.hpp"

namespace camera {
	using namespace lovense;

	void setup_proc_handler(proc_handler* handle, ws::WebServerImpl* server);


	void StartAIMode(bool bStart, std::string type);
	bool GetAIModeStatus(std::string type);

	void CallWSResetLovenseScene(std::string type);

	bool CheckAiConfigValid(std::string type = "109");

	bool ActiveOBSCamera(bool active);

	int handle_get_camera_list(const json11::Json& data, json11::Json::object& resp, std::string& err_message);

	template <typename _Connection>
	int process_message(
		std::shared_ptr<_Connection> connection,
		const std::string& encryMsg,
		const std::string& msg,
		const json11::Json& root)
	{
		connection->send("");
		return 0;
	}

	template <typename _Connection>
	int process_select_camera(
		std::shared_ptr<_Connection> connection,
		const std::string&,
		const std::string&,
		const json11::Json& root)
	{
		connection->send("");
		return 0;
	}

	template <typename _Connection>
	int process_active_camera(
		std::shared_ptr<_Connection> connection,
		const std::string&,
		const std::string&,
		const json11::Json& root)
	{
		connection->send("");
		return 0;
	}

	template <typename _Connection>
	int process_camera(
		std::shared_ptr<_Connection> connection,
		const std::string&,
		const std::string&,
		const json11::Json& root)
	{
		connection->send("");
		return 0;
	}

}



#endif // !__WEBSOCKET_LOVENSE_CAMERA_HANDLER_HPP__

