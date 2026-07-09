#ifndef __WEB_SOCKET_LOVENSE_INSTA360_HPP__
#define __WEB_SOCKET_LOVENSE_INSTA360_HPP__

#include <memory>

#include "lovense_proc_call.hpp"
#include "json11/json11.hpp"
#include "lovense_websocket_common.hpp"


namespace insta360 {
	using namespace lovense;

	void setup_proc_handler(proc_handler* handle, ws::WebServerImpl* server);

	template <typename _Connection>
	int process_message(
		std::shared_ptr<_Connection> connection,
		const std::string& encryMsg,
		const std::string& msg,
		const json11::Json& data)
	{
		connection->send("");
		return 0;
	}
}




#endif
