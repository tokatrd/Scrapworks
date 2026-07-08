#include <map>
#include <future>
#include <sstream>

#include "obs.hpp"

#include "json/CJsonObject.hpp"
#include "common/CHttpClient.h"
#include "lovense_http_message.hpp"
#include "common/ThreadPool.h"

#include "lovense_error.hpp"
#include "lovense_websocket_helper.hpp"
#include "lovense_util_helper.hpp"
#include "lovense_secret_center.hpp"
#include "lovense_string_helper.hpp"
#include "lovense_uri.hpp"

#include "rs_common.hpp"
#include "rs_webcam_proxy.hpp"
#include "rs_helper.hpp"

static const std::string WEBCAM_PROXY_URL = "";

namespace lovense
{
	namespace rs
	{
		WebcamProxy* WebcamProxy::instance()
		{
			static WebcamProxy client;
			return &client;
		}

		WebcamProxy::WebcamProxy()
			: _io(std::make_unique<sio::client>())
			, thread_pool_(1)
			
		{
			using std::placeholders::_1;
			using std::placeholders::_2;

			handlers_["360"] = std::bind(&WebcamProxy::on_webcam_message, this, _1, _2);
		}

		bool WebcamProxy::init()
		{
			_io->set_reconnect_attempts(2);
			_io->set_fail_listener(std::bind(&WebcamProxy::on_failed, this));

			using std::placeholders::_1;		
		
			_io->set_open_listener(std::bind(&WebcamProxy::on_connected, this));
			_io->set_close_listener(std::bind(&WebcamProxy::on_closed, this, _1));
			_io->set_fail_listener(std::bind(&WebcamProxy::on_failed, this));
			
			return true;
		}

		void WebcamProxy::init_proxy_info(proc::proxy_token_callback callback)
		{
			{
				std::unique_lock<std::mutex> lock(token_callback_mutex_);
				token_callback_ = callback;
			}			
		}

		void WebcamProxy::set_proxy_token_callback(proc::proxy_token_callback callback)
		{
			{
				std::unique_lock<std::mutex> lock(token_callback_mutex_);
				token_callback_ = callback;
			}
		}

		void WebcamProxy::shutdown()
		{
			if (_io) {
				_io->clear_socket_listeners();
				_io->socket()->off_all();
				_io->socket()->off_error();
				_io->clear_con_listeners();
				_io->sync_close();
			}
			thread_pool_.Stop();
		}

		void WebcamProxy::release()
		{
			_io->close();
		}

		void WebcamProxy::on_connected()
		{
			{
				status_ = e_Connected;

				std::unique_lock<std::mutex> lk(connect_lock_);
				connect_cv_.notify_all();
			}

			using std::placeholders::_1;
			using std::placeholders::_2;
			using std::placeholders::_3;
			using std::placeholders::_4;
			auto sock = _io->socket();

			sock->on_error([](sio::message::ptr const& message) {
				auto m = message->get_flag();
				});
		}

		void WebcamProxy::on_closed(sio::client::close_reason const& reason)
		{
			//status_ = e_SocketAddress;
			status_ = e_Init;

			std::unique_lock<std::mutex> lk(connect_lock_);
			connect_cv_.notify_all();

			blog(LOG_INFO, "[lovense relay server]on_closed reason: %d", reason);
		}

		void WebcamProxy::on_failed()
		{
			//status_ = e_Error;
			status_ = e_Init;
			{
				std::unique_lock<std::mutex> lk(connect_lock_);
				connect_cv_.notify_all();
			}			

			proc::call_BiLog("Server", "W0003", "connect webcam proxy server failed");
			blog(LOG_INFO, "[lovense relay server]connect webcam proxy server failed");
		}

		void WebcamProxy::connect()
		{
			if (status_.load() < e_SocketAddress) {
				return;
			}
			_io->socket()->off_all();
			_io->close();

			status_.store(e_Connecting);

			{
				std::unique_lock<std::mutex> lock(socket_io_mutex_);
				_io->connect(this->socket_io_url_, this->socket_io_query_);
			}
		}

		//-------------------------------------------------------------------------------------
		void WebcamProxy::send_message(const std::string& message)
		{
			if (!_io->opened()) {
				return;
			}

			neb::CJsonObject root;
			root.Add("data", message);

			_io->socket()->emit("webcam_one_to_many_ts", root.ToString());

		}

		void WebcamProxy::execute_token_callback(std::string token, int code, std::string error)
		{
			std::unique_lock<std::mutex> lock(token_callback_mutex_);
			if (token_callback_) {
				token_callback_(token, code, error);
			}			
		}

		void WebcamProxy::refresh_token()
		{
			blog(LOG_INFO, "[lovense-replay-server] call refresh_token status: %d", status_.load());
			thread_pool_.enqueue([this]() {
				error::Code code{ error::Code::lvs_Ok };
				std::string err_message;
				std::string token;

				auto s = status_.load();
				if (e_SocketAddress > s || s == e_Error)
				{
					if (!this->init_webcam_socket_info(err_message))
					{
						code = error::Code::lvs_Proxy_Handle;
					}
				}

				if (code == error::Code::lvs_Ok)
				{
					if (! _io->opened())
					{
						this->connect();
					}									

					if (!this->request_webcam_token(token, err_message))
					{
						code = error::Code::lvs_Proxy_Handle;
					}
					else {
						{							
							std::unique_lock<std::mutex> lk(connect_lock_);
							if (!connect_cv_.wait_for(lk, std::chrono::seconds(60), [&] {

								auto s = status_.load();
								if (s != e_Connecting)
								{
									code = (status_.load() == e_Connected) ? error::Code::lvs_Ok : error::Code::lvs_Proxy_Connect;
									return true;
								}

								return false;
							}))
							{
								code = (status_.load() == e_Connected) ? error::Code::lvs_Ok : error::Code::lvs_Proxy_Connect;
							}
						}
					}
				}

#ifdef _TEST_BUILD
				blog(LOG_INFO, "[lovense-replay-server] test refresh_token: %d %s", code, err_message.c_str());
#else
				blog(LOG_INFO, "[lovense-replay-server] refresh_token: %d %s", code, err_message.c_str());
#endif

				this->execute_token_callback(token, (int)code, err_message);
			});		
		}

		void WebcamProxy::on_control_connected(
			std::string const& name, sio::message::ptr const& data, bool hasAck, sio::message::list& ack_resp)
		{
			this->refresh_token();
		}

		void WebcamProxy::on_message(
			std::string const& name, sio::message::ptr const& data, bool hasAck, sio::message::list& ack_resp)
		{
			if (data->get_flag() != sio::message::flag_string)
			{
				return;
			}
		
			std::string msg = data->get_string();

			neb::CJsonObject json_obj;
			if (! json_obj.Parse(msg)) {
				blog(LOG_WARNING, "[lovense-replay-server] parse json err: %s", json_obj.GetErrMsg().c_str());
				return;
			}

			std::string data_str;
			json_obj.Get("data", data_str);

			neb::CJsonObject data_obj;
			data_obj.Parse(data_str);

			std::string s = data_obj.ToString();
			std::string t_n;
			data_obj.Get("type", t_n);

			std::string type_name;
			if (!data_obj.Get("type", type_name) || type_name.empty()) {
				return;
			}

			data_obj.Add("is_proxy", true);
			auto func_it = handlers_.find(type_name);
			if (func_it != handlers_.end()) {
				func_it->second(data_str, data_obj);
				return;
			}			
		}

		//-----------------------------------------------------------------------------------
		void WebcamProxy::on_webcam_message(std::string message, neb::CJsonObject json_obj)
		{			
			proc::forward_webcam_message(json_obj.ToString());
		}

		bool WebcamProxy::init_webcam_socket_info(std::string& err_message)
		{
			try {

				bool ok{ false };
				std::string url = WEBCAM_PROXY_URL;

				std::string token = get_app_token();				
				std::stringstream formData;
				formData << "cid=" << lovense::urlencode(token);

				std::string response;
				std::list<std::string> empty_header;
				std::string post_data = formData.str();
				int code = this->http_post(url, post_data, response);
				if (code == 0)
				{
#ifdef _TEST_BUILD
					blog(LOG_WARNING, "[lovense-replay-server] test initSocket response: %s", response.c_str());
#endif
					http::MessageReader reader(response);
					if (reader)
					{
						std::string url, io_path;
						reader.Get("wsUrl", url);
						reader.Get("socketIoPath", io_path);

						if ((!url.empty()) && (!io_path.empty()))
						{						

							try {
								lovense::Uri uri(url);							

								if (io_path.size() > 0 && io_path[io_path.size()-1] != '/')
								{
									io_path.push_back('/');
								}
								std::unique_lock<std::mutex> lock(socket_io_mutex_);
							
								this->socket_io_url_ = uri.scheme() + ":" + uri.path() + io_path;
								for (auto it : uri.getQueryParams())
								{
									this->socket_io_query_[it.first] = it.second;
								}
								ok = true;
							}
							catch (std::exception&)
							{
								err_message = "proxy server resp socket uri error";
							}
						}
						else {
							err_message = "proxy server resp socket info error";
						}
					}
					else {
						err_message = reader.err_message();
						if (err_message.empty())
						{
							err_message = "parse resp message error";
						}
					}
				}
				else {
					err_message = string_util::format("curl code: %d", code);
				}

				status_ = err_message.empty() ? e_SocketAddress : e_Init;

		
				return ok;
			}
			catch (...) {
				err_message = "get webcam_socket_info exception";
				return false;
			}
		}

		bool WebcamProxy::request_webcam_token(std::string& proxy_token, std::string& err_message)
		{
			try {
				bool ok{ false };
				std::string url = WEBCAM_PROXY_URL;

				std::string token = get_app_token();
				token = lovense::EncryptionAES(token, aes_key_.c_str(), aes_iv_.c_str());
				std::stringstream formData;
				formData << "cid=" << lovense::urlencode(token);

				std::string response;				
				std::list<std::string> empty_header;
				std::string post_data = formData.str();
				int code = this->http_post(url, post_data, response);
				if (code == 0)
				{
#ifdef _TEST_BUILD
					blog(LOG_WARNING, "[lovense-replay-server] test genQrCode response: %s", response.c_str());
#endif
					http::MessageReader reader(response);
					if (reader)
					{
						std::string server_token;
						reader.Get("token", server_token);

						if ((!server_token.empty()))
						{
							proxy_token = server_token;
							//blog(LOG_INFO, "proxy_token: %s", lovense::urlencode(server_token).c_str());
#if _DEBUG
							printf("proxy_token: %s\n", lovense::urlencode(server_token).c_str());
#endif
							ok = true;
						}
						else {
							err_message = "proxy server resp token error";
						}
					}
					else {
						err_message = reader.err_message();
					}
				}
				else {
					err_message = string_util::format("curl code: %d", code);
				}

				return ok;
					
			}
			catch (...) {
				err_message = "get webcam_token exception";
				return false;
			}
		}

		static size_t OnWriteData(void* buffer, size_t size, size_t nmemb, void* lpVoid) {
			std::string* str = dynamic_cast<std::string*>((std::string*)lpVoid);
			if (NULL == str || NULL == buffer)
			{
				return -1;
			}

			char* pData = (char*)buffer;
			str->append(pData, size * nmemb);
			return nmemb;
		};

		int WebcamProxy::http_post(const std::string& strUrl, const std::string& strPost, std::string& strResponse)
		{
			int ret = -1;
			CURL* curl;
			curl = curl_easy_init();
			if (curl) {
				curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
				curl_easy_setopt(curl, CURLOPT_URL, strUrl.c_str());
				curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
				curl_easy_setopt(curl, CURLOPT_DEFAULT_PROTOCOL, "https");
				struct curl_slist* headers = NULL;
				headers = curl_slist_append(headers, "User-Agent: stream master");
				headers = curl_slist_append(headers, "Accept: */*");
				headers = curl_slist_append(headers, "Connection: keep-alive");
				curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

				curl_easy_setopt(curl, CURLOPT_POSTFIELDS, strPost.c_str());
				curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, OnWriteData);
				curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&strResponse);
				curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 8);
				curl_easy_setopt(curl, CURLOPT_TIMEOUT, 8);
				ret = curl_easy_perform(curl);
			}
			curl_easy_cleanup(curl);

			return ret;
		}
	}
}
