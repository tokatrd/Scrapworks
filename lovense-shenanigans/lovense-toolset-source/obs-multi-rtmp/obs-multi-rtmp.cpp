#include "pch.h"

#include <list>
#include <regex>
#include <filesystem>

#include "push-widget.h"

#ifdef _WIN32
#include <Windows.h>
#endif

#define ConfigSection "obs-multi-rtmp"

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


class MultiOutputWidget : public QDockWidget
{
    int dockLocation_;
    bool dockVisible_{true};
    bool reopenShown_;

public:
    void AddPanelWithServiceConfig(QString config) {
        QJsonObject root;
        QJsonDocument doc = QJsonDocument::fromJson(config.toStdString().c_str());
        if (!doc.isNull()) {
            if (doc.isObject()) {
                QJsonObject remote = doc.object();
                QJsonObject settings = remote.value("settings").toObject();
                if (settings.isEmpty())
                    return;
                root["name"] = settings.value("service").toString();
                root["rtmp-path"] = settings.value("server").toString();
                root["rtmp-key"] = settings.value("key").toString();
                if (ConfigIsExist(root)) {

                    return;
                }
            }
            else
                return;
        }
        else
            return;

       
        //GetGlobalService().RunInUIThread([&]() {
            auto pushwidget = createPushWidget(root, container_);
            layout_->addWidget(pushwidget);
            SaveConfig();
        //});
    }

public:
    MultiOutputWidget(QWidget* parent = 0)
        : QDockWidget(parent)
        , reopenShown_(false)
    {
        setWindowTitle(obs_module_text("Title"));
        this->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
        this->setMinimumSize(270, 300);

        // save dock location
        QObject::connect(this, &QDockWidget::dockLocationChanged, [this](Qt::DockWidgetArea area) {
            dockLocation_ = area;

            QRect rect = this->geometry();
            auto obsWin = (QMainWindow*)obs_frontend_get_main_window();
            if (area == Qt::DockWidgetArea::NoDockWidgetArea && rect.x() == 0 && rect.y() == 0) {
                
                QRect rectMain = obsWin->geometry();
                rect.setX(rectMain.x() + 50);
                rect.setY(rectMain.y() + 50);
                this->setGeometry(rect);
                
            }
            
        });

        QObject::connect(this, &QDockWidget::featuresChanged, [this](QDockWidget::DockWidgetFeatures features) {
#ifndef TOOLSET_VERSION
            //
            features &= ~QDockWidget::QDockWidget::DockWidgetFloatable;
            features &= ~QDockWidget::QDockWidget::DockWidgetMovable;
#ifdef __APPLE__
            features &= ~QDockWidget::QDockWidget::DockWidgetClosable;
#endif
            this->setFeatures(features);
#else
            this->setFeatures(features);
#endif
        });

        timer_ = new QTimer(this);
        timer_->setInterval(std::chrono::milliseconds(1000));
        QObject::connect(timer_, &QTimer::timeout, [this]() {

            is_exist_run_ = isExistRunning();            

            if (batchSwitchButton_) {
                auto btn_label = is_exist_run_ ? "Btn.BatchStop" : "Btn.BatchStart";
                batchSwitchButton_->setText(obs_module_text(btn_label));
            }
        });

        scroll_ = new QScrollArea(this);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        scroll_->move(0, 30);
#else
		scroll_->move(0, 22);
#endif

        container_ = new QWidget(this);
        layout_ = new QGridLayout(container_);
        layout_->setAlignment(Qt::AlignmentFlag::AlignTop);
        layout_->setVerticalSpacing(0);
             

        // init widget  Create new streaming target button
        auto btn_label = is_exist_run_ ? "Btn.BatchStop" : "Btn.BatchStart";
        batchSwitchButton_ = new QPushButton(obs_module_text(btn_label), container_);
        QObject::connect(batchSwitchButton_, &QPushButton::clicked, [this]() {            
            if (is_exist_run_) {
                this->batchStop();
               /* auto res = QMessageBox(QMessageBox::Icon::Information,
                    "?",
                    obs_module_text("Ques.DropDelay"),
                    QMessageBox::StandardButton::Yes | QMessageBox::StandardButton::No,
                    this
                ).exec();
                if (res == QMessageBox::Yes) {
                    this->batchStop();
                }             */       
            }
            else {                 
                this->batchStart();
            }
        });
        layout_->addWidget(batchSwitchButton_);
        layout_->addItem(new QSpacerItem(0, 10), 3, 0);
        
        // init widget  
        addButton_ = new QPushButton(obs_module_text("Btn.NewTarget"), container_);
        QObject::connect(addButton_, &QPushButton::clicked, [this]() {
            auto pushwidget = createPushWidget(QJsonObject(), container_);
            layout_->addWidget(pushwidget);
            
            if (pushwidget->ShowEditDlg())
                SaveConfig();
            else
                delete pushwidget;
        });
        layout_->addWidget(addButton_);
        
        // load config
        LoadConfig();

        scroll_->setWidgetResizable(true);
        scroll_->setWidget(container_);
        setLayout(layout_);
        resize(270, 400);
        this->setVisible(false);
        this->setFloating(false);

        if (timer_) {
            timer_->start();
        }
    }

    ~MultiOutputWidget() {
        if (timer_) {
            timer_->stop();
            delete timer_;
            timer_ = nullptr;
        }
    }

    bool isExistRunning() {
        for (int i = 0; i < layout_->count(); ++i) {
            auto w = dynamic_cast<PushWidget*>(layout_->itemAt(i)->widget());
            if (w == nullptr) {
                continue;
            }

            if (w->IsRunning()) {                
                return true;
            }
        }

        return false;
    }

    void batchStart() {
        for (int i = 0; i < layout_->count(); ++i) {
            auto w = dynamic_cast<PushWidget*>(layout_->itemAt(i)->widget());
            if (w == nullptr) {
                continue;
            }

            w->Start();
        }
    }

    void batchStop() 
    {
        for (int i = 0; i < layout_->count(); ++i) {
            auto w = dynamic_cast<PushWidget*>(layout_->itemAt(i)->widget());
            if (w == nullptr) {
                continue;
            }

            w->ForceStop();
        }
    }

    void visibleToggled(bool visible)
    {
        dockVisible_ = visible;

        if (dockVisible_) {
            this->setFloating(false);
        }
        return;

#if 0
        if (visible == false
            && reopenShown_ == false
            && !config_has_user_value(obs_frontend_get_global_config(), ConfigSection, "DockVisible"))
        {
            reopenShown_ = true;
            QMessageBox(QMessageBox::Icon::Information, 
                obs_module_text("Notice.Title"), 
                obs_module_text("Notice.Reopen"), 
                QMessageBox::StandardButton::Ok,
                this
            ).exec();
        }
#endif
    }

    bool event(QEvent *event) override
    {
        if (event->type() == QEvent::Resize)
        {
            scroll_->resize(width(), height() - 22);
        }
        return QDockWidget::event(event);
    }

    std::vector<PushWidget*> GetAllPushWidgets()
    {
        std::vector<PushWidget*> result;
        for(auto& c : container_->children())
        {
            if (c->objectName() == "push-widget")
            {
                auto w = dynamic_cast<PushWidget*>(c);
                result.push_back(w);
            }
        }
        return result;
    }

    void RemoveAll()
    {

    }

    void SaveConfig()
    {
        auto profile_config = obs_frontend_get_profile_config();
        
        QJsonArray targetlist;
        for(auto x : GetAllPushWidgets())
            targetlist.append(x->Config());
        QJsonObject root;
        root["targets"] = targetlist;
        root["visible"] = dockVisible_;
        QJsonDocument jsondoc;
        jsondoc.setObject(root);
        config_set_string(profile_config, ConfigSection, "json", jsondoc.toJson().toBase64().constData());

        config_save_safe(profile_config, "tmp", "bak");
    }

    void LoadConfig()
    {
        auto profile_config = obs_frontend_get_profile_config();

        QJsonObject conf;
        auto base64str = config_get_string(profile_config, ConfigSection, "json");
        if (!base64str || !*base64str) { // compatible with old version
            base64str = config_get_string(obs_frontend_get_global_config(), ConfigSection, "json");
        }

        if (base64str && *base64str)
        {
            auto bindat = QByteArray::fromBase64(base64str);
			auto jsondoc = QJsonDocument::fromJson(bindat);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)      			
			if (jsondoc.isEmpty() && (!bindat.isEmpty())) {
				jsondoc = QJsonDocument::fromBinaryData(bindat);
			}
#endif
            if (jsondoc.isObject()) {
                conf = jsondoc.object();

                // load succeed. remove all existing widgets
                for (auto x : GetAllPushWidgets())
                    delete x;
            }

        }
        else {            
            is_first_ = true;
        }

        auto targets = conf.find("targets");
        if (targets != conf.end() && targets->isArray())
        {
            for(auto x : targets->toArray())
            {
                if (x.isObject())
                {
                    auto pushwidget = createPushWidget(((QJsonValue)x).toObject(), container_);
                    layout_->addWidget(pushwidget);
                }
            }
        }

        auto visible_it = conf.find("visible");
        if (visible_it != conf.end() && visible_it->isBool()) {
            dockVisible_ = visible_it->toBool();
        }
        else {
            dockVisible_ = true;
        }
    }

    bool ConfigIsExist(const QJsonObject& json) {
        bool exist = false;
        for (auto x : GetAllPushWidgets()) {
            auto xCon = x->Config();
            if (xCon.value("name")==json.value("name")){
                x->UpdateConfig(json);
                exist = true;
                break;
            }
        }
       
        return exist;
    }

    bool is_first_load() const
    {
        return is_first_;
    }

    void InitShow()
    {
        if (dockVisible_) {
            auto obsWin = (QMainWindow*)obs_frontend_get_main_window();
            obsWin->addDockWidget(Qt::DockWidgetArea::LeftDockWidgetArea, this);

            this->blockSignals(true);
            this->setVisible(true);
            this->blockSignals(false);
            QRect rect = obsWin->geometry();
            obsWin->resizeDocks({ this }, { rect.topLeft().y() }, Qt::Vertical);
            obsWin->resizeDocks({ this }, { rect.topLeft().y() }, Qt::Horizontal);

            if (this->is_first_load()) {
                this->setFloating(false);
            }
        }
        else {
            setVisible(false);
        }
               
    }

    bool GetDockVisible() const
    {
        return dockVisible_;
    }
    
private:
    QWidget* container_ = 0;
    QScrollArea* scroll_ = 0;
    QGridLayout* layout_ = 0;
    QPushButton* addButton_ = 0;
    QPushButton* batchSwitchButton_ = 0;
    QTimer* timer_{ nullptr };

    bool    is_exist_run_{ false };

    bool    is_first_{ false };
};

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-multi-rtmp", "en-US")


void AddRtmpServiceProc(void* data, calldata_t* calldata) {
    if (calldata) {
        const char* value = nullptr;
        if (calldata_get_string(calldata, "rtmpconfig", &value)) {
            auto dock = static_cast<MultiOutputWidget*>(data);
            if (dock) {
                //QString config = QString::fromStdString(std::string(value));
                //std::shared_ptr<QString> config = std::make_shared<QString>(QString::fromStdString(std::string(value)));
                QString config = QString::fromStdString(std::string(value));
                //dock->AddPanelWithServiceConfig(config);
                GetGlobalService().RunInUIThread([dock,config]()
                {
                    dock->AddPanelWithServiceConfig(config);
                });
            }
        }
    }
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

    blog(LOG_INFO, "load obs-multi-rtmp");

    auto obs_window = (QMainWindow*)obs_frontend_get_main_window();
    if (obs_window == nullptr)
        return false;
    QMetaObject::invokeMethod(obs_window, []() {
        s_service.uiThread_ = QThread::currentThread();
    });

    auto dock = new MultiOutputWidget(obs_window);
#if (LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(28, 0, 0))
    dock->setObjectName("obs-multi-rtmp-dock-lovense-qt6");
#else
	dock->setObjectName("obs-multi-rtmp-dock-lovense");
#endif
   
    auto action = (QAction*)obs_frontend_add_dock(dock);
    QObject::connect(action, &QAction::toggled, dock, &MultiOutputWidget::visibleToggled);

    proc_handler_t* handle = obs_get_proc_handler();
    if (handle) {
        proc_handler_add(handle, "void AddRtmpServiceProc()", AddRtmpServiceProc, dock);
    }

    obs_frontend_add_event_callback(
        [](enum obs_frontend_event event, void *private_data) {

            //blog(LOG_INFO, "obs-multi-rtmp event: %d", event);

            auto dock = static_cast<MultiOutputWidget*>(private_data);

            for(auto x: dock->GetAllPushWidgets())
                x->OnOBSEvent(event);

            if (event == obs_frontend_event::OBS_FRONTEND_EVENT_EXIT)
            {
                dock->SaveConfig();
            }
            else if (event == obs_frontend_event::OBS_FRONTEND_EVENT_PROFILE_CHANGED)
            {
                static_cast<MultiOutputWidget*>(private_data)->LoadConfig();
            }
            else if (event == obs_frontend_event::OBS_FRONTEND_EVENT_FINISHED_LOADING) {

                dock->InitShow();
#if 0
                auto obsWin = (QMainWindow*)obs_frontend_get_main_window();
                obsWin->addDockWidget(Qt::DockWidgetArea::LeftDockWidgetArea, dock);

                dock->blockSignals(true);
                dock->setVisible(true);
                dock->blockSignals(false);
                QRect rect = obsWin->geometry();
                obsWin->resizeDocks({ dock }, { rect.topLeft().y() }, Qt::Vertical);
                obsWin->resizeDocks({ dock }, { rect.topLeft().y() }, Qt::Horizontal);

                if (dock->is_first_load()) {
                    dock->setFloating(false);
                }
#endif

            
            }
#ifndef DEF_TOOLSETS
            else if (event == obs_frontend_event::OBS_FRONTEND_EVENT_WINDOWS_SHOW_CHANGED) {

                QMainWindow* obsWin = (QMainWindow*)obs_frontend_get_main_window();
				//bool bShow = obsWin->isVisible() && !obsWin->isMinimized();
				bool bShow = obsWin->isVisible();

#if 0
                blog(LOG_INFO, "obs-multi-rtmp isVisible: %d, bShow: %d isMinimized:%d hidden: %d",
					dock->GetDockVisible(), bShow, obsWin->isMinimized(), obsWin->isHidden());
#endif

                if (dock->GetDockVisible()) {                
                    dock->blockSignals(true);
                    dock->setVisible(bShow);
                    dock->blockSignals(false);
                    QRect rect = obsWin->geometry();
                    obsWin->resizeDocks({ dock }, { rect.topLeft().y() }, Qt::Vertical);
                    obsWin->resizeDocks({ dock }, { rect.topLeft().y() }, Qt::Horizontal);
                }
                else {
                    dock->setVisible(false);
                }
               
            }
#endif
        }, dock
    );

    return true;
}

const char *obs_module_description(void)
{
    return "Multiple RTMP Output Plugin";
}
