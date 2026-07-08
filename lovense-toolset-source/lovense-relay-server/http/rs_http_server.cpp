#include "httplib/httplib.h"

#include "lovense_util_helper.hpp"
#include "lovense_obs_helper.hpp"

#include "rs_http_server.hpp"

namespace lovense
{
	namespace rs
	{
		class HttpServerProxy
		{
		private:
			bool _start_impl(int port)
			{
				try {
					std::filesystem::path dist_path;
					bool ok = get_dist_directory(dist_path);
					if (!ok) {
						mlog("relay-server", LOG_WARNING, "start http server: get dist failed");
						return false;
					}

					auto ret = server_impl_.set_mount_point("/dist", dist_path.string());
					if (!ret) {
						mlog("relay-server", LOG_WARNING, "mount dist directory failed");
						return false;
					}

					mlog("relay-server", LOG_INFO, "start http server port: %d", port);

					ok = server_impl_.listen("0.0.0.0", port);
					if (!ok) {
						mlog("relay-server", LOG_WARNING, "start http server failed. port: %d", port);
						return false;
					}

				}
				catch (const std::filesystem::filesystem_error& e) {
					mlog("relay-server", LOG_WARNING, "filesystem_error: %d", e.code());
					return false;
				}
				catch (const std::exception& e) {
					mlog("relay-server", LOG_WARNING, "exception: %s", e.what());
					return false;
				}
				catch (...)
				{
					mlog("relay-server", LOG_WARNING, "catch unkonwn exception");
					return false;
				}
				
				return true;
			}
		public:
			HttpServerProxy()
			{
			}

			bool start(int port)
			{	
				thread_ = std::thread([this, port]() {
					_start_impl(port);
				});

				return true;
			}

			void stop()
			{
				server_impl_.stop();
				thread_.join();
			}
		private:
			httplib::Server server_impl_;
			std::thread     thread_;

			int             port_{ 0 };
		};

		//----------------------------------------------------
		static HttpServerProxy* ServerProxy()
		{
			static HttpServerProxy inst;
			return &inst;
		}

		bool start_http_server()
		{
			return ServerProxy()->start(30458);
		}

		void stop_http_server()
		{
			ServerProxy()->stop();
		}
	}
}
