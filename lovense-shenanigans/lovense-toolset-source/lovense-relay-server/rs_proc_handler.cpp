#include "obs.hpp"

#include "lovense_obs_proc.hpp"
#include "lovense_proc_call.hpp"
#include "rs_proc_handler.hpp"
#include "client/rs_webcam_proxy.hpp"
#include "rs_common.hpp"
#include "rs_helper.hpp"


namespace lovense
{
	namespace rs
	{
		static std::map<std::string, json11::Json> __module_switch_map;

		void setup_proc_hander()
		{
			proc_handler* handle = obs_get_proc_handler();
			if (handle == nullptr)
			{
				blog(LOG_ERROR, "[lovense-relay-server] obs_get_proc_handler failed");
				return;
			}

			//-----------------------------------------------------------------------
			proc_handler_add(
				handle,
				"void __lovense_relay_server_RefreshVersion()",
				[](void* param, calldata_t* data) {
					std::string dist_version;
					std::string toolset_version;
					rs::get_version(dist_version, toolset_version, true);
				}, nullptr
			);

			//-----------------------------------------------------------------------
			proc_handler_add(
				handle,
				"void __lovense_relay_server_RefreshProxyWebcamToken()",
				[](void* param, calldata_t* data) {				
					WebcamProxy::instance()->refresh_token();
				}, nullptr
			);

			proc_handler_add(
				handle,
				"void __lovense_relay_server_InitWebcamProxyInfo()",
				[](void* param, calldata_t* data) {
					lovense::proc::proxy_token_callback callback{ nullptr };

					calldata_get_ptr(data, "callback", &callback);

					WebcamProxy::instance()->init_proxy_info(callback);
				}, nullptr
			);

			proc_handler_add(
				handle,
				"void __lovense_relay_server_SetWebcamProxyTokenCallback()",
				[](void* param, calldata_t* data) {
					lovense::proc::proxy_token_callback callback{ nullptr };

					calldata_get_ptr(data, "callback", &callback);

					WebcamProxy::instance()->set_proxy_token_callback(callback);
				}, nullptr
			);

			proc_handler_add(
				handle,
				"void __lovense_relay_server_ReleaseWebcamProxy()",
				[](void* param, calldata_t* data) {
					std::string message;

					lovense::CallDataProxy data_proxy(data);
					WebcamProxy::instance()->release();
				}, nullptr
			);

			//-----------------------------------------------------------------------
			proc_handler_add(
				handle,
				"void __lovense_relay_server_ForwardWebcamProxyMessage()",
				[](void* param, calldata_t* data) {
					std::string message;

					lovense::CallDataProxy data_proxy(data);
					bool ok = data_proxy.get("message", message, true);
					if (ok)
					{
						WebcamProxy::instance()->send_message(message);
					}
				}, nullptr
			);

			//-----------------------------------------------------------------------			
			proc_handler_add(
				handle,
				"void __lovense_relay_server_RegisterModuleSwitchBool()",
				[](void* param, calldata_t* data) {
					try {
						std::string key;
						lovense::CallDataProxy data_proxy(data);
						bool ok = data_proxy.get("key", key, true);
						if (!ok || key.empty()) {
							return;
						}

						bool value{ false };
						data_proxy.get("value", value);
						__module_switch_map[key] = value;

					}
					catch (const std::exception& e) {
						rs_error("RegisterModuleSwitchBool exception: %s", e.what());
					}
					
				}, nullptr
			);

			proc_handler_add(
				handle,
				"void __lovense_relay_server_RegisterModuleSwitchInt()",
				[](void* param, calldata_t* data) {

					try {
						std::string key;
						lovense::CallDataProxy data_proxy(data);
						bool ok = data_proxy.get("key", key, true);
						if (!ok || key.empty()) {
							return;
						}

						int value{ 0 };
						data_proxy.get("value", value);
						__module_switch_map[key] = value;
					}
					catch (const std::exception& e) {
						rs_error("RegisterModuleSwitchInt exception: %s", e.what());
					}
					

				}, nullptr
			);

			proc_handler_add(
				handle,
				"void __lovense_relay_server_RegisterModuleSwitchString()",
				[](void* param, calldata_t* data) {

					try {
						std::string key;
						lovense::CallDataProxy data_proxy(data);
						bool ok = data_proxy.get("key", key, true);
						if (!ok || key.empty()) {
							return;
						}

						std::string value{ 0 };
						data_proxy.get("value", value);
						__module_switch_map[key] = value;
					}
					catch (const std::exception& e) {
						rs_error("RegisterModuleSwitchInt exception: %s", e.what());
					}


				}, nullptr
			);
		}

		void full_module_switch_values(json11::Json::object& values)
		{
			for (const auto& it : __module_switch_map)
			{
				try {
					values[it.first] = it.second;
				}
				catch (const std::exception& e) {
					rs_error("full_module_switch_values exception: %s", e.what());
				}
			}
		}
	}
}
