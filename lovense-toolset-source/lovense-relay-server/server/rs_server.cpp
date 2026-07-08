#include "obs.hpp"
#include "obs-frontend-api.h"
#include "util/util.hpp"

#include "lovense_obs_common.hpp"
#include "lovense_common.hpp"
#include "lovense_string_helper.hpp"
#include "lovense_websocket_common.hpp"
#include "lovense_websocket_helper.hpp"

#include "LovenseUntil.h"
#include "rs_server.hpp"
#include "rs_helper.hpp"
#include "rs_common.hpp"

#include "rs_proxy.hpp"

namespace lovense::rs
{

	//-------------------------------------------------------
	static ObsServer server_impl;

	//-------------------------------------------------------
	void __lvs_OnProcRecivData(ws::WsocketType socket_t, calldata_t* data)
	{
		if (!data) return;

		const char* value       = nullptr;
		long long   client_type = 0;
		bool ok{ false };
		ok = calldata_get_string(data, "msg", &value);
		if (!ok || value == nullptr) return;
		ok = calldata_get_int(data, "client_type", &client_type);
		if (!ok) return;
		{
			ForwardMessage((ws::WsocketType)socket_t, value, (int)client_type, true);
		}
	}

	void __lvs_ws_OnProcRecivData(void* param, calldata_t* data)
	{
		__lvs_OnProcRecivData(ws::WsocketType::WS_SOCKET, data);
	}

	void __lvs_wss_OnProcRecivData(void* param, calldata_t* data)
	{
		__lvs_OnProcRecivData(ws::WsocketType::WSS_SOCKET, data);
	}


	static void __lvs_ws_GetPort(void* param, calldata_t* data)
	{
		uint32_t* port_result{ nullptr };
		auto ok = calldata_get_ptr(data, "result", &port_result);
		if (!ok || port_result == nullptr) {
			return;
		}
		*port_result = GetWsPort();
	}

	static void __lvs_wss_GetPort(void* param, calldata_t* data)
	{
		uint32_t* port_result{ nullptr };
		auto ok = calldata_get_ptr(data, "result", &port_result);
		if (!ok || port_result == nullptr) {
			return;
		}
		*port_result = GetWssPort();
	}

	static void setup_obs_handler()
	{
		proc_handler* handle = obs_get_proc_handler();
		if (!handle) {
			lovense::log::warn("obs_get_proc_handler failed");
			return;
		}


		static int wss_t = 1;
		{
			proc_handler_add(handle,
				"void __lvs_wss_GetPort()",
				__lvs_wss_GetPort, (void*)&wss_t
			);

			proc_handler_add(handle,
				"void __lvs_wss_OnProcRecivData(string json)",
				__lvs_wss_OnProcRecivData, (void*)&wss_t
			);
		}


		static int ws_t = 0;
		{
			proc_handler_add(
				handle,
				"void __lvs_ws_GetPort()",
				__lvs_ws_GetPort, (void*)&ws_t
			);

			proc_handler_add(
				handle,
				"void __lvs_ws_OnProcRecivData(string json)",
				__lvs_ws_OnProcRecivData, (void*)&ws_t
			);
		}
	}

	//-------------------------------------------------------
	void InitServerModel()
	{
		smart::Server::Config config;		
	
#ifdef DEF_TOOLSETS
		std::string dist_version, version;
		rs::get_version(dist_version, version);

		config.app_name                  = "obs toolset";
		config.ws_port                   = DEF_TOOLSET_WS_WEBSOCKET_PORT;
		config.wss_port                  = DEF_TOOLSET_WSS_WEBSOCKET_PORT;
		config.port_strategy             = smart::Server::PORT_INC10;
		config.plugin.dist_version       = dist_version;
		config.plugin.toolset_version    = version;		
#else
		config.app_name                  = "stream master";
		config.ws_port                   = DEF_WS_WEBSOCKET_PORT;
		config.wss_port                  = DEF_WSS_WEBSOCKET_PORT;
		config.port_strategy             = smart::Server::PORT_STATIC;
		config.plugin.dist_version       = GetStreamMasterObsVersion();
		config.plugin.toolset_version    = config.plugin.dist_version;
#endif	
		std::string cert_dir             = lovense::ws::GetCertDir();
		if (!cert_dir.empty()) {
			config.cert_path             = cert_dir;
		}

		server_impl.initialize(config);	

#ifdef DEF_TOOLSETS
		int ws_port                      = server_impl.get_ws_port();
		int wss_port                     = server_impl.get_wss_port();
		auto profile_config              = obs_frontend_get_profile_config();
		if (profile_config) { // compatible with old version
			config_set_int(profile_config, "lovense", "wsport", ws_port);
			config_set_int(profile_config, "lovense", "wssport", wss_port);
		}
#endif

		setup_obs_handler();
	}

	void StartServerModel()
	{
		server_impl.start();
	}

	void StopServerModel()
	{
		server_impl.shutdown();
	}

	int GetWsPort()
	{
		return server_impl.get_ws_port();
	}

	int GetWssPort()
	{
		return server_impl.get_wss_port();
	}

	void ForwardMessage(ws::WsocketType socket_t, const std::string& msg, int clientType, bool is_encryption)
	{
		auto server = server_impl.server(socket_t);
		if (server) {
			server->SendMessageToClient(msg, clientType, is_encryption);
		}
	}

	//---------------------------------------------------------------------------------
	std::shared_ptr<ws::WebServerImpl> ObsServer::create_server_impl(ws::WsocketType ws_type)
	{
		if (ws_type == ws::WsocketType::WS_SOCKET) {

			int port = config_.ws_port;
			try {
				if (port != 0) {
					return std::shared_ptr<rs::ObsProxy<smart::WsServer>>(
						new rs::ObsProxy<smart::WsServer>(
							std::shared_ptr<smart::WsServer>(new smart::WsServer()),
							port, config_.plugin));
				}
				else
				{
					lovense::log::warn("ws server create failed. ws_port: %d", port);
				}
			}
			catch (const std::exception& e)
			{
				lovense::log::warn("ws server create failed. ws_port: %d, exception: %s", port, e.what());
			}

		}
		else if (ws_type == ws::WsocketType::WSS_SOCKET)
		{
			int port = config_.wss_port;

			if (!certificate_manager_) return nullptr;

			std::string cer_path;
			std::string contents;
			bool ok  = certificate_manager_->get_public_file(cer_path);
			     ok &= certificate_manager_->get_private_key(contents);

			if (ok) {
				try {
					boost::asio::const_buffer private_buffer(&contents[0], contents.size());
					if (port > 0) {
						return std::shared_ptr<rs::ObsProxy<smart::WssServer>>(
							new rs::ObsProxy<smart::WssServer>(std::shared_ptr<smart::WssServer>(
								new smart::WssServer(cer_path, private_buffer)),
								port, config_.plugin));
					}
					else {
						rs_warn("wss server not start. port: %d", port);
					}
				}
				catch (const std::exception& e) {
					rs_warn("wss server exception:%s", e.what());
				}
			}
			else {
				rs_warn("wss server not start. get secret key failed");
			}
#if 0
			try {
				if (config_.cert_path.empty()) return nullptr;

				std::filesystem::path cert_file = config_.cert_path / DEF_CRT_FILE_NAME;
				std::filesystem::path pem_file  = config_.cert_path / DEF_PEM_FILE_NAME;

				std::error_code code;
				if (std::filesystem::exists(cert_file, code) && std::filesystem::exists(pem_file, code) && port > 0)
				{
					return std::shared_ptr<rs::ObsProxy<smart::WssServer>>(
						new rs::ObsProxy<smart::WssServer>(std::shared_ptr<smart::WssServer>(
							new smart::WssServer(cert_file.string().c_str(), pem_file.string().c_str())),
							port, config_.plugin));
				}
				else {
					lovense::log::warn("wss server not start. certFile: %s pemFile: %s port: %d"
						, cert_file.c_str(), pem_file.c_str(), port);
					return nullptr;
				}
			}
			catch (const std::exception& e)
			{
				lovense::log::warn("wss server create failed. ws_port: %d, exception: %s", port, e.what());
			}
#endif
		}

		return nullptr;
	}

	//-------------------------------------------
}
