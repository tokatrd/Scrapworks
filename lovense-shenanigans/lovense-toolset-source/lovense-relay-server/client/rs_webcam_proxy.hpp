#pragma once

#include <mutex>
#include <memory>
#include <condition_variable>

#include <sio_client.h>
//#include "common/CHttpClient.h"
#include "json/CJsonObject.hpp"
#include "common/ThreadPool.h"

#include "lovense_obs_proc.hpp"

namespace lovense
{
	namespace rs
	{
		class WebcamProxy 
		{
			enum _Status
			{
				e_Init          = 0,
				e_SocketAddress = 1,
				e_Connecting    = 2,
				e_Connected     = 3,
				e_Error         = 999,
			};
		public:
			static WebcamProxy* instance();

			bool init();			
			void shutdown();
			void release();

			void refresh_token();

			void init_proxy_info(proc::proxy_token_callback callback);
			void set_proxy_token_callback(proc::proxy_token_callback callback);
			void send_message(const std::string& message);
			
		private:
			WebcamProxy();			
			//------------------------------------------------------
			void on_connected();
			void on_closed(sio::client::close_reason const& reason);
			void on_failed();

			void on_message(std::string const& name, sio::message::ptr const& data, bool hasAck, sio::message::list& ack_resp);
			void on_control_connected(std::string const& name, sio::message::ptr const& data, bool hasAck, sio::message::list& ack_resp);
			//------------------------------------------------------
			void connect();


			//------------------------------------------------------
			bool init_webcam_socket_info(std::string& err_message);
			bool request_webcam_token(std::string& token, std::string& err_message);
			
		private:
			void on_webcam_message(std::string message, neb::CJsonObject json_obj);

			int http_post(const std::string& strUrl, const std::string& strPost, std::string& strResponse);

			void execute_token_callback(std::string token, int code, std::string error);
		private:
			std::unique_ptr<sio::client> _io;
			std::string                  err_message_;

			std::map<std::string, std::function<void(std::string, neb::CJsonObject)>> handlers_;

			//
			std::string aes_key_;
			std::string aes_iv_;

			std::atomic<int> status_{ e_Init };

			//-----------------------------------------------
			std::mutex  socket_io_mutex_;
			std::string socket_io_url_;
			//std::string socket_io_path_;
			std::map<std::string, std::string> socket_io_query_;

			
			ThreadPool              thread_pool_;
			std::condition_variable connect_cv_;
			std::mutex              connect_lock_;

			std::mutex                  token_callback_mutex_;
			proc::proxy_token_callback  token_callback_;			
		};
	}
}
