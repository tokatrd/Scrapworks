#include "pch.h"

class PushWidget : virtual public QWidget {
public:
    virtual ~PushWidget() {}
    virtual QJsonObject Config() = 0;
    virtual bool ShowEditDlg() = 0;
    virtual void UpdateConfig(const QJsonObject& object) = 0;
    virtual void OnOBSEvent(obs_frontend_event ev) = 0;
    
    virtual void Start() = 0;
    virtual void Stop() = 0;
    virtual void ForceStop() = 0;
    virtual bool IsRunning() = 0;
};

PushWidget* createPushWidget(QJsonObject conf, QWidget* parent = 0);
