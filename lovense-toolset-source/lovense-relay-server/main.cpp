
#include <list>
#include <regex>
#include <filesystem>

#ifdef _WIN32
#include <Windows.h>
#endif

#include "obs.hpp"
#include "obs-module.h"
#include "obs-frontend-api.h"
#include "util/profiler.hpp"

#include "lovense_secret_center.hpp"
#include "lovense_obs_helper.hpp"
#include "lovense_log_handler.hpp"
#include "lovense_lib_proxy.hpp"

#include "rs_proc_handler.hpp"
#include "rs_helper.hpp"
#include "rs_common_handler.hpp"

#include "client/rs_webcam_proxy.hpp"
//#include "server/smart_cam_server_model.hpp"
#include "server/rs_server.hpp"
#include "server/rs_stream_handler.hpp"

#ifdef ENABLE_HTTP_SERVER
#include "http/rs_http_server.hpp"
#endif

#ifdef ENABLE_IPC_SERVER
#include "ipc/rs_ipc_handler.hpp"
#endif

#ifdef DEF_TOOLSETS
#include "BlogDispatcher.h"
#ifdef _WIN32
    #include "toolset/lovense_toolset_custom.hpp"
#endif
#endif


using namespace lovense;

ObsLibProxy obs_lib_proxy;

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("lovense-relay-server", "en-US")
OBS_MODULE_AUTHOR("lovense")

bool initialize()
{
#if defined(DEF_TOOLSETS) && !defined(__APPLE__)

	bool ok = toolset::initialize();
	if (!ok) {
		blog(LOG_WARNING, "toolset initialize failed");
		return false;
	}
#endif

	std::string err_message;
#if defined(DEF_TOOLSETS)
	std::string dist_version, toolet_version;
	rs::get_version(dist_version, toolet_version);

	blog(LOG_INFO, "lovense toolset version:%s", toolet_version.c_str());

	//std::string version = rs::GetDistHtmlVersion();
	if (!SecretCenter::instance()->init(err_message, toolet_version))
#else
	if (!SecretCenter::instance()->init(err_message))
#endif
	{
		blog(LOG_ERROR, "lovense-relay-server secret center init failed: %s", err_message.c_str());	
	}

#ifdef DEF_TOOLSETS
	std::string email;
	std::string account_id;
	lovense::load_user_email(email);
	lovense::load_account_id(account_id);
	bool log_ok = BlogDispatcher::GetInstance()->Init(toolet_version, email);
	if (log_ok) {
		BlogDispatcher::GetInstance()->SetAccountId(account_id);
		BlogDispatcher::GetInstance()->setup_obs_log_handler();
	}

	BILOG_SYSTEM_INFO();
#endif

	if (!rs::WebcamProxy::instance()->init())
	{
		proc::call_BiLog("Server", "W0002", "proxy client init failed");
		blog(LOG_ERROR, "lovense-relay-server init failed");
	}

	rs::setup_proc_hander();

	rs::register_top_feedback_event();

	
	rs::InitServerModel();

	

	return true;
}

void on_obs_loaded()
{
	ProfileScope("lovense::relay-server:on_obs_loaded");
	try {
		rs::StartServerModel();
	}
	catch (std::exception& e) {
		blog(LOG_ERROR, "start server exctiption: %s", e.what());
	}

#ifdef ENABLE_HTTP_SERVER
	rs::start_http_server();
#endif


#ifdef DEF_TOOLSETS 
	rs::report_obs_info();
#endif // DEF_TOOLSETS 
	
	rs::report_extra_modules();

#if defined(DEF_TOOLSETS) && !defined(__APPLE__)
	toolset::on_load_end();
#endif

#ifdef ENABLE_IPC_SERVER
	rs::start_ipc_server();
#endif
}

void obs_frontend_callback_c(enum obs_frontend_event event, void* private_data){
    if (event == obs_frontend_event::OBS_FRONTEND_EVENT_FINISHED_LOADING) {
        on_obs_loaded();
    }
	else if (event == obs_frontend_event::OBS_FRONTEND_EVENT_SCENE_CHANGED) {
		//
#if 0
		rs::reset_current_scene_feedback_srgb();
#endif
	}	
    else if(event == obs_frontend_event::OBS_FRONTEND_EVENT_EXIT){

#ifdef ENABLE_IPC_SERVER
        rs::shutdown_ipc_server();
#endif

        rs::release_top_feedback_event();

        rs::WebcamProxy::instance()->shutdown();
        rs::StopServerModel();

#ifdef ENABLE_HTTP_SERVER
        rs::stop_http_server();
#endif
		rs::report_profilter_inited();
    }

	rs::stream::handle_event(event, private_data);
}

bool obs_module_load()
{
#if 0
	AllocConsole();
	freopen("CONIN$", "r", stdin);
	freopen("CONOUT$", "w", stdout);
	freopen("CONOUT$", "w", stderr);
#endif
    // check obs version
	if (obs_get_version() < MAKE_SEMANTIC_VERSION(26, 1, 0))
	{
		return false;
	}

	try {
		if (!lovense::log::is_handler()) {
			log_handler_t  obs_log_hander{ nullptr };
			void* obs_log_params{ nullptr };
			base_get_log_handler(&obs_log_hander, &obs_log_params);
			lovense::log::set_handler(obs_log_hander, obs_log_params);
			lovense::log::info("enable lovense logger");
		}	

		if (!initialize())
		{
			llog(LOG_WARNING, "initialize failed");
			return false;
		}
	}
	catch (const std::filesystem::filesystem_error& err) {
		mlog("relay-server", LOG_WARNING, "obs_module_load->filesystem_error: %d", err.code().value());
	}
	catch (const std::exception& e) {
		mlog("relay-server", LOG_WARNING, "obs_module_load->initialize exception: %s", e.what());
	}
	
    
#if defined(__APPLE__) && defined(DEF_TOOLSETS)

#else
    obs_frontend_add_event_callback(obs_frontend_callback_c, nullptr);
#endif
    
	
  
    return true;
}

void obs_module_post_load(void){
    
#if defined(__APPLE__) && defined(DEF_TOOLSETS)
    on_obs_loaded();
#endif
}

void obs_module_unload()
{
    
#if defined(__APPLE__) && defined(DEF_TOOLSETS)
    rs::release_top_feedback_event();
    rs::WebcamProxy::instance()->shutdown();
    rs::StopServerModel();
#endif

#ifdef DEF_TOOLSETS
	BlogDispatcher::GetInstance()->Release();
#endif
}




const char *obs_module_description(void)
{
    return "lovense relay server plugin";
}
