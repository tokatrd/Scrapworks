#ifdef _WIN32

#include "obs.hpp"

#include "json/CJsonObject.hpp"

#include "lovense_common.hpp"
#include "lovense_obs_proc.hpp"

#include "lovense_websocket_common.hpp"
#include "lovense_websocket_protocol.hpp"

#include "rs_ipc_handler.hpp"




namespace lovense
{
	namespace rs
	{
		static std::shared_ptr<IpcMessageServer> ipc_server()
		{
			static std::shared_ptr<IpcMessageServer> ipc_server = CreateIPCServer();
			return ipc_server;
		}

		void start_ipc_server()
		{
			ipc_server()->setup_handler(IpcCmd::Cmd_NotifyUpdate, [](IpcMessage message) {
			
				int mode{-1};
				message.Get("mode", mode);

				int err_code{ 0 };
				message.Get("err_code", err_code);

				blog(LOG_INFO, "[lovense]Cmd_NotifyUpdate message code: %d. mode: %d, err_code: %d", message.code(), mode, err_code);
				proc::notify_script_updated(mode, err_code);
			});


			ipc_server()->start();
		}


		void shutdown_ipc_server()
		{
			ipc_server()->shutdown();
		}

	}
}

#endif
