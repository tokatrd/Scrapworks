#pragma once
#include <obs-frontend-api.h>
#include "BlogDispatcher.h"
#include "lovense_util_helper.hpp"
#include "lovense_string_helper.hpp"

#include "lovense_websocket_common.hpp"
#include "lovense_websocket_error.hpp"
#include "lovense_websocket_protocol.hpp"
#include "lovense_smart_server.hpp"
#include "lovense_websocket_helper.hpp"
#include "lovense_obs_common.hpp"
#include "lovense_obs_proc.hpp"
#include "lovense_obs_helper.hpp"
#include "lovense_certificate_manager.hpp"

#include "lovense_camera_handler.hpp"
#include "lovense_insta360_handler.hpp"
#include "lovense_noise_gate_handler.hpp"
#include "rs_stream_handler.hpp"

#include "rs_common.hpp"
#include "rs_proc_handler.hpp"


namespace lovense::rs
{
	using namespace lovense::ws;

	static std::string create_success_response(const char* type)
	{
		auto code = error::Code::lvs_Ok;
		neb::CJsonObject obj;
		obj.Add("code", code);
		obj.Add("type", type);
		std::string err_message = error::message(code);
		obj.Add("message", err_message);
		return obj.ToString();
	}

#ifdef DEF_TOOLSETS
#ifdef _WIN32
	static HHOOK g_hHook = nullptr;
	static LRESULT CALLBACK CBTHookProc(int nCode, WPARAM wParam, LPARAM lParam)
	{
		if (nCode == HCBT_ACTIVATE) {
			HWND hYes = GetDlgItem((HWND)wParam, IDYES);
			RECT rcYes;
			GetWindowRect(hYes, &rcYes);
			POINT ptYes = { rcYes.left, rcYes.top };
			ScreenToClient((HWND)wParam, &ptYes);

			MoveWindow(hYes, ptYes.x - (rcYes.right - rcYes.left), ptYes.y,
				(rcYes.right - rcYes.left) * 2,
				rcYes.bottom - rcYes.top, TRUE);
			SetWindowText(hYes, L"Save and start streaming");

			HWND hNo = GetDlgItem((HWND)wParam, IDNO);
			SetWindowText(hNo, L"Save");
			HWND hCancel = GetDlgItem((HWND)wParam, IDCANCEL);
			SetWindowText(hCancel, L"Cancel");
			UnhookWindowsHookEx(g_hHook);
			g_hHook = nullptr;
		}
		return 0;
	}
#endif // WIN32
#endif	

	//----------------------------------------------------------------------------------------------
	template<typename socket_type> class ObsProxy : public lovense::smart::Proxy<socket_type> {

		using ProxyServer = socket_type;

	private:
		int process_message_module(std::shared_ptr<typename ProxyServer::Connection> connection,
			const std::string& encryMsg,
			const std::string& msg,
			const json11::Json& data)
		{		
			return 0;
		}

		//---------------------------------------------------------------------------		
		int process_message_110(
			std::shared_ptr<typename ProxyServer::Connection> connection,
			const std::string& encryMsg,
			const std::string& msg,
			const json11::Json& root)
		{
			return 0;
		}

		int process_message_119(
			std::shared_ptr<typename ProxyServer::Connection> connection,
			const std::string& encryMsg,
			const std::string& msg,
			const json11::Json& root)
		{
			return 0;
		}

		int process_message_120(
			std::shared_ptr<typename ProxyServer::Connection> connection,
			const std::string& encryMsg,
			const std::string& msg,
			const json11::Json& root)
		{

			return 0;
		}

		int process_message_rtmp_common(
			std::shared_ptr<typename ProxyServer::Connection> connection,
			const std::string& encryMsg,
			const std::string& msg,
			const json11::Json& root)
		{			
			return 0;
		}

		int process_message_multi_rtmp_common(
			std::shared_ptr<typename ProxyServer::Connection> connection,
			const std::string& encryMsg,
			const std::string& msg,
			const json11::Json& data)
		{			
			return 0;
		}

		int process_message_108(
			std::shared_ptr<typename ProxyServer::Connection> connection,
			const std::string& encryMsg,
			const std::string& msg,
			const json11::Json& root)
		{			
			return 0;
		}
	public:
		ObsProxy(
			std::shared_ptr<ProxyServer>   server,
			int                            port,
			smart::PluginInfo              plugin
		) : lovense::smart::Proxy<socket_type>(server, port, plugin)
		{

		}

		void SetupOBSProcHandler() {

			proc_handler* handle = obs_get_proc_handler();
			if (handle) {
				camera::setup_proc_handler(handle, this);
				insta360::setup_proc_handler(handle, this);
			}	
		}

		void Start() override
		{
			//Register a call process for communication with lovense-ai-camera.dll
			this->SetupOBSProcHandler();

			lovense::smart::Proxy<socket_type>::Start();
		}
	protected:
		void on_start(const std::string& err_message) override
		{
			if (err_message.empty()) {
				if (std::is_same<ProxyServer, smart::WsServer>::value) {
					lovense::proc::report_log("start", "R0013", "Toolset Server 2.0 Start[%d] Success!", this->port());
				}
			}
			else
			{
				lovense::proc::report_log("start", "R0014", "websocket exception: %s port: %d", err_message.c_str(), this->port());
			}
		}

		void on_open_client(int client_type, std::shared_ptr<typename ProxyServer::Connection> connection) override
		{
			if (client_type == lovense::ws::CLIENT_FROM_SMART) {
				lovense::proc::report_log("start", "R0017", "client smart[dist] on open success");
			}			
		}

		std::string get_dist_version() override
		{
#ifdef DEF_TOOLSETS
			std::string dist_version;
			std::string toolset_version;
			rs::get_version(dist_version, toolset_version);

			return dist_version;
#else
			return lovense::smart::Proxy<socket_type>::get_dist_version();
#endif
		}

		std::string get_toolset_version() override
		{
#ifdef DEF_TOOLSETS
			std::string dist_version;
			std::string toolset_version;
			rs::get_version(dist_version, toolset_version);

			return toolset_version;
#else
			return lovense::smart::Proxy<socket_type>::get_toolset_version();
#endif
		}
	
		std::string pack_105_message(bool smartOnline, bool camOnlien,
			const std::string tSetVersion,
			const std::string pVersion) override
		{
			return "";
		}
	};

}
#pragma once
