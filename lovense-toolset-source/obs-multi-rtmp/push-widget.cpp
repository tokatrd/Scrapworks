#include "pch.h"
#include <regex>
#include <optional>
#include "push-widget.h"
#include "edit-widget.h"

namespace mr {
	enum class PushStreamStatus
	{
		e_Unkonwn = 0,
		e_Connecting = 1,
		e_Streaming = 2,
		e_Reconnecting = 3,
		e_Stopping = 4,
		e_Stopped = 5,
		e_StreamingFailed = 6,
	};
}

class IOBSOutputEventHanlder
{
public:
    virtual void OnStarting() {}
    static void OnOutputStarting(void* x, calldata_t* param)
    {
        auto thiz = static_cast<IOBSOutputEventHanlder*>(x);
        thiz->OnStarting();
    }

    virtual void OnStarted() {}
    static void OnOutputStarted(void* x, calldata_t* param)
    {
        auto thiz = static_cast<IOBSOutputEventHanlder*>(x);
        thiz->OnStarted();
    }

    virtual void OnStopping() {}
    static void OnOutputStopping(void* x, calldata_t* param)
    {
        auto thiz = static_cast<IOBSOutputEventHanlder*>(x);
        thiz->OnStopping();
    }

    virtual void OnStopped(int code) {}
    static void OnOutputStopped(void* x, calldata_t* param)
    {
        auto thiz = static_cast<IOBSOutputEventHanlder*>(x);
        thiz->OnStopped(calldata_int(param, "code"));
    }

    virtual void OnReconnect() {}
    static void OnOutputReconnect(void* x, calldata_t* param)
    {
        auto thiz = static_cast<IOBSOutputEventHanlder*>(x);
        thiz->OnReconnect();
    }

    virtual void OnReconnected() {}
    static void OnOutputReconnected(void* x, calldata_t* param)
    {
        auto thiz = static_cast<IOBSOutputEventHanlder*>(x);
        thiz->OnReconnected();
    }

    virtual void onDeactive() {}
    static void OnOutputDeactive(void* x, calldata_t* param)
    {
        auto thiz = static_cast<IOBSOutputEventHanlder*>(x);
        thiz->onDeactive();
    }

    void SetAsHandler(obs_output_t* output)
    {
        auto outputSignal = obs_output_get_signal_handler(output);
        if (outputSignal)
        {
            signal_handler_connect(outputSignal, "starting", &IOBSOutputEventHanlder::OnOutputStarting, this);
            signal_handler_connect(outputSignal, "start", &IOBSOutputEventHanlder::OnOutputStarted, this);
            signal_handler_connect(outputSignal, "reconnect", &IOBSOutputEventHanlder::OnOutputReconnect, this);
            signal_handler_connect(outputSignal, "reconnect_success", &IOBSOutputEventHanlder::OnOutputReconnected, this);
            signal_handler_connect(outputSignal, "stopping", &IOBSOutputEventHanlder::OnOutputStopping, this);
            signal_handler_connect(outputSignal, "deactivate", &IOBSOutputEventHanlder::OnOutputDeactive, this);
            signal_handler_connect(outputSignal, "stop", &IOBSOutputEventHanlder::OnOutputStopped, this);
        }
    }

    void DisconnectSignals(obs_output_t* output)
    {
        auto outputSignal = obs_output_get_signal_handler(output);
        if (outputSignal)
        {
            signal_handler_disconnect(outputSignal, "starting", &IOBSOutputEventHanlder::OnOutputStarting, this);
            signal_handler_disconnect(outputSignal, "start", &IOBSOutputEventHanlder::OnOutputStarted, this);
            signal_handler_disconnect(outputSignal, "reconnect", &IOBSOutputEventHanlder::OnOutputReconnect, this);
            signal_handler_disconnect(outputSignal, "reconnect_success", &IOBSOutputEventHanlder::OnOutputReconnected, this);
            signal_handler_disconnect(outputSignal, "stopping", &IOBSOutputEventHanlder::OnOutputStopping, this);
            signal_handler_disconnect(outputSignal, "deactivate", &IOBSOutputEventHanlder::OnOutputDeactive, this);
            signal_handler_disconnect(outputSignal, "stop", &IOBSOutputEventHanlder::OnOutputStopped, this);
        }
    }
};


class PushWidgetImpl : public PushWidget, public IOBSOutputEventHanlder
{
    QJsonObject conf_;

    QPushButton* btn_ = 0;
    QLabel* name_ = 0;
    QLabel* fps_ = 0;
    QLabel* msg_ = 0;

    using clock = std::chrono::steady_clock;
    clock::time_point last_info_time_;
    uint64_t total_frames_ = 0;
    QTimer* timer_ = 0;

    QPushButton* edit_btn_ = 0;
    QPushButton* remove_btn_ = 0;

    QGridLayout* layout_ = 0;

    obs_output_t* output_ = 0;
    bool isUseDelay_ = false;

	mr::PushStreamStatus status_{ 0 };
	clock::time_point    status_uptime_;

	//private
	void _update_status(mr::PushStreamStatus status)
	{
		status_ = status;
		status_uptime_ = clock::now();

	}

    bool ReleaseOutputService()
    {
        if (!output_)
            return true;
        else if (output_ && obs_output_active(output_) == false)
        {
            auto service = obs_output_get_service(output_);
            if (service)
            {
                obs_output_set_service(output_, nullptr);
                obs_service_release(service);
            }
            return true;
        }
        else
            return false;
    }
    
    bool ReleaseOutputEncoder()
    {
        if (!output_)
            return true;
        else if (output_ && obs_output_active(output_) == false)
        {
            auto venc = obs_output_get_video_encoder(output_);
            if (venc)
            {
                obs_output_set_video_encoder(output_, nullptr);
                obs_encoder_release(venc);
            }
            
            auto aenc = obs_output_get_audio_encoder(output_, 0);
            if (aenc)
            {
                obs_output_set_audio_encoder(output_, nullptr, 0);
                obs_encoder_release(aenc);
            }

            return true;
        }
        else
            return false;
    }

    bool PrepareOutputService()
    {
        if (!output_)
            return false;
        
        ReleaseOutputService();

        auto conf = obs_data_create();
        if (!conf)
            return false;
        
        obs_data_set_string(conf, "server", tostdu8(conf_["rtmp-path"].toString()).c_str());
        obs_data_set_string(conf, "key", tostdu8(conf_["rtmp-key"].toString()).c_str());

        auto user = tostdu8(conf_["rtmp-user"].toString());
        auto pass = tostdu8(conf_["rtmp-pass"].toString());
        if (!user.empty())
        {
            obs_data_set_bool(conf, "use_auth", true);
            obs_data_set_string(conf, "username", user.c_str());
            obs_data_set_string(conf, "password", pass.c_str());
        }
        else
            obs_data_set_bool(conf, "use_auth", false);
        
        auto service = obs_service_create("rtmp_custom", "multi-output-service", conf, nullptr);
        obs_data_release(conf);
        if (!service)
            return false;
        obs_output_set_service(output_, service);

        return true;
    }
   

    bool EncoderAvailable(const char* encoder)
    {
        const char* val;
        int i = 0;

        while (obs_enum_encoder_types(i++, &val))
            if (strcmp(val, encoder) == 0)
                return true;

        return false;
    }

#define SIMPLE_ENCODER_X264 "x264"
#define SIMPLE_ENCODER_X264_LOWCPU "x264_lowcpu"
#define SIMPLE_ENCODER_QSV "qsv"
#define SIMPLE_ENCODER_NVENC "nvenc"
#define SIMPLE_ENCODER_AMD "amd"

    const char* GetDefaultVideoEncoderId()
    {
        auto main_cfg = obs_frontend_get_profile_config();
        if (!main_cfg) return nullptr;

        const char* encoder = config_get_string(main_cfg, "SimpleOutput",
            "StreamEncoder");
        if (!encoder) {
            return nullptr;
        }

        if (strcmp(encoder, SIMPLE_ENCODER_QSV) == 0) {
            return "obs_qsv11";

        }
        else if (strcmp(encoder, SIMPLE_ENCODER_AMD) == 0) {
            return "amd_amf_h264";

        }
        else if (strcmp(encoder, SIMPLE_ENCODER_NVENC) == 0) {
            return EncoderAvailable("jim_nvenc") ? "jim_nvenc" : "ffmpeg_nvenc";
        }
        else {
            return "obs_x264";
        }
    }

    int GetAudioBitrate(config_t* cfg) const
    {
        int bitrate = (int)config_get_uint(cfg, "SimpleOutput", "ABitrate");

        //return FindClosestAvailableAACBitrate(bitrate);
        return bitrate;
    }

    const char* GetDefaultAudioEncoderid(int bitrate)
    {
        //return GetAACEncoderForBitrate(bitrate);
        return FindAudioEncoderFromCodec("AAC");
    }

    const char* FindAudioEncoderFromCodec(const char* type)
    {
        const char* alt_enc_id = nullptr;
        size_t i = 0;

        while (obs_enum_encoder_types(i++, &alt_enc_id)) {
            const char* codec = obs_get_encoder_codec(alt_enc_id);
            if (strcmp(type, codec) == 0) {
                return alt_enc_id;
            }
        }

        return nullptr;
    }

    bool PrepareOutputEncoders()
    {
        if (!output_)
            return false;
        
        ReleaseOutputEncoder();          


        // read config
        auto venc_id = QJsonUtil::Get(conf_, "v-enc", std::string{});
        auto aenc_id = QJsonUtil::Get(conf_, "a-enc", std::string{});
        auto v_bitrate = QJsonUtil::Get(conf_, "v-bitrate", 2000);
        auto a_bitrate = QJsonUtil::Get(conf_, "a-bitrate", 128);
        auto v_keyframe_sec = QJsonUtil::Get(conf_, "v-keyframe-sec", 3);
        auto a_mixer = QJsonUtil::Get(conf_, "a-mixer", 0);
        auto v_bframes = QJsonUtil::Get<int>(conf_, "v-bframes");
        auto resolution = QJsonUtil::Get<std::string>(conf_, "v-resolution");
        int v_width = -1, v_height = -1;
        
        {
            if (resolution.has_value()) {
                std::regex res_pattern(R"__(\s*(\d{1,5})\s*x\s*(\d{1,5})\s*)__");
                std::smatch match;
                if (std::regex_match(resolution.value(), match, res_pattern))
                {
                    v_width = std::stoi(match[1].str());
                    v_height = std::stoi(match[2].str());
                }

                if (a_mixer < 0 || a_mixer > 5)
                    a_mixer = 0;
            }
        }



        // ====== prepare encoder
        obs_encoder *venc = 0, *aenc = 0;


        //1. Priority to self settings
        if (!venc_id.empty()) {
            obs_data_t* video_settings = obs_data_create();
            obs_data_set_int(video_settings, "bitrate", v_bitrate);
            obs_data_set_int(video_settings, "keyint_sec", v_keyframe_sec);
            if (v_bframes.has_value())
                obs_data_set_int(video_settings, "bf", v_bframes.value());
            venc = obs_video_encoder_create(venc_id.c_str(), "multi-rtmp-video-encoder", video_settings, nullptr);
            obs_data_release(video_settings);
            obs_encoder_set_video(venc, obs_get_video());
            if (v_width > 0 && v_height > 0)
            {
                obs_encoder_set_scaled_size(venc, v_width, v_height);
            }
        }

        if (!aenc_id.empty()) {
            //audio
            obs_data_t* audio_settings = obs_data_create();
            obs_data_set_int(audio_settings, "bitrate", a_bitrate);
            aenc = obs_audio_encoder_create(aenc_id.c_str(), "multi-rtmp-audio-encoder", audio_settings, a_mixer, nullptr);
            obs_data_release(audio_settings);
            obs_encoder_set_audio(aenc, obs_get_audio());
        }

        //2. from obs_frontend_get_streaming_output
        // load stream encoders
        if (!venc || !aenc)
        {
            obs_output_t* stream_out = obs_frontend_get_streaming_output();

            if (stream_out && !venc)
            {
                venc = obs_output_get_video_encoder(stream_out);
#if (LIBOBS_API_VER < MAKE_SEMANTIC_VERSION(28, 0, 0))
				obs_encoder_addref(venc);
#endif
            }
            
            if (stream_out && !aenc)
            {
                aenc = obs_output_get_audio_encoder(stream_out, 0);
#if (LIBOBS_API_VER < MAKE_SEMANTIC_VERSION(28, 0, 0))
				obs_encoder_addref(venc);
#endif
            }

            obs_output_release(stream_out);
        }

        //3. from default
        if (!venc)
        {
            auto main_cfg = obs_frontend_get_profile_config();
            //
            const char* encoder_id = GetDefaultVideoEncoderId();

            if (encoder_id) {
                venc = obs_video_encoder_create(
                    encoder_id, "multi-rtmp-video-encoder", nullptr, nullptr);
                obs_encoder_set_video(venc, obs_get_video());
            }
        }

        if (!aenc) 
        {
            auto main_cfg = obs_frontend_get_profile_config();

            int bitrate = GetAudioBitrate(main_cfg);
            const char* audio_id = GetDefaultAudioEncoderid(bitrate);
            if (audio_id == nullptr) {
                const char* codec = obs_output_get_supported_audio_codecs(output_);
                if (codec && strcmp(codec, "aac") != 0) {
                    audio_id = FindAudioEncoderFromCodec(codec);
                }
            }

            if (audio_id) {
                obs_data_t* settings = obs_data_create();
                obs_data_set_int(settings, "bitrate", bitrate);

                aenc = obs_audio_encoder_create(
                    audio_id, "multi-rtmp-video-encoder", nullptr, 0, nullptr);
                if (aenc) {
                    obs_encoder_update(aenc, settings);
                    obs_encoder_set_audio(aenc, obs_get_audio());
                }
                obs_data_release(settings);
            }
        }

        obs_output_set_video_encoder(output_, venc);
        obs_output_set_audio_encoder(output_, aenc, 0);

        if (!aenc || !venc)
        {
            ReleaseOutputEncoder();

           auto msgbox = new QMessageBox(QMessageBox::Icon::Critical,
                obs_module_text("Notice.Title"),
                obs_module_text("Notice.GetEncoder"),
                QMessageBox::StandardButton::Ok,
                this
            );

            msgbox->exec();
            return false;
        }

        return true;
    }

    bool ReleaseOutput()
    {
        if (output_) {
            DisconnectSignals(output_);
        }

        if (output_ && obs_output_active(output_)) {
            obs_output_force_stop(output_);
        }

        if (output_ && obs_output_active(output_) == false)
        {
            bool ret = ReleaseOutputService();
            ret = ReleaseOutputEncoder() && ret;

            obs_output_release(output_);
            output_ = nullptr;

            return ret;
        }
        else if (output_) {
            obs_output_release(output_);
            output_ = nullptr;

            return true;
        }
        else if (output_ == nullptr)
            return true;
        else
            return false;
    }

public:
    PushWidgetImpl(QJsonObject conf, QWidget* parent = 0)
        : QWidget(parent)
        , conf_(conf)
    {
        QObject::setObjectName("push-widget");

        timer_ = new QTimer(this);
        timer_->setInterval(std::chrono::milliseconds(1000));
        QObject::connect(timer_, &QTimer::timeout, [this]() {
            if (!output_)
                return;
            
            auto new_frames = obs_output_get_total_frames(output_);
            auto now = clock::now();

            auto intervalms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_info_time_).count();
            if (intervalms > 0)
            {
                auto text = std::to_string((new_frames - total_frames_) * 1000 / intervalms) + " FPS";
                fps_->setText(text.c_str());
            }

            total_frames_ = new_frames;
            last_info_time_ = now;

			//Fixed 20230211: When multi-push and OBS push to the same streaming site, multi-push cannot stop after OBS push ends (actually no streaming)
			//If stopping state persists beyond set time, force stop
			if (
				status_ == mr::PushStreamStatus::e_Stopping
				&& IsRunning()
				&& std::chrono::duration_cast<std::chrono::seconds>(now - status_uptime_).count() > 5)
			{
				obs_output_force_stop(output_);
			}
        });

        auto layout = new QGridLayout(this);
        layout_ = layout;
        layout->addWidget(name_ = new QLabel(obs_module_text("NewStreaming"), this), 0, 0, 1, 2);
        layout->addWidget(fps_ = new QLabel(u8"", this), 0, 2);
        layout->addWidget(btn_ = new QPushButton(obs_module_text("Btn.Start"), this), 1, 0);
        QObject::connect(btn_, &QPushButton::clicked, [this]() {
            StartStop();
        });

        layout->addWidget(edit_btn_ = new QPushButton(obs_module_text("Btn.Edit"), this), 1, 1);
        QObject::connect(edit_btn_, &QPushButton::clicked, [this]() {
            ShowEditDlg();
        });

        layout->addWidget(remove_btn_ = new QPushButton(obs_module_text("Btn.Delete"), this), 1, 2);
        QObject::connect(remove_btn_, &QPushButton::clicked, [this]() {
            auto msgbox = new QMessageBox(QMessageBox::Icon::Question,
                obs_module_text("Question.Title"),
                obs_module_text("Question.Delete"),
                QMessageBox::Yes | QMessageBox::No,
                this
            );
            if (msgbox->exec() == QMessageBox::Yes) {
                GetGlobalService().RunInUIThread([this]() {
                    delete this;
                });
            }
        });

        //Error message
        //layout->addWidget(msg_ = new QLabel(u8"", this), 2, 0, 1, 3);
        //msg_->setWordWrap(true);
        
        //Empty line
        //layout->addItem(new QSpacerItem(0, 10), 3, 0);

        layout->setColumnMinimumWidth(0, 75);
        layout->setColumnMinimumWidth(1, 75);
        layout->setColumnMinimumWidth(2, 75);

        LoadConfig();
    }

    ~PushWidgetImpl()
    {
        ReleaseOutput();
    }

    QJsonObject Config() override
    {
        return conf_;
    }

    void UpdateConfig(const QJsonObject& object) override {
        conf_ = object;
        LoadConfig();
    }

    void OnOBSEvent(obs_frontend_event ev) override
    {
        if (ev == obs_frontend_event::OBS_FRONTEND_EVENT_EXIT
            || ev == obs_frontend_event::OBS_FRONTEND_EVENT_PROFILE_CHANGED
            || ev == obs_frontend_event::OBS_FRONTEND_EVENT_PROFILE_LIST_CHANGED
        ) {
            ForceStop();
        } else if (ev == obs_frontend_event::OBS_FRONTEND_EVENT_STREAMING_STARTING) {
            if (!IsRunning() && QJsonUtil::Get(conf_, "syncstart", false)) {
                StartStop();
            }
        } else if (ev == obs_frontend_event::OBS_FRONTEND_EVENT_STREAMING_STOPPING) {
            if (IsRunning() && QJsonUtil::Get(conf_, "syncstart", false)) {
                StartStop();
            }
        }
    }

    void LoadConfig()
    {
        name_->setText(QJsonUtil::Get(conf_, "name", QString("")));
    }

    void ResetInfo()
    {
        total_frames_ = 0;
        last_info_time_ = clock::now();
        fps_->setText("");
    }

    bool IsRunning()
    {
        if (output_ == nullptr)
            return false;
        if (output_ != nullptr && obs_output_active(output_) == false)
            return false;
        if (output_ != nullptr && obs_output_active(output_) == true)
            return true;
        assert(false);
        return false;
    }

    void StartStop()
    {
        ResetMsg();

        if (IsRunning() == false){
            // recreate output
            ReleaseOutput();

            if (output_ == nullptr)
            {
                output_ = obs_output_create("rtmp_output", "multi-output", nullptr, nullptr);
                SetAsHandler(output_);
            }

            if (output_) {
                isUseDelay_ = false;
                
                auto profileConfig = obs_frontend_get_profile_config();
                if (profileConfig) {
                    bool useDelay = config_get_bool(profileConfig, "Output", "DelayEnable");
                    bool preserveDelay = config_get_bool(profileConfig, "Output", "DelayPreserve");
                    int delaySec = config_get_int(profileConfig, "Output", "DelaySec");
                    obs_output_set_delay(output_, 
                        useDelay ? delaySec : 0,
			            preserveDelay ? OBS_OUTPUT_DELAY_PRESERVE : 0
                    );

                    if (useDelay && delaySec > 0)
                        isUseDelay_ = true;
                }
            }

            if (!PrepareOutputService())
            {
                SetMsg(obs_module_text("Error.CreateRtmpService"));
                return;
            }

            if (!PrepareOutputEncoders())
            {
                SetMsg(obs_module_text("Error.CreateEncoder"));
                return;
            }

            if (!obs_output_start(output_))
            {
                SetMsg(obs_module_text("Error.StartOutput"));
            }
        }
        else if (output_ != nullptr)
        {
            bool useForce = false;
            if (isUseDelay_) {
                auto res = QMessageBox(QMessageBox::Icon::Information, 
                    "?",
                    obs_module_text("Ques.DropDelay"), 
                    QMessageBox::StandardButton::Yes | QMessageBox::StandardButton::No,
                    this
                ).exec();
                if (res == QMessageBox::Yes)
                    useForce = true;
            }

            if (!useForce)
                obs_output_stop(output_);
            else
                obs_output_force_stop(output_);
        }
    }

    void Start()
    {
        if (!IsRunning()) {
            StartStop();
        }
    }

    void Stop()
    {
        if (IsRunning())
        {
            obs_output_stop(output_);
        }
    }

    void ForceStop()
    {
        if (IsRunning())
        {
            obs_output_force_stop(output_);
        }
    }

    bool ShowEditDlg() override
    {
        ResetMsg();

        std::unique_ptr<EditOutputWidget> dlg{ createEditOutputWidget(conf_, this) };

        if (dlg->exec() == QDialog::DialogCode::Accepted)
        {
            conf_ = dlg->Config();
            LoadConfig();
            return true;
        }
        else
            return false;
    }

    void SetMsg(QString msg)
    {
		
        if (msg.size() > 0 && ! msg_ && layout_) {
            layout_->addWidget(msg_ = new QLabel(u8"", this), 2, 0, 1, 3);
            msg_->setWordWrap(true);
            
        }

        if (msg_) {
			if (msg.size() > 0) {
				msg_->setText(msg);
				msg_->setToolTip(msg);
			}
			else {
				ResetMsg();
			}
        }
    }

    void ResetMsg() {
        if (msg_ && layout_) {
            layout_->removeWidget(msg_);
            delete msg_;
            msg_ = nullptr;
        }
    }

    // obs logical
    void OnStarting() override
    {
        GetGlobalService().RunInUIThread([this]()
        {
			this->_update_status(mr::PushStreamStatus::e_Connecting);
            remove_btn_->setEnabled(false);
            btn_->setText(obs_module_text("Status.Stop"));
            btn_->setEnabled(true);
            SetMsg(obs_module_text("Status.Connecting"));
            remove_btn_->setEnabled(false);
        });
    }

    void OnStarted() override
    {
        GetGlobalService().RunInUIThread([this]() {
			this->_update_status(mr::PushStreamStatus::e_Streaming);
            remove_btn_->setEnabled(false);
            btn_->setText(obs_module_text("Status.Stop"));
            btn_->setEnabled(true);
            SetMsg(obs_module_text("Status.Streaming"));

            ResetInfo();
            timer_->start();
        });
    }

    void OnReconnect() override
    {
        GetGlobalService().RunInUIThread([this]() {
			this->_update_status(mr::PushStreamStatus::e_Reconnecting);
            remove_btn_->setEnabled(false);
            btn_->setText(obs_module_text("Status.Stop"));
            btn_->setEnabled(true);
            SetMsg(obs_module_text("Status.Reconnecting"));
        });
    }

    void OnReconnected() override
    {
        GetGlobalService().RunInUIThread([this]() {
			this->_update_status(mr::PushStreamStatus::e_Streaming);
            remove_btn_->setEnabled(false);
            btn_->setText(obs_module_text("Status.Stop"));
            btn_->setEnabled(true);
            SetMsg(obs_module_text("Status.Streaming"));

            ResetInfo();
        });
    }

    void OnStopping() override
    {
        GetGlobalService().RunInUIThread([this]() {
			this->_update_status(mr::PushStreamStatus::e_Stopping);
            remove_btn_->setEnabled(false);
            btn_->setText(obs_module_text("Status.Stop"));
            btn_->setEnabled(true);
            SetMsg(obs_module_text("Status.Stopping"));
        });
    }

    void OnStopped(int code) override
    {
        GetGlobalService().RunInUIThread([this, code]() {
			this->_update_status(mr::PushStreamStatus::e_Stopped);
            ResetInfo();
            timer_->stop();

            remove_btn_->setEnabled(true);
            btn_->setText(obs_module_text("Btn.Start"));
            btn_->setEnabled(true);
            SetMsg(u8"");

            switch(code)
            {
                case 0:
                    SetMsg(u8"");
                    break;
                case -1:
                    SetMsg(obs_module_text("Error.WrongRTMPUrl"));
                    break;
                case -2:
                    SetMsg(obs_module_text("Error.ServerConnect"));
                    break;
                case -3:
                    SetMsg(obs_module_text("Error.ServerHandshake"));
                    break;
                case -4:
                    SetMsg(obs_module_text("Error.ServerRefuse"));
                    break;
                default:
                    SetMsg(obs_module_text("Error.Unknown"));
                    break;
            }
        });

        ReleaseOutputEncoder();
    }
};

PushWidget* createPushWidget(QJsonObject conf, QWidget* parent) {
    return new PushWidgetImpl(conf, parent);
}
