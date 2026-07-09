#pragma once

#include "lovense_smart_server.hpp"


namespace lovense::rs
{
	//----------------------------------
	void InitServerModel();
	void StartServerModel();
	void StopServerModel();

	int  GetWsPort();
	int  GetWssPort();

	void ForwardMessage(ws::WsocketType server, const std::string& msg, int clientType, bool  is_encryption = false);
	

	class ObsServer : public lovense::smart::Server
	{
	public:
		std::shared_ptr<ws::WebServerImpl> create_server_impl(ws::WsocketType ws_type) override;
	};
}
