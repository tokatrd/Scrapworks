#ifndef MAINCONTROLDOCKWIDGET_H
#define MAINCONTROLDOCKWIDGET_H

#include <QDockWidget>
#include <QpushButton>
#include "uvc_camera.h"
namespace Ui {
class MainControlDockWidget;
}

class MainControlDockWidget : public QDockWidget
{
    Q_OBJECT

public:
    explicit MainControlDockWidget(QWidget *parent = nullptr);
    ~MainControlDockWidget();

public:
    void InitInsta360();
    void InitControlLable();
    void UpdatePanTitl(int x, int y, bool move);
    void InitResulotion();
    //OBS Event
    void OnOBSFinishedLoadingEvent();
    void OnOBSExitEvent();

private slots:
   
    void on_buttonPanReset_clicked();

    //zoom filter
    void on_buttonZoomSubtract_clicked();

    void on_buttonZoomAdd_clicked();

    void on_sliderZoom_sliderMoved(int position);
    void on_sliderZoom_valueChanged(int val);

    void on_buttonZoomAdd_pressed();

    void on_buttonZoomSubtract_pressed();

    //Image Settings
    void on_sliderColortemperature_sliderMoved(int position);

    void on_sliderBrightness_sliderMoved(int position);

    void on_sliderContrast_sliderMoved(int position);

    void on_sliderChroma_sliderMoved(int position);

    void on_sliderAcutance_sliderMoved(int position);

    void on_buttonImagesSettingsReset_clicked(bool checked);

    void on_sliderExposure_valueChanged(int value);


    //Advance Mode
    void on_buttonCompositHead_clicked(bool checked);

    void on_buttonCompositHalt_clicked(bool checked);

    void on_buttonCompositFull_clicked(bool checked);

    void on_combCameraResolution_currentIndexChanged(const QString &arg1);


    void on_buttonAddPanPosition_clicked(bool checked);

    void on_comboBoxTrackSpeed_currentIndexChanged(int index);

    void on_sliderFouceRange_valueChanged(int value);

    void on_buttonUp_clicked();

    void on_buttonDown_clicked();

    void on_buttonLeft_clicked();

    void on_buttonRight_clicked();

    //Remote Adjust Control
    void on_buttonRemoteAdjust_clicked();

    void on_combCameraResolution_activated(const QString &arg1);

    void on_combCameraResolution_currentIndexChanged(int index);

public slots:
    void on_button_Enable_Composition(bool enable);
    void on_button_Enable_AutoTrack(bool enable);
    void on_button_Enable_AutoFouce(bool enable);
    void on_button_Enable_LiveMode(bool enable);
    void on_button_Enable_HDRMode(bool enable);
    void on_button_Enable_MirorMode(bool enable);
    void on_button_Enable_AllGesture(bool enable);

    //AI Gesture
    void on_button_Enable_GestureTrack(bool enable);
    void on_button_Enable_GestureZoom(bool enable);


    void UpdateComposition(CompositionStyle style);
    void UpdateVideoMode(VideoMode mode, VideoModeAuxiliaryData extenddata);

    //AutoExposure
    void on_button_Enable_AutoExposure(bool enable);//�Զ��ع�
    void on_button_Enable_AutoWhitebalance(bool enable);//�Զ���ƽ��
    void InitImageSettings(bool reset);
    void InitAdvanceSettings(bool reset);
    void CompositionStatusChange(CompositionStyle style);
public:
    virtual void timerEvent(QTimerEvent* ev);
    void SetButtonImage(QPushButton* button, const char *image);
    virtual bool eventFilter(QObject *obj, QEvent *event);
    void LoadPositionConfig();
    Ui::MainControlDockWidget *ui;
    int m_checkInsta360Timer;
    int m_updateUITimer_{0};
    bool is_checked_{ false };
};

#endif // MAINCONTROLDOCKWIDGET_H
