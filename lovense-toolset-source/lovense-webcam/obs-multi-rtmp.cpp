#include "pch.h"

#include <list>
#include <regex>
#include <filesystem>
#include "form/MainControlDockWidget.h"
#include "UntilHelp.h"
#include "PositionTokensData.h"
#include "json/CJsonObject.hpp"
#ifdef _WIN32
#include <Windows.h>
#endif

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("lovense-webcam", "en-US")

static class GlobalServiceImpl : public GlobalService
{
public:
    bool RunInUIThread(std::function<void()> task) override {
        if (uiThread_ == nullptr)
            return false;
        QMetaObject::invokeMethod(uiThread_, [func = std::move(task)]() {
            func();
        });
        return true;
    }

    QThread* uiThread_ = nullptr;
    std::function<void()> saveConfig_;
} s_service;


GlobalService& GetGlobalService() {
    return s_service;
}

static std::string GetPluginPath()
{
    std::string model_path = obs_get_module_binary_path(obs_current_module());
    model_path = model_path.substr(0, model_path.find_last_of('/'));
    return model_path;
}

extern void OnReciveWsMessage(const std::string& msg);
void OnRecivWebsocketMessage(void* data, calldata_t* callData)
{
    if (callData) {
        const char* value = nullptr;
        bool ret = calldata_get_string(callData, "msg", &value);
        if (ret)
        {
            blog(LOG_INFO,"[Insta360] %s",value);
            OnReciveWsMessage(value);
        }
    }
}

void OnSendWebsocketMessage(const std::string &msg) {
    calldata_t cData;
    calldata_init(&cData);
    proc_handler_t* handle = obs_get_proc_handler();
    if (handle)
    {
        calldata_set_string(&cData, "msg", msg.c_str());
        proc_handler_call(handle, "OnProcRecivInsta360Data", &cData);
        proc_handler_call(handle, "OnWSSProcRecivInsta360Data", &cData);
    }
    calldata_free(&cData);
}


static void SetupOBSProc() {
    proc_handler_t* handle = obs_get_proc_handler();
    if (handle) {
        proc_handler_add(handle, "void OnRecivWebsocketMessage(string json)", OnRecivWebsocketMessage, NULL);
    }
}

void saveConfig()
{
	TokenVector rules;
	GetAllTokenInfo(rules);
	auto profile_config = obs_frontend_get_profile_config();
	neb::CJsonObject arrays;
	for (auto item : rules) {
		neb::CJsonObject token;
		token.Add("name", item.second->m_position_name);
		token.Add("token", item.second->m_token_value);
		token.Add("duration", item.second->m_duration);
		token.Add("panvalue", item.second->m_panvalue);
		token.Add("titlvalue", item.second->m_titlvalue);
		token.Add("zoom", (int)item.second->m_zoomvalue);
		token.Add("index", (int)item.second->m_index);
		arrays.Add(token);
	}
	config_set_string(profile_config, "lovense-webcam", "json",
			  arrays.ToString().c_str());
	config_save_safe(profile_config, "tmp", "bak");
}

bool obs_module_load()
{
    // check obs version
#if (LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(28, 0, 0))
	if (obs_get_version() < MAKE_SEMANTIC_VERSION(28, 0, 0))
#else
	if (obs_get_version() < MAKE_SEMANTIC_VERSION(26, 1, 0))
#endif
	{
		return false;
	}

    SetupOBSProc();

	obs_frontend_push_ui_translation(obs_module_get_string);
	
    UntilRes::SetPluginPath(GetPluginPath());

    auto obs_window = (QMainWindow*)obs_frontend_get_main_window();
    if (obs_window == nullptr)
        return false;
    QMetaObject::invokeMethod(obs_window, []() {
        s_service.uiThread_ = QThread::currentThread();
    });

    auto dock = new MainControlDockWidget(obs_window);
    dock->setObjectName("lovense-insta-360-dock2");
    dock->setVisible(false);
#if 0
    auto action = (QAction*)obs_frontend_add_dock(dock);
    auto obsWin = (QMainWindow*)obs_frontend_get_main_window();
#if 1
    obsWin->addDockWidget(Qt::DockWidgetArea::RightDockWidgetArea, dock);
    dock->setVisible(false);
#else
    obs_frontend_add_dock(dock);
    dock->setVisible(true);
#endif
#endif
    //QObject::connect(action, &QAction::toggled, dock, &MultiOutputWidget::visibleToggled);
    obs_frontend_add_event_callback([](enum obs_frontend_event event, void* private_data) {
            auto dock = static_cast<MainControlDockWidget*>(private_data);

            if (event == obs_frontend_event::OBS_FRONTEND_EVENT_EXIT)
            {
				dock->OnOBSExitEvent();
				saveConfig();
            }
            else if (event == obs_frontend_event::OBS_FRONTEND_EVENT_FINISHED_LOADING) {
				auto obsWin = (QMainWindow*)obs_frontend_get_main_window();
				obsWin->addDockWidget(Qt::DockWidgetArea::RightDockWidgetArea, dock);
				dock->OnOBSFinishedLoadingEvent();

            }
#ifndef DEF_TOOLSETS
			else if (event == obs_frontend_event::OBS_FRONTEND_EVENT_WINDOWS_SHOW_CHANGED) {
#if 0
				QMainWindow* obsWin = (QMainWindow*)obs_frontend_get_main_window();
				bool bShow = !obsWin->isHidden() && !obsWin->isMinimized();

				if (dock->isVisible() == bShow) {
					return;
				}

				dock->blockSignals(true);
				dock->setVisible(bShow);
				dock->blockSignals(false);
#endif
			}
#endif
       
        }, dock);

    return true;
}

void obs_module_unload()
{
	obs_frontend_pop_ui_translation();
}


const char *obs_module_description(void)
{
    return "lovense insta 360 control";
}
