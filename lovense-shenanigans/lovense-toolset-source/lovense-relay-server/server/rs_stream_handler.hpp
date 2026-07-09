#pragma once

#include "json11/json11.hpp"
#include "rs_server.hpp"

namespace lovense::rs::stream
{
	enum RecordStatus
	{
		RECORDING_STARTING  = 1,
		RECORDING_STARTED   = 2,
		RECORDING_STOPPING  = 3,
		RECORDING_STOPPED   = 4,
		RECORDING_PAUSED    = 5,
	};

	void pack_stream_module(json11::Json::object& data);
	void handle_event(enum obs_frontend_event event, void* private_data);

	int handle_start_record(const json11::Json& data, json11::Json::object& resp, std::string& err_message);

	int handle_stop_record(const json11::Json& data, json11::Json::object& resp, std::string& err_message);

	int handle_get_record_settings(const json11::Json& data, json11::Json::object& resp, std::string& err_message);

	int handle_select_record_dir(const json11::Json& data, json11::Json::object& resp, std::string& err_message);

	int handle_open_record_dir(const json11::Json& data, json11::Json::object& resp, std::string& err_message);

	template <typename _Connection>
	int process(
		std::shared_ptr<_Connection> connection,
		const std::string&,
		const std::string&,
		const json11::Json& root)
	{
		int                  code{ 0 };
		json11::Json::object resp_data;
		std::string          err_message;
		std::string          action = root["data"]["function"].string_value();
		if (action == "start_record") {
			code = handle_start_record(root, resp_data, err_message);
		}
		else if (action == "stop_record") {
			code = handle_stop_record(root, resp_data, err_message);
		}
		else if (action == "get_record_settings") {
			code = handle_get_record_settings(root, resp_data, err_message);
		}
		else if (action == "open_record_dir") {
			code = handle_open_record_dir(root, resp_data, err_message);
		}
		else if (action == "select_record_dir") {
			code = handle_select_record_dir(root, resp_data, err_message);
		}
		else {
			code = ws::eErrorCode::LVS_WS_ERR_NON_METHOD;
			err_message = "unkonwn function";
		}

		if (err_message.empty()) {
			err_message = error::message((error::Code)code);
		}

		json11::Json::object data = root["data"].object_items();
		for (const auto& it : resp_data)
		{
			data[it.first] = it.second;
		}

		json11::Json response = json11::Json::object{
			{"type",    root["type"].string_value()},
			{"code",    code} ,
			{"data",    std::move(data)},
			{"message", err_message},
		};

		connection->send(lovense::ws::EncryptionAES(response.dump()));

		return 0;
	}
}
