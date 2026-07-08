#include "lovense_insta360_handler.hpp"
#include "lovense_websocket_common.hpp"
#include "LovenseUntil.h"
#include "rs_server.hpp"

namespace insta360 {

	using namespace lovense;

	static void OnProcRecivInsta360Data(void* param, calldata_t* data)
	{
		OnProcReceivedData(ws::WsocketType::WS_SOCKET, data, lovense::ws::CLIENT_FROM_CAM, "msg");
	}

	static void OnWSSProcRecivInsta360Data(void* param, calldata_t* data)
	{
		OnProcReceivedData(ws::WsocketType::WSS_SOCKET, data, lovense::ws::CLIENT_FROM_CAM, "msg");
	}

	void setup_proc_handler(proc_handler* handle, ws::WebServerImpl* server)
	{
		if (server->SocketType() == ws::WsocketType::WSS_SOCKET)
		{
			static int socket_t = 1;
			proc_handler_add(
				handle,
				"void OnWSSProcRecivInsta360Data(string json)",
				OnWSSProcRecivInsta360Data,
				(void*)&socket_t);
		}
		else {
			static int socket_t = 0;
			proc_handler_add(handle,
				"void OnProcRecivInsta360Data(string json)",
				OnProcRecivInsta360Data,
				(void*)&socket_t);
		}
	}

	
}


