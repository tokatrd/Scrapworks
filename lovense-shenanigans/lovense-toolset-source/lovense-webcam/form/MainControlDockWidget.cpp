#include <array>
#include <thread>
#include <fstream>
#include <random>

//-qt
#include <QToolTip>
#include <QLabel>
#include <Qicon>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QSpinBox>
#include <QMenu>
#include <QTableView>
#include <QHeaderView>
#include <QStringList>
#include <QStandardItemModel>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QWidget>
#include <QMainWindow>
#include <QMenuBar>
#include <QSpacerItem>
#include <QProcess>
#include <QFileInfo>
#include <QDesktopServices>
#include <QUrl>
#include <QLibraryInfo>
#include <QPainterPath>
#include <QImage>
#include <QImageReader>
#include <QFileDialog>
#include <QGraphicsColorizeEffect>
//-obs
#include "obs-module.h"
#include "util/config-file.h"
#include <obs-frontend-api.h>
//-core
#include "json/CJsonObject.hpp"
#include "lovense_obs_helper.hpp"
#include "lovense_util_helper.hpp"
#include "lovense_string_helper.hpp"
#include "lovense_camera_helper.hpp"
#include "lovense_obs_proc.hpp"
#include "lovense_error.hpp"
#include "lovense_obs_config.hpp"
//-module
#include "wc_common.hpp"
#include "CLink360Adapter.h"
#include "UntilHelp.h"
#include "wc-resource.hpp"

#include "RemoteControlDialog.h"
#include "PositionWidget.h"
#include "NewPositionDialog.h"
#include "NewControlDialog.h"
#include "MainControlDockWidget.h"
#include "ui_MainControlDockWidget.h"
#include "UI_ControlConfigWidget.h"
#include "UI_HelpTipLabel.h"
#include "control-panel-widget.hpp"
#include "color-preset-dialog.hpp"
#include "color-preset-item.hpp"

using namespace lovense;

MainControlDockWidget::MainControlDockWidget(QWidget *parent, const wc::Camera& camera)
	: MainWidget(parent)
    , ui(new Ui::MainControlDockWidget)
	, m_link_controller(new wc::LinkController())
{
    this->setFeatures(QDockWidget::NoDockWidgetFeatures);

	Resource::instance();

	m_link_config = wc::ConfigCenter::instance()->get();
	assert(m_link_config);

    ui->setupUi(this);

	ui->comboBox_Webcams->setVisible(false);

	this->initImageSettingsUI();

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	MainControlDockWidget::adjustFont(this);
#endif

	ui->label_12->setText("");
	ui->labelExposure->setText("");
	

    connect(ui->btnCompositionEnable, SIGNAL(statusChanged(bool)), this, SLOT(on_button_Enable_Composition(bool)));
    connect(ui->buttonAutoTrack, SIGNAL(statusChanged(bool)), this, SLOT(on_button_Enable_AutoTrack(bool)));
    connect(ui->buttonAutoFouce, SIGNAL(statusChanged(bool)), this, SLOT(on_button_Enable_AutoFouce(bool)));
#if 0
    connect(ui->buttonLiveMode, SIGNAL(statusChanged(bool)), this, SLOT(on_button_Enable_LiveMode(bool)));
#endif
    connect(ui->buttonHDRMode,      SIGNAL(statusChanged(bool)), this, SLOT(on_button_Enable_HDRMode(bool)));
    connect(ui->buttonMiroMode,     SIGNAL(statusChanged(bool)), this, SLOT(on_button_Enable_MirorMode(bool)));
	connect(ui->buttonVerticalFlip, SIGNAL(statusChanged(bool)), this, SLOT(on_button_Enable_MirrorVertically(bool)));

    //Gesture Enable
	connect(ui->buttonGestureSettings, SIGNAL(statusChanged(bool)), this, SLOT(on_button_Enable_GestureSettings(bool)));
    connect(ui->buttonGestureTrack,    SIGNAL(statusChanged(bool)), this, SLOT(on_button_Enable_GestureTrack(bool)));
    connect(ui->buttonGestureZoom,     SIGNAL(statusChanged(bool)), this, SLOT(on_button_Enable_GestureZoom(bool)));
    
    connect(ui->buttonAutoExposure, SIGNAL(statusChanged(bool)), this, SLOT(on_button_Enable_AutoExposure(bool)));
    //on_button_Enable_AutoWhitebalance
    connect(ui->buttonWhitebalance, SIGNAL(statusChanged(bool)), this, SLOT(on_button_Enable_AutoWhitebalance(bool)));
	

    ui->tabWidget->setCurrentIndex(0);

    
	{
		auto image = wc::Resource::image("gestureTrack.png");
		image = image.scaled(QSize(35, 35), Qt::KeepAspectRatio, Qt::SmoothTransformation);
		ui->labelGestureTrackImage->setPixmap(image);
		ui->labelGestureTrackImage->setFixedSize(QSize(42, 42));
	}

	{
		auto image = wc::Resource::image("gestureZoom.png");
		image = image.scaled(QSize(35, 35), Qt::KeepAspectRatio, Qt::SmoothTransformation);
		ui->labelGestureZoomImage->setPixmap(image);
		ui->labelGestureZoomImage->setFixedSize(QSize(42, 42));
	}
	
	//zoom
	auto plus_button_icon = QIcon(wc::Resource::plus());
	ui->buttonAddPanPosition->setIcon(plus_button_icon);
	//ui->buttonAddControl->setIcon(plus_button_icon);
	ui->buttonZoomAdd->setIcon(plus_button_icon);
	ui->button_AddColorPreset->setIcon(plus_button_icon);

	auto minus_button_icon = QIcon(wc::Resource::minus());
	ui->buttonZoomSubtract->setIcon(minus_button_icon);

	{
		/*ui->buttonPanReset->setStyleSheet(
			"QToolButton{ border-style: flat; background: transparent; }"
		);*/
		ui->buttonPanReset->setToolButtonStyle(Qt::ToolButtonIconOnly);
		ui->buttonPanReset->setAutoFillBackground(true);
		ui->buttonPanReset->setIcon(wc::Resource::reset());		
	}

	{
		/*ui->buttonPanReset->setStyleSheet(
			"QToolButton{ border-style: flat; background: transparent; }"
		);*/
		ui->buttonImagesSettingsReset->setToolButtonStyle(Qt::ToolButtonIconOnly);
		ui->buttonImagesSettingsReset->setAutoFillBackground(true);
		ui->buttonImagesSettingsReset->setIcon(wc::Resource::reset());
	}

	auto panTiltCallback = std::bind(&MainControlDockWidget::OnPanControlUpdatePanTitlValue, this
		, std::placeholders::_1
		, std::placeholders::_2
		, std::placeholders::_3);

	
	WSProtocol::instance()->SetUIPanTiltCallback(panTiltCallback);

#ifdef __APPLE__
    //this->InitInsta360();
    ui->combCameraResolution->setMinimumWidth(160);
    this->setMinimumWidth(350);
    ui->ChatroomNoticeValue_horizontalLayout->addSpacerItem(new QSpacerItem(1,1,QSizePolicy::Expanding,QSizePolicy::Fixed));
    ui->ChatroomNoticeValue_horizontalLayout->setContentsMargins(10, 0, 0, 0);
    ui->ChatroomNoticeValue_horizontalLayout->setSpacing(5);
    ui->ChatroomNoticeValue_horizontalLayout->setAlignment(Qt::AlignCenter);
    ui->comboBoxChatroomNoticeFrequency->setMinimumWidth(60);
#endif
    

     foreach(QAbstractSpinBox * sb, this->findChildren<QAbstractSpinBox *>())
    {
	    sb->installEventFilter(this);
    }
    foreach(QComboBox * cb, this->findChildren<QComboBox *>())
    {
	    cb->installEventFilter(this);
    }
    foreach(QSlider * cb, this->findChildren<QSlider *>())
    {
	    cb->installEventFilter(this);
    }
    
	this->initScreenMode();

	this->initChatroomNotice();

	this->initAudioFeatureUI();

	this->init_color_preset();

	this->init_anti_flicker();

	this->init_ai_zoom();

	//
	this->ui->widget_PresetPosition->set_add_button(ui->buttonAddPanPosition);

	//
	this->switch_webcam_model(camera);
}

//-------------------------------------------------------------------------------------------------
void MainControlDockWidget::initControlStickUI()
{
	bool is_new_qt_version = QVersionNumber::compare(QLibraryInfo::version(), QVersionNumber(6, 6, 3)) >= 0 ? true : false;

	auto _setImageButton = [&is_new_qt_version](QPushButton* btn, const char* image)->void {
		QIcon icon(wc::Resource::image(image));
		if (!icon.isNull()) {
			btn->setFlat(true);
			btn->setAutoFillBackground(true);
			btn->setIcon(icon);


			if (is_new_qt_version) {
				btn->setStyleSheet("QPushButton {"\
					"border: none;"\
					"background: transparent;"\
					"}");
			}
		}
	};

	{
		_setImageButton(ui->buttonUp, "up.png");
		if (is_new_qt_version) {
			QRect rect = ui->buttonUp->geometry();
			//offset top
			rect.translate(0, 5);
			ui->buttonUp->setGeometry(rect);
		}

		connect(ui->buttonUp, &QPushButton::pressed, this, [this]() {
			this->OnPanControlEvent(PAN_UP, false);
			});
		connect(ui->buttonUp, &QPushButton::released, this, [this]() {
			this->OnPanControlEvent(PAN_UP, true);
			});
	}

	{
		ui->labelPanBackground->setPixmap(wc::Resource::image("large.png"));
	}

	{
		_setImageButton(ui->buttonDown, "down.png");
		if (is_new_qt_version) {
			QRect rect = ui->buttonDown->geometry();
			//offset top
			rect.translate(0, 5);
			ui->buttonDown->setGeometry(rect);
		}

		connect(ui->buttonDown, &QPushButton::pressed, this, [this]() {
			this->OnPanControlEvent(PAN_DOWN, false);
			});
		connect(ui->buttonDown, &QPushButton::released, this, [this]() {
			this->OnPanControlEvent(PAN_DOWN, true);
			});

	}

	{
		_setImageButton(ui->buttonLeft, "left.png");
		connect(ui->buttonLeft, &QPushButton::pressed, this, [this]() {
			//OnPanControl(PAN_LEFT, false);
			this->OnPanControlEvent(PAN_LEFT, false);
			});
		connect(ui->buttonLeft, &QPushButton::released, this, [this]() {
			//OnPanControl(PAN_LEFT, true);
			this->OnPanControlEvent(PAN_LEFT, true);
			});
	}

	{
		_setImageButton(ui->buttonRight, "right.png");
		connect(ui->buttonRight, &QPushButton::pressed, this, [this]() {
			//OnPanControl(PAN_RIGHT, false);
			this->OnPanControlEvent(PAN_RIGHT, false);
			});
		connect(ui->buttonRight, &QPushButton::released, this, [this]() {
			//OnPanControl(PAN_RIGHT, true);
			this->OnPanControlEvent(PAN_RIGHT, true);
			});
	}

	auto panTiltCallback = std::bind(&MainControlDockWidget::OnPanControlUpdatePanTitlValue, this
		, std::placeholders::_1
		, std::placeholders::_2
		, std::placeholders::_3);

	ui->widget_ControlRocker->SetPanTiltCallback(panTiltCallback);
}

void MainControlDockWidget::initControlPanelUI()
{
	ui->widget_ControlPanel->setMovedCallback(
		std::bind(&MainControlDockWidget::OnControlPanelMoved, this
			, std::placeholders::_1
			, std::placeholders::_2
			, std::placeholders::_3
		)			
	);

	ui->widget_ControlPanel->setMovedOverCallback(
		std::bind(&MainControlDockWidget::OnControlPanelMovedOver, this
			, std::placeholders::_1
			, std::placeholders::_2
			, std::placeholders::_3
		)
	);
}

void MainControlDockWidget::setControlUI(wc::WebcamModel model)
{
	if (model == wc::WebcamModel::WEBCAM_LINK) {		
		ui->widget_ControlPanel->setVisible(false);
		this->initControlStickUI();
		ui->widget_ControlStick->setVisible(true);
	}
	else if (model == wc::WebcamModel::WEBCAM_LINK_2C) {
		ui->widget_ControlStick->setVisible(false);		
		this->initControlPanelUI();
		ui->widget_ControlPanel->setVisible(true);
	}
}

//-------------------------------------------------------------------------------------------------

int MainControlDockWidget::getScreenIndex(int mode)
{
	int index = 0;
	for (int i = 0; i < ui->combSreenMode->count(); i++)
	{
		if (mode == ui->combSreenMode->itemData(i).toInt())
		{
			index = i;
			break;
		}
	}
	return index;
}

int MainControlDockWidget::getCurrentScreenIndex()
{
	int mode = m_link_config->get_screen_mode();
	return this->getScreenIndex(mode);
}

void MainControlDockWidget::initScreenMode()
{
	ui->combSreenMode->blockSignals(true);
	ui->combSreenMode->addItem(obs_module_text("Webcam.LandscapeMode"), wc::LiveMode_Horizontal);
	ui->combSreenMode->addItem(obs_module_text("Webcam.PortraitMode"), wc::LiveMode_Vertical);
	ui->combSreenMode->blockSignals(false);
	
	ui->SreenModeTip->setStyleSheet("QToolButton{ border-style: flat; background: transparent; }");
	ui->SreenModeTip->setToolButtonStyle(Qt::ToolButtonIconOnly);
	ui->SreenModeTip->setAutoFillBackground(true);
	ui->SreenModeTip->setIcon(wc::Resource::help());

	ui->SreenModeTip->installEventFilter(this);

#if 0
	on_resize_ScreenModeUI();
#endif

	//---------------------------------------------------------
	//init data
	int mode = UntilRes::GetLiveMode();
	m_link_config->set_screen_mode(mode);

	updateScreenModeUI();
}

void MainControlDockWidget::updateScreenModeUI()
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	ui->SreenModeTip->setToolTip(getScreenModeTip());
#endif

	int index = getCurrentScreenIndex();
	if (index == ui->combSreenMode->currentIndex())
	{
		return;
	}	

	if (ui->combSreenMode->count() > 0) {
		ui->combSreenMode->setCurrentIndex(index);
	}
}

const char* MainControlDockWidget::getScreenModeTip()
{
	auto mode = m_link_config->get_screen_mode();
	if (mode == wc::LiveMode_Horizontal) {
		return obs_module_text("Webcam.LandscapeModeTip");
	}
	else {
		return obs_module_text("Webcam.PortraitModeTip");
	}
}

void MainControlDockWidget::on_combSreenMode_currentIndexChanged(int index)
{
	int mode = ui->combSreenMode->currentData().toInt();
	if (mode == m_link_config->get_screen_mode()) {
		return;
	}

	QString err_message;
	bool streamming = obs_frontend_streaming_active();
	if (streamming) {
		err_message = obs_module_text("Webcam.CloseStreamingMessage");
	}
	if (err_message.isEmpty() && obs_frontend_recording_active()) {
		err_message = obs_module_text("Webcam.CloseRecordingMessage");
	}
	if (err_message.isEmpty() && obs_frontend_virtualcam_active()) {
		err_message = obs_module_text("Webcam.CloseVirtualCamMessage");
	}

	if (err_message.isEmpty() && (obs_video_active() || lovense::proc::is_multi_streaming_active()))
	{
		err_message = obs_module_text("Webcam.CloseStreamingMessage");
	}

	if (! err_message.isEmpty())
	{
		ui->combSreenMode->setCurrentIndex(this->getCurrentScreenIndex());
		QMessageBox::information(this, "information", err_message, QMessageBox::NoButton, QMessageBox::Ok);
		return;
	}

	//
	bool ok = UntilRes::SetLiveMode((wc::LiveMode)mode);
	if (ok) {
		m_link_config->set_screen_mode(mode);
	}
}

//-------------------------------------------------------------------------------------------------------
void MainControlDockWidget::setAutoTrackingUI(wc::WebcamModel model)
{
	if (model == wc::WebcamModel::WEBCAM_LINK) {
		ui->comboBoxTrackSpeed->blockSignals(true);
		ui->comboBoxTrackSpeed->clear();
		ui->comboBoxTrackSpeed->addItem("Fast", (int)TrackSpeed::Fast);
		ui->comboBoxTrackSpeed->addItem("Normal", (int)TrackSpeed::Normal);
		ui->comboBoxTrackSpeed->addItem("Slow", (int)TrackSpeed::Slow);
		ui->comboBoxTrackSpeed->blockSignals(false);

		ui->label_14->setText(obs_module_text("Webcam.EnableAutoTracking"));
	}
	else if (model == wc::WebcamModel::WEBCAM_LINK_2C) {
		ui->widgetTrackSpeed->setVisible(false);

		ui->label_14->setText(obs_module_text("Webcam.EnableAutoFraming"));
	}

	initAutoTrackingTip();
}

int MainControlDockWidget::getSpeedModeIndex(int mode)
{
	int index = 1;
	for (int i = 0; i < ui->comboBoxTrackSpeed->count(); i++)
	{
		if (mode == ui->comboBoxTrackSpeed->itemData(i).toInt())
		{
			index = i;
			break;
		}
	}
	return index;
}

void MainControlDockWidget::updateTrackSpeed(bool enable)
{
	ui->widgetTrackSpeed->setVisible(enable);
	if (enable) {
		TrackSpeed speed = TrackSpeed::Fast;
		CLink360Adapter::Impl()->UVCCEx()->GetTrackSpeed(speed);
		ui->comboBoxTrackSpeed->blockSignals(true);
		ui->comboBoxTrackSpeed->setCurrentIndex((int)speed);
		ui->comboBoxTrackSpeed->blockSignals(false);
	}
}

void MainControlDockWidget::initAutoTrackingTip()
{
	ui->autoTrackingTips->setVisible(false);

	auto text = string_util::format("<u>%s</u>", obs_module_text("Webcam.DisableAutoTrackingLink"));
	ui->closeAutoTrackingLink->setText(text.c_str());
	ui->closeAutoTrackingLink->setVisible(false);
	ui->closeAutoTrackingLink->installEventFilter(this);

#ifdef _WIN32
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	ui->closeAutoTrackingLink->setMinimumWidth(200);
#else
	ui->closeAutoTrackingLink->setMinimumWidth(250);
#endif
#else
	ui->closeAutoTrackingLink->setMinimumWidth(200);
#endif
	ui->closeAutoTrackingLink->setAlignment(Qt::AlignHCenter);
}

void MainControlDockWidget::showAutoTrackingTip()
{
	m_checkAutoTrackTime_ = time(nullptr);

	if (ui->autoTrackingTips->isVisible())
	{
		return ;
	}

	bool enable{ false };
	bool ok = CLink360Adapter::Impl()->IsEnaledAutoTracking(enable);
	if (!ok || (!enable)) {
		return;
	}
	
	ui->autoTrackingTips->setVisible(true);
	ui->closeAutoTrackingLink->setVisible(true);
}

void MainControlDockWidget::hideAutoTrackingTip(bool close_auto_track)
{
	ui->autoTrackingTips->setVisible(false);
	ui->closeAutoTrackingLink->setVisible(false);

	if (close_auto_track) {
		on_button_Enable_AutoTrack(false);
	}
}

void MainControlDockWidget::checkAutoTrackingTip()
{
	if (! ui->autoTrackingTips->isVisible())
	{
		return;
	}

	auto now = time(nullptr);
	if (now - m_checkAutoTrackTime_ >= 3) {
		hideAutoTrackingTip(false);
	}
}

//-------------------------------------------------------------------------------------------------------
void MainControlDockWidget::init_ai_zoom()
{
	{
		auto image      = wc::Resource::image("exclamation-point.png");
		auto image_size = QSize(15, 15);
		image           = image.scaled(image_size, Qt::KeepAspectRatio, Qt::SmoothTransformation);

		auto label      = new QLabel();
		label->setPixmap(image);

		QGraphicsColorizeEffect* effect = new QGraphicsColorizeEffect(label);
		effect->setColor(Qt::white);
		effect->setStrength(1); 
		label->setGraphicsEffect(effect);

		auto win_size = ui->buttonCompositFull->size();
		auto x = win_size.width() / 2 + win_size.width() % 2 - image_size.width() / 2;
		auto y = 20;
		label->move(x, y);
		label->setParent(ui->buttonCompositFull);

		m_composit_full_symbol = label;
	}

	{
		auto label = new QLabel();
		label->setText("Not supported");
		label->setStyleSheet("color: white; font-size: 8px;");

		auto win_size = ui->buttonCompositFull->size();
		auto x        = 5;
		auto y        = 36;
		label->move(x, y);
		label->setParent(ui->buttonCompositFull);

		m_composit_full_text = label;
	}

	ui->buttonCompositFull->installEventFilter(this);
	m_composit_full_symbol->installEventFilter(this);
	m_composit_full_text->installEventFilter(this);
}

void MainControlDockWidget::showFullBodyTip()
{
	if (m_fullBodyTip) {
		return;
	}

	m_fullBodyTip = new QLabel();
	m_fullBodyTip->setWordWrap(true);
	m_fullBodyTip->setAlignment(Qt::AlignLeft);
	m_fullBodyTip->setText(obs_module_text("Webcam.FullBodyTip"));

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	QFont font;
	font.setFamily(QString::fromUtf8("\345\276\256\350\275\257\351\233\205\351\273\221"));

	m_fullBodyTip->setFont(font);
#endif

	ui->fullBodyTipLayout->addWidget(m_fullBodyTip);
}

void MainControlDockWidget::hideFullBodyTip()
{
	if (m_fullBodyTip) {
		ui->fullBodyTipLayout->removeWidget(m_fullBodyTip);
		delete m_fullBodyTip;
		m_fullBodyTip = nullptr;
	}
}

//-------------------------------------------------------------------------------------------------------
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
void MainControlDockWidget::adjustFont(QWidget* widget)
{
	QFont font;
	font.setFamily(QString::fromUtf8("\345\276\256\350\275\257\351\233\205\351\273\221"));

	for (int i = 0; i < widget->children().size(); ++i) {
		auto w = dynamic_cast<QWidget*>(widget->children()[i]);
		if (w == nullptr) {
			continue;
		}

		w->setFont(font);

		MainControlDockWidget::adjustFont(w);
	}
}
#endif

void MainControlDockWidget::resizeEvent(QResizeEvent* event)
{
	auto size = event->size();
	int width = size.width();
	int height = size.height();

	int trisector = int(width / 3);

	ui->label->setMaximumWidth(160);
	ui->label_16->setMaximumWidth(trisector);
	ui->label_16->setWordWrap(true);
	ui->label_18->setMaximumWidth(trisector);
	ui->label_18->setWordWrap(true);
	ui->label_20->setMaximumWidth(trisector);
	ui->label_20->setWordWrap(true);

	ui->label_10->setMaximumWidth(135);

	QFontMetrics metrics(ui->tabWidget->font());

	//if (std::abs(event->oldSize().width() - width) > 5) {
	if (true) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
		int width = trisector - 30;
#else
		int width = trisector - 15;
#endif		
		if (width < 50) {
			width = 50;
		}
		ui->tabWidget->setStyleSheet(
			QString::fromUtf8(
				string_util::format("QTabBar::tab{ width:%d; }", width).c_str()));

		std::vector<const char*> label_txt = {
			obs_module_text("Webcam.GimbalControl"),
			obs_module_text("Webcam.Advanced"),
			obs_module_text("Webcam.ImagesSettings"),
		};

		for (int i = 0; i < ui->tabWidget->count(); i++) {
			if (i >= label_txt.size()) continue;

			const char* str_ptr = label_txt[i];
			if (!str_ptr) continue;

			QString str = QString(str_ptr);
			QString sub_str = metrics.elidedText(str, Qt::TextElideMode::ElideRight, width);

			QString last_str = str.mid(0, sub_str.size());
			ui->tabWidget->setTabText(i, last_str);
			
		}
	}
	
	QDockWidget::resizeEvent(event);
}

void MainControlDockWidget::InitInsta360()
{
	auto sdk_impl = CLink360Adapter::Impl();

    CameraControlInfo info;
    if (sdk_impl->UVCC()->GetZoomAbsoluteRange(info))
    {
		this->ui->sliderZoom->blockSignals(true);
		if (m_update_zoom) {
			this->ui->sliderZoom->setRange(info.min, info.max);			
			this->ui->sliderZoom->setSingleStep(info.step);
			ui->widget_ControlPanel->setZoomRange(info.min, info.max);
			m_update_zoom = false;
		}

		if (this->ui->sliderZoom->value() != info.cur) {
			this->ui->sliderZoom->setValue(info.cur);
		}

		this->ui->sliderZoom->blockSignals(false);
    }

	std::map<ExtendFuction, bool> ext_func_status;
	if (sdk_impl->UVCCEx()->GetExtendFuncStatus(ext_func_status)) {
		WSProtocol::instance()->update_extend_function_status(ext_func_status);

		InitImageSettings(ext_func_status, false);
		InitAdvanceSettings(ext_func_status);

		checkAutoTrackingTip();
	}
}

void MainControlDockWidget::InitResulotion()
{	
	int screen_mode = m_link_config->get_screen_mode();

	uint32_t width = 0, height = 0;
	std::vector<camera::ResolutionRadio> resolution;

	int index = CLink360Adapter::Impl()->GetMatchResolution(screen_mode, width, height, resolution);
	if (index < 0) {
		return;
	}

	bool reset{ false };
	if (m_resolutions_list != resolution) {
		ui->combCameraResolution->blockSignals(true);
		ui->combCameraResolution->clear();
		for (auto radio : resolution) {
			auto res_text = radio.str();
			auto t_string = radio.size_str();
			if (!t_string.empty()) {
#ifdef _WIN32
				res_text += "\t" + t_string;
#else
				std::string matchString(20, ' ');
				matchString.replace(0, res_text.length(), res_text);
				matchString.replace(matchString.length() - t_string.length(), t_string.length(), t_string);
				res_text = matchString;
#endif
			}
			ui->combCameraResolution->addItem(res_text.c_str());
		}
		ui->combCameraResolution->blockSignals(false);
		m_resolutions_list = resolution;
		WSProtocol::instance()->update_resolution(m_resolutions_list, index);
		reset = true;

	}

	if (m_frame_width != width || m_frame_height != height)
	{
		ui->combCameraResolution->blockSignals(true);
		ui->combCameraResolution->setCurrentIndex(index);
		ui->combCameraResolution->blockSignals(false);

		obs_source_t* source      = UntilRes::FindCurrentDshowSource();
		obs_source_t* scenesource = obs_frontend_get_current_scene();
		if (width > height) {			
			UntilRes::TransfromFixToScreen(obs_scene_from_source(scenesource),	source);			
		}
		else {
			UntilRes::TransfromSizeToScreen(obs_scene_from_source(scenesource),	source);
		}
		obs_source_release(scenesource);

		m_frame_width  = width;
		m_frame_height = height;

		ui->widget_ControlPanel->setResolutionRadio({ m_frame_width, m_frame_height });

		WSProtocol::BroadcastCameraStatus();
	}
}

//----------------------------------------------------------------------------
void MainControlDockWidget::initChatroomNotice()
{
#if 0
	ui->ChatroomNoticeLabel->setVisible(false);
	ui->ChatroomNoticeEveryLabel->setVisible(false);
	ui->comboBoxChatroomNoticeFrequency->setVisible(false);
	ui->chatroomNoticeTip->setVisible(false);
	ui->ChatroomNoticeMinuteLabel->setVisible(false);
#else
	ui->comboBoxChatroomNoticeFrequency->setFixedWidth(60);
	//ui->comboBoxChatroomNoticeFrequency->setMaximumWidth(60);

	ui->comboBoxChatroomNoticeFrequency->blockSignals(true);
	const int sFrequencys[] = { 1, 3, 5, 10, 15, 30, 60 };
	int frequency = m_link_config->get_chatroom_notice_frequency();
	int index{ 0 }, i{ 0 };

	for (int f : sFrequencys) {
		ui->comboBoxChatroomNoticeFrequency->addItem(std::to_string(f).c_str(), QVariant::fromValue(f));
		if (f == frequency) {
			index = i;
		}
		++i;
	}
	
	ui->comboBoxChatroomNoticeFrequency->setCurrentIndex(index);

	ui->comboBoxChatroomNoticeFrequency->blockSignals(false);

	//---------------------------------------
	//
	ui->chatroomNoticeTip->setStyleSheet("QToolButton{ border-style: flat; background: transparent; }");
	ui->chatroomNoticeTip->setToolButtonStyle(Qt::ToolButtonIconOnly);
	ui->chatroomNoticeTip->setAutoFillBackground(true);
	ui->chatroomNoticeTip->setIcon(wc::Resource::help());
	ui->chatroomNoticeTip->installEventFilter(this);

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	ui->chatroomNoticeTip->setToolTip(obs_module_text("Webcam.SupportWebSites"));
#endif

#endif
}

void MainControlDockWidget::on_comboBoxChatroomNoticeFrequency_currentIndexChanged(int index)
{
	//
	auto frequency = ui->comboBoxChatroomNoticeFrequency->itemData(index).toInt();
	auto cfg_frequency = m_link_config->get_chatroom_notice_frequency();
	if (frequency == cfg_frequency)
	{
		return;
	}

	m_link_config->set_chatroom_notice_frequency(frequency);

	WSProtocol::BroadcastTokenInfo();
}

//----------------------------------------------------------------------------
MainControlDockWidget::~MainControlDockWidget()
{
	ui->widget_ControlRocker->SetPanTiltCallback(nullptr);

	hideFullBodyTip();

	if (m_link_controller) {
		delete m_link_controller;
		m_link_controller = nullptr;
	}

	if (m_help_timer) {
		m_help_timer->stop();
		delete m_help_timer;
		m_help_timer = nullptr;
	}

    delete ui;
}

#if 0
void MainControlDockWidget::OnPositionWidgetDeleteEvent()
{
	if (ui->buttonAddControl) {
		ui->buttonAddControl->setEnabled(m_link_config->is_enable_control());;
	}
}
#endif

//-----------------------------------------------------------------------------------------
void MainControlDockWidget::OnPanControlEvent(PanDirection direction, bool btnRelease)
{
	if (m_checkShowAutoTrackingTip)
	{
		showAutoTrackingTip();
	}

	CLink360Adapter::Impl()->MoveDirection(direction, !btnRelease);
}

void MainControlDockWidget::OnPanControlUpdatePanTitlValue(int /*x*/, int /*y*/, bool move)
{
	if (m_checkShowAutoTrackingTip)
	{
		showAutoTrackingTip();
	}
}

void MainControlDockWidget::OnControlPanelMoved(uint16_t x, uint16_t y, int zoom_val)
{
	CLink360Adapter::Impl()->SetHostPTZ(x, y, zoom_val);
}

void MainControlDockWidget::OnControlPanelMovedOver(uint16_t x, uint16_t y, int zoom_val)
{
	WSProtocol::BroadcastCameraStatus();
}

//-----------------------------------------------------------------------------------------
void MainControlDockWidget::on_comboBox_Webcams_currentIndexChanged(int index)
{
	auto data = ui->comboBox_Webcams->itemData(index).value<wc::Camera>();
	CLink360Adapter::GetInstance()->SelectLink(data);
}

//-----------------------------------------------------------------------------------------
void MainControlDockWidget::on_buttonPanReset_clicked()
{
	if (CLink360Adapter::Impl()->Reset()) {
		ui->sliderZoom->setValue(100);
		WSProtocol::BroadcastReset();
	}
}


void MainControlDockWidget::on_buttonZoomSubtract_clicked()
{
    this->ui->sliderZoom->setValue(this->ui->sliderZoom->value() - this->ui->sliderZoom->singleStep());
	on_sliderZoom_sliderMoved(this->ui->sliderZoom->value());
}


void MainControlDockWidget::on_buttonZoomAdd_clicked()
{
    this->ui->sliderZoom->setValue(this->ui->sliderZoom->value()+ this->ui->sliderZoom->singleStep());
	on_sliderZoom_sliderMoved(this->ui->sliderZoom->value());
}

void MainControlDockWidget::on_sliderZoom_sliderMoved(int position)
{	
	if (CLink360Adapter::Impl()->UVCC()->SetZoomAbsolute(position)) {
		ui->widget_ControlPanel->setZoom(position);
	}
}

void MainControlDockWidget::on_sliderZoom_valueChanged(int val)
{
    on_sliderZoom_sliderMoved(val);
}

void MainControlDockWidget::on_sliderColortemperature_sliderMoved(int position)
{
	if (CLink360Adapter::Impl()->SetWhitebalanceTemperatureValue(position)) {
		ui->lableColortemperature->setText(QString("%1k").arg(position));
	}   
}

void MainControlDockWidget::on_sliderBrightness_sliderMoved(int position)
{
	CLink360Adapter::Impl()->SetBrightnessValue(position);
}

void MainControlDockWidget::on_sliderContrast_sliderMoved(int position)
{  
	CLink360Adapter::Impl()->SetContrastValue(position);
}

void MainControlDockWidget::on_sliderChroma_sliderMoved(int position)
{
	CLink360Adapter::Impl()->SetSaturationValue(position);
}

void MainControlDockWidget::on_sliderAcutance_sliderMoved(int position)
{
	CLink360Adapter::Impl()->SetSharpnessValue(position);
}

void MainControlDockWidget::on_button_Enable_Composition(bool enable) 
{  
	auto impl = CLink360Adapter::Impl();
	CompositionStyle style = CompositionStyle::None;
	if (impl->EnableAIZoom(enable, style)) {
		CompositionStatusChange(style);
	}
}

void MainControlDockWidget::on_button_Enable_AutoTrack(bool enable) {

	auto impl = CLink360Adapter::Impl();
	if (!impl->EnableAutoTracking(enable))
	{
		return;
	}

	if (!enable) {
		impl->UVCCEx()->EnableExtendFuncWork(ExtendFuction::AiZoom, false);
	}

	if (impl->SupportTrackSpeed()) {
		updateTrackSpeed(enable);
	}

	ui->widget_ControlPanel->setEnabled(!enable);
	if(enable)
		on_buttonPanReset_clicked();
	//
	WSProtocol::BroadcastCameraStatus();
}

void MainControlDockWidget::on_button_Enable_AutoFouce(bool enable) {
    CLink360Adapter* app = CLink360Adapter::GetInstance();
    app->GetImpl()->UVCC()->EnableAutoFocus(enable);
    ui->widgetAutoFouce->setVisible(!enable);
    if (!enable) {
        CameraControlInfo info;
        app->GetImpl()->UVCC()->GetFocusAbsoluteRange(info);;
        ui->sliderFouceRange->setRange(info.min, info.max);
        ui->sliderFouceRange->setValue(info.cur);
        ui->sliderFouceRange->setSingleStep(info.step);
    }
}

void MainControlDockWidget::on_button_Enable_HDRMode(bool enable) {
    //TODO:
	CLink360Adapter::Impl()->EnableExtendFuncWork(ExtendFuction::HDR, enable);
}

void MainControlDockWidget::on_button_Enable_MirorMode(bool enable) {
	CLink360Adapter::Impl()->EnableExtendFuncWork(ExtendFuction::Mirror, enable);
}

void MainControlDockWidget::on_button_Enable_MirrorVertically(bool enable)
{
	CLink360Adapter::Impl()->EnableExtendFuncWork(ExtendFuction::MirrorVertically, enable);
}


void MainControlDockWidget::UpdateComposition(CompositionStyle style) {
	auto sdk_impl = CLink360Adapter::Impl();
	if (sdk_impl->model() == wc::WebcamModel::WEBCAM_LINK_2C) {
		if (style >= CompositionStyle::FullBody) {
			return;
		}
	}
 
	if (!sdk_impl->UVCCEx()->SetCompositionStyle(style)) {
		return;
	}

	if (!sdk_impl->UVCCEx()->EnableExtendFuncWork(ExtendFuction::AiZoom, style != CompositionStyle::None)) {
		return;
	}

    CompositionStatusChange(style);
}

void MainControlDockWidget::on_buttonCompositHead_clicked(bool checked)
{
    UpdateComposition(CompositionStyle::OnlyHead);
}


void MainControlDockWidget::on_buttonCompositHalt_clicked(bool checked)
{
    UpdateComposition(CompositionStyle::HalfBody);
}


void MainControlDockWidget::on_buttonCompositFull_clicked(bool checked)
{
    UpdateComposition(CompositionStyle::FullBody);
}

void MainControlDockWidget::on_sliderExposure_valueChanged(int value)
{
    CLink360Adapter* app = CLink360Adapter::GetInstance();
    float fvalue = float(value) / 10;
    bool ret = app->GetImpl()->SetExposureCompensation(fvalue);
    ui->labelExposure->setText(QString("%1EV").arg((double)fvalue, 3, 'f', 1, QLatin1Char('0')));
}

void MainControlDockWidget::on_sliderIOS_valueChanged(int value)
{
	CLink360Adapter* app = CLink360Adapter::GetInstance();
	app->GetImpl()->SetISOValue(value);
	ui->labelIOS->setText(QString("%1").arg(value));
}

void MainControlDockWidget::on_sliderShutterSpeed_valueChanged(int value)
{
	CLink360Adapter* app = CLink360Adapter::GetInstance();

	if (m_speed_list_.empty()) {
		return;
	}

	long v = 30;
	if (value >= 0 && value < m_speed_list_.size()) {
		v = m_speed_list_[value];	 
	}

	app->GetImpl()->SetShutterSpeed(v);
	ui->labelShutterSpeed->setText(QString("1/%1").arg(v));
}

void MainControlDockWidget::on_button_AddColorPreset_clicked()
{
	ui->widget_ColorPresets->on_add_event();	
}

void MainControlDockWidget::on_button_Enable_AutoExposure(bool enable) {
	
	auto sdk_impl = CLink360Adapter::Impl();
	if (!sdk_impl->EnableAutoExposure(enable)) {
		return;
	}

	showAutoExposureUI(enable);
    if (enable) {
		float value = 0;
		//sdk_impl->UVCCEx()->GetExposureCompensation(value);
		setEnableExposureValue(value);
	}
	else {
		auto model = sdk_impl->model();
		
		uint16_t ios_value{ 1200 };
		long     shutter_speed{ 160 };
		
		sdk_impl->UVCCEx()->SetISOValue(ios_value);
		sdk_impl->UVCC()->SetShutterSpeed(shutter_speed);

		setDisableExposureValue(ios_value, shutter_speed);		
	}

}

void MainControlDockWidget::on_button_Enable_AutoWhitebalance(bool enable)//Auto White Balance
{
    CLink360Adapter* app = CLink360Adapter::GetInstance();
    uint8_t mode = enable ? 1 : 0;
    bool ret = app->GetImpl()->UVCC()->SetWhitebalanceTemperatureMode(mode);
    ui->sliderColortemperature->setEnabled(!enable);
    if (!enable) {
        CameraControlInfo info;
        ret = app->GetImpl()->UVCC()->GetWhitebalanceTemperatureRange(info);
        if (ret) {
            ui->sliderColortemperature->setRange(info.min, info.max);
            ui->sliderColortemperature->setValue(info.cur);
            ui->sliderColortemperature->setSingleStep(info.step);
        }
    }
    assert(ret);
}

#if 0
void MainControlDockWidget::on_button_RestoreFactorySettings_clicked(bool checked)
{
	CLink360Adapter::Impl()->UVCCEx()->RestoreFactorySet(true);
}
#endif

void MainControlDockWidget::on_buttonImagesSettingsReset_clicked(bool checked)
{
	std::map<ExtendFuction, bool> ext_func_status;
	if (!CLink360Adapter::Impl()->UVCCEx()->GetExtendFuncStatus(ext_func_status)) {
		return;
	}

    InitImageSettings(ext_func_status, true);
}

void MainControlDockWidget::initImageSettingsUI()
{
	const int min_w = 40;

	ui->labelExposure->setMinimumWidth(min_w);
	ui->labelIOS->setMinimumWidth(min_w);
	ui->labelShutterSpeed->setMinimumWidth(min_w);

	ui->lableColortemperature->setMinimumWidth(min_w);

	ui->sliderIOS->setFixedWidth(200);
	ui->sliderShutterSpeed->setFixedWidth(200);
	ui->sliderExposure->setFixedWidth(200);
	ui->sliderColortemperature->setFixedWidth(200);

	showAutoExposureUI(true);
}

void MainControlDockWidget::showAutoExposureUI(bool is_auto, bool enable_hdr)
{
	if (enable_hdr) {
		ui->widget_ImageExposure->setToolTip(obs_module_text("Webcam.DisabledExposureTips"));
		ui->buttonAutoExposure->setEnabled(false);
		ui->sliderExposure->setEnabled(false);
		//
		ui->sliderIOS->setEnabled(false);

		ui->sliderShutterSpeed->setEnabled(false);
	}
	else {
		ui->widget_ImageExposure->setToolTip("");
		ui->buttonAutoExposure->setEnabled(true);
		ui->buttonAutoExposure->setChecked(is_auto);

		ui->sliderExposure->setVisible(is_auto);
		ui->sliderExposure->setEnabled(is_auto);
		ui->labelExposure->setVisible(is_auto);
		//
		ui->sliderIOS->setVisible(!is_auto);
		ui->sliderIOS->setEnabled(!is_auto);
		ui->labelIOS->setVisible(!is_auto);


		ui->labelShutterSpeed->setVisible(!is_auto);
		ui->sliderShutterSpeed->setVisible(!is_auto);
		ui->sliderShutterSpeed->setEnabled(!is_auto);
	}
	
}

void MainControlDockWidget::setEnableExposureValue(float value)
{
	CameraControlInfo info;
	CLink360Adapter::Impl()->GetExposureCompensationControlInfo(info);

	ui->sliderExposure->setRange(info.min, info.max);
	ui->sliderExposure->setSingleStep(info.step);	
	ui->sliderExposure->setValue(value*10);
	ui->labelExposure->setText(QString("%1EV").arg((double)value, 3, 'f', 1, QLatin1Char('0')));
}


//-------------------------------------------------------------------------------------------------------
int MainControlDockWidget::get_shutter_speed_index(long speed)
{
	auto idx = (int)m_speed_list_.size() - 1;
	for (int i = 0; i < (int)m_speed_list_.size(); i++)
	{
		if (speed < m_speed_list_[i])
		{
			idx = i;
		}
		else if (speed == m_speed_list_[i])
		{
			idx = i;
			break;
		}
	}

	return idx;
}


void MainControlDockWidget::setDisableExposureValue(uint16_t isoValue, long shutterSpeed)
{
	auto sdk_impl = CLink360Adapter::Impl();
	CameraControlInfo info;
	sdk_impl->GetISOControlInfo(info);
	//ui
	//iso
	ui->sliderIOS->blockSignals(true);
	ui->sliderIOS->setRange(info.min, info.max);
	ui->sliderIOS->setSingleStep(info.step);
	ui->sliderIOS->setValue(isoValue);
	ui->sliderIOS->blockSignals(false);

	ui->sliderIOS->setMouseTracking(false);
	ui->labelIOS->setText(QString("%1").arg(isoValue));

	//Shutter
	if (m_speed_list_.empty()) {
		sdk_impl->UVCC()->GetShutterSpeedList(m_speed_list_);

		/*if (m_speed_list_.empty()) {
			m_speed_list_ = {
				8000,6400,5000,4000,3200,2500,2000,1600,1250,1000,800,640,500,400,320,240,200,160,120,100,80,60,50,40,30
			};
		}*/
	}

	if (m_speed_list_.empty()) {
		return;
	}

	auto idx = get_shutter_speed_index(shutterSpeed);
	
	ui->sliderShutterSpeed->blockSignals(true);
	ui->sliderShutterSpeed->setRange(0, (int)m_speed_list_.size() - 1);
	ui->sliderShutterSpeed->setSingleStep(1);
	ui->sliderShutterSpeed->setValue(idx);
	ui->sliderShutterSpeed->blockSignals(false);
	ui->labelShutterSpeed->setText(QString("1/%1").arg(shutterSpeed));
}


void MainControlDockWidget::InitImageSettings(std::map<ExtendFuction, bool>& ext_func_status, bool reset) {

	CLink360Adapter* app = CLink360Adapter::GetInstance();
	if (reset) {
		app->GetImpl()->ResetImageSettings();
	}

	wc::ImageSettings setting;

    bool ret    = false;
    bool enable = false;

	ret = app->GetImpl()->UVCCEx()->GetAutoExposureStatus(enable);
	if (ret) {
		showAutoExposureUI(enable, ext_func_status[ExtendFuction::HDR]);

		setting.AutoExposure = enable;

		if (enable) {
			float value = 0; //�Զ��ع�			
			if (app->GetImpl()->UVCCEx()->GetExposureCompensation(value)) {
				setEnableExposureValue(value);
				setting.ExposureCompensation = value;
			}		
		}
		else {
			uint16_t  isoValue{0};
			long      shutterSpeed{ 0 };
			if (app->GetImpl()->UVCCEx()->GetISOValue(isoValue)
				&& app->GetImpl()->UVCC()->GetShutterSpeed(shutterSpeed))
			{
				setDisableExposureValue(isoValue, shutterSpeed);

				setting.ISOValue     = isoValue;
				setting.ShutterSpeed = shutterSpeed;
			}			
		}
	}
   
    CameraControlInfo info;
    uint8_t  balanceAuto = 0;
    uint16_t balanceValue = 0;

    ret = app->GetImpl()->UVCC()->GetWhitebalanceTemperatureMode(balanceAuto);
	setting.WhitebalanceTemperatureMode = balanceAuto;
    //if (!balanceAuto) {//Not auto balance;
        ret = app->GetImpl()->UVCC()->GetWhitebalanceTemperatureRange(info);
        if (ret) {
            ui->sliderColortemperature->setRange(info.min, info.max);
            ui->sliderColortemperature->setValue(info.cur);
            ui->sliderColortemperature->setSingleStep(info.step);
            ui->lableColortemperature->setText(QString("%1k").arg(info.cur));

			WSProtocol::instance()->update_whitebalance(info);

			setting.WhitebalanceTemperatureValue = info.cur;
        }
    //}
    ui->buttonWhitebalance->setChecked(balanceAuto);
    ui->sliderColortemperature->setEnabled(!balanceAuto);

    ret = app->GetImpl()->UVCC()->GetBrightnessRange(info);
    if (ret) {
        ui->sliderBrightness->setRange(info.min, info.max);
        ui->sliderBrightness->setValue(info.cur);
        ui->sliderBrightness->setSingleStep(info.step);

		WSProtocol::instance()->update_brightness(info);

		setting.BrightnessValue = info.cur;
    }   

    ret = app->GetImpl()->UVCC()->GetContrastRange(info);//default 50
    if (ret) {
        ui->sliderContrast->setRange(info.min, info.max);
        ui->sliderContrast->setValue(info.cur);
        ui->sliderContrast->setSingleStep(info.step);

		WSProtocol::instance()->update_contrast(info);

		setting.ContrastValue = info.cur;
    }

    ret = app->GetImpl()->UVCC()->GetSaturationRange(info);
    if (ret) {
        ui->sliderChroma->setRange(info.min, info.max);
        ui->sliderChroma->setValue(info.cur);
        ui->sliderChroma->setSingleStep(info.step);

		WSProtocol::instance()->update_saturation(info);

		setting.SaturationValue = info.cur;
    }

    ret = app->GetImpl()->UVCC()->GetSharpnessRange(info);
    if (ret) {
        ui->sliderAcutance->setRange(info.min, info.max);
        ui->sliderAcutance->setValue(info.cur);
        ui->sliderAcutance->setSingleStep(info.step);

		WSProtocol::instance()->update_sharpness(info);

		setting.SharpnessValue = info.cur;
    }

	WSProtocol::instance()->update_image_settings(setting);
}

void MainControlDockWidget::InitAdvanceSettings(std::map<ExtendFuction, bool>& enable_status)
{
	VideoMode mode{ (VideoMode)-1 };
    VideoModeAuxiliaryData extenddata;
    CLink360Adapter* app = CLink360Adapter::GetInstance();

	bool is_tracking{ false };

	auto sdk_impl = app->GetImpl();
	if (sdk_impl->UVCCEx()->GetVideoMode(mode, extenddata)) {
		is_tracking = sdk_impl->IsEnaledAutoTracking(mode);

		ui->buttonAutoTrack->setChecked(is_tracking);
		WSProtocol::instance()->update_is_tracking(is_tracking);
		WSProtocol::instance()->update_host_coord(extenddata.host_x, extenddata.host_y);

		if (sdk_impl->SupportTrackSpeed()) {
			updateTrackSpeed(is_tracking);
		}

		ui->widget_ControlPanel->setEnabled(!is_tracking);
		if (m_cam_model == wc::WebcamModel::WEBCAM_LINK_2C) {
			ui->buttonPanReset->setEnabled(!is_tracking);
		}


		m_checkShowAutoTrackingTip = is_tracking;

		ui->widget_ControlPanel->update(extenddata);
		//-------------------------------------------------------------
	}

    //std::map<ExtendFuction, bool> enable_status;
	if (!enable_status.empty()) {
		if ((int)mode >= 0) {
			CompositionStyle style{ CompositionStyle::None };
			bool             enable_ai_zoom{ false };
			if (is_tracking) {
				enable_ai_zoom = enable_status[ExtendFuction::AiZoom];
				ui->btnCompositionEnable->setChecked(enable_ai_zoom);
				sdk_impl->GetCompositionStyle(style);

				if (enable_ai_zoom) {
					CompositionStatusChange(style);
				}
				else {
					CompositionStatusChange(CompositionStyle::None);
				}
			}
			else {
				ui->btnCompositionEnable->setChecked(false);
				CompositionStatusChange(CompositionStyle::None);
			}

			WSProtocol::instance()->update_ai_zoom(enable_ai_zoom, (int)style);
		}

		if (!enable_status[ExtendFuction::VScreen]) {
			sdk_impl->UVCCEx()->EnableVerticalScreen(true);
		}

		ui->buttonHDRMode->setChecked(enable_status[ExtendFuction::HDR]);
		ui->buttonMiroMode->setChecked(enable_status[ExtendFuction::Mirror]);
#if USE_SDK_V2
		ui->buttonVerticalFlip->setChecked(enable_status[ExtendFuction::MirrorVertically]);
#endif		
	}


	{
		bool status = false;
		sdk_impl->UVCC()->GetAutoFocusStatus(status);
		ui->buttonAutoFouce->setChecked(status);
		ui->widgetAutoFouce->setVisible(!status);
		if (!status) {
			CameraControlInfo info;
			sdk_impl->UVCC()->GetFocusAbsoluteRange(info);;
			ui->sliderFouceRange->setRange(info.min, info.max);
			ui->sliderFouceRange->setValue(info.cur);
			ui->sliderFouceRange->setSingleStep(info.step);
		}
	}

	{
		bool enable{ false };
		if (sdk_impl->IsEnableAllGesture(enable)) {
			ui->buttonGestureSettings->setChecked(enable);
			ui->buttonGestureTrack->setEnabled(enable);
			ui->buttonGestureZoom->setEnabled(enable);

			std::map<Gesture, bool> gesture_enable_status;
			if (sdk_impl->UVCCEx()->GetGestureStatus(gesture_enable_status)) {
				ui->buttonGestureTrack->setChecked(gesture_enable_status[Gesture::Gesture_Palm]);
				ui->buttonGestureZoom->setChecked(gesture_enable_status[Gesture::Gesture_L]);

				WSProtocol::instance()->update_gesture_status(gesture_enable_status);
			}
		}		
	}

	this->updateScreenModeUI();


	//
	{
		PowerLineFrequency value;
		if (sdk_impl->UVCC()->GetPowerLineFrequency(value)) {
			auto index = this->get_anti_flicker_index(value);
			if (index >= 0 && index != ui->comboBox_AntiFlicker->currentIndex()) {
				ui->comboBox_AntiFlicker->blockSignals(true);
				ui->comboBox_AntiFlicker->setCurrentIndex(index);
				ui->comboBox_AntiFlicker->blockSignals(false);
			}
		}
	}
}

//----------------------------------------------------------------------------------
void MainControlDockWidget::initAudioFeatureUI()
{

}

int MainControlDockWidget::getAudioFeatureIndex(int mode)
{
	int index = -1;
	for (int i = 0; i < ui->comboBox_AudioMode->count(); i++)
	{
		if (mode == ui->comboBox_AudioMode->itemData(i).toInt())
		{
			index = i;
			break;
		}
	}
	return index;
}

void MainControlDockWidget::on_comboBox_AudioMode_currentIndexChanged(int index)
{
	int mode = ui->comboBox_AudioMode->currentData().toInt();
	CLink360Adapter::Impl()->SetAudioCaptureMode(mode);
}

//--------------------------------------------------------
void MainControlDockWidget::init_color_preset()
{
	ui->tips_ColorPreset->setStyleSheet("QToolButton{ border-style: flat; background: transparent; }");
	ui->tips_ColorPreset->setToolButtonStyle(Qt::ToolButtonIconOnly);
	ui->tips_ColorPreset->setAutoFillBackground(true);
	ui->tips_ColorPreset->setIcon(wc::Resource::help());

	ui->tips_ColorPreset->installEventFilter(this);

	ui->widget_ColorPresets->set_add_button(ui->button_AddColorPreset);
}
//--------------------------------------------------------

void MainControlDockWidget::init_anti_flicker()
{
	ui->comboBox_AntiFlicker->blockSignals(true);
	ui->comboBox_AntiFlicker->addItem(obs_module_text("Webcam.ResistAuto"), (int)PowerLineFrequency::FREQUENCY_Auto);
	ui->comboBox_AntiFlicker->addItem(obs_module_text("Webcam.Resist50HZ"), (int)PowerLineFrequency::FREQUENCY_50HZ);
	ui->comboBox_AntiFlicker->addItem(obs_module_text("Webcam.Resist60HZ"), (int)PowerLineFrequency::FREQUENCY_60HZ);
	ui->comboBox_AntiFlicker->blockSignals(false);
}

int  MainControlDockWidget::get_anti_flicker_index(PowerLineFrequency frequency)
{
	int index = -1;
	for (int i = 0; i < ui->comboBox_AntiFlicker->count(); i++)
	{
		if ((int)frequency == ui->comboBox_AntiFlicker->itemData(i).toInt())
		{
			index = i;
			break;
		}
	}
	return index;
}

void MainControlDockWidget::on_comboBox_AntiFlicker_currentIndexChanged(int index)
{
	int frequency = ui->comboBox_AntiFlicker->currentData().toInt();
	auto sdk_impl = CLink360Adapter::Impl();
	sdk_impl->UVCC()->SetPowerLineFrequency((PowerLineFrequency)frequency);
}
//-------------------------------------------------------------------------------------------------
void MainControlDockWidget::switch_webcam_model(const wc::Camera& camera)
{
	m_link_config = wc::ConfigCenter::instance()->get();

	auto model = camera.model();

	setAutoTrackingUI(model);
	setControlUI(model);

	m_cam_model = model;

	if (model == wc::WebcamModel::WEBCAM_LINK) {
		this->setWindowTitle(obs_module_text("Webcam.Title"));

		ui->widget_VerticalFlip->setVisible(false);
		ui->groupBox_AudioMode->setVisible(false);

		ui->label_10->setText(obs_module_text("Webcam.AutoTracking"));

		//Tracking Speed
		ui->widgetTrackSpeed->setVisible(true);
		ui->label_14->setText(obs_module_text("Webcam.EnableAutoTracking"));

		//Portrait/Landscape Switch
		ui->widget_SreenMode->setVisible(true);
		ui->horizontalLayout_26->setContentsMargins(9, 9, 42, 9);

		ui->buttonPanReset->setEnabled(true);
	}
	else if (model == wc::WebcamModel::WEBCAM_LINK_2C) {
		this->setWindowTitle(obs_module_text("Webcam.Title2"));

		ui->widget_VerticalFlip->setVisible(true);
		ui->groupBox_AudioMode->setVisible(true);

		ui->label_10->setText(obs_module_text("Webcam.AutoFraming"));

		//Tracking Speed
		ui->widgetTrackSpeed->setVisible(false);
		ui->label_14->setText(obs_module_text("Webcam.EnableAutoFraming"));

		//Portrait/Landscape Switch
		ui->widget_SreenMode->setVisible(false);
		ui->horizontalLayout_26->setContentsMargins(9, 9, 0, 9);
	}

	m_frame_width  = 0;
	m_frame_height = 0;
	m_update_zoom  = true;
	m_speed_list_.clear();

	LoadConfigWidget(camera);

	if (model == wc::WebcamModel::WEBCAM_LINK) {
		//Restore Portrait/Landscape Mode
		auto mode = m_link_config->get_screen_mode();
		UntilRes::SetLiveMode((wc::LiveMode)mode);		
	}
	else {
		//link2c does not support portrait mode
		UntilRes::SetLiveMode(wc::LiveMode::LiveMode_Horizontal);
	}

	{
		CompositionStatusChange(CompositionStyle::None, false, true);		
	}

	WSProtocol::BroadcastReset();
	//WSProtocol::BroadcastTokenInfo(true);
}

void MainControlDockWidget::refreshSwitchWebcamUI(const std::vector<wc::Camera>& active_camera_list, int index)
{
	bool more_webcam_visible = ui->comboBox_Webcams->isVisible();
	int active_webcam_size   = active_camera_list.size();
	if (active_webcam_size > 1)
	{
		//Available camera list has changed
		if (m_last_active_camera_list != active_camera_list) {

			ui->comboBox_Webcams->blockSignals(true);
			ui->comboBox_Webcams->clear();
			for (int i = 0; i < (int)active_camera_list.size(); i++)
			{
				const auto& it = active_camera_list[i];
				auto data = QVariant::fromValue(it);
				bool x = data.canConvert<wc::Camera>();
				ui->comboBox_Webcams->addItem(it.get_show_text().c_str(), data);
			}
			if (!active_camera_list.empty()) {
				ui->comboBox_Webcams->setCurrentIndex(index);
			}

			ui->comboBox_Webcams->blockSignals(false);

			m_last_active_camera_list = active_camera_list;
		}
		else {
			//
			if (ui->comboBox_Webcams->currentIndex() != index) {
				ui->comboBox_Webcams->blockSignals(true);
				ui->comboBox_Webcams->setCurrentIndex(index);
				ui->comboBox_Webcams->blockSignals(false);			
			}
		}

		//m_last_camera_index = index;
		if (!more_webcam_visible) {
			ui->comboBox_Webcams->setVisible(true);
		}
	}
	else {
		if (more_webcam_visible) {
			ui->comboBox_Webcams->setVisible(false);
		}
	}
}

//-------------------------------------------------------------------------------------------------
void MainControlDockWidget::refresh_status(bool normal, const std::vector<wc::Camera>& active_camera_list, int index)
{
	this->refreshSwitchWebcamUI(active_camera_list, index);
	if (normal) {		
		this->InitResulotion();
		this->InitInsta360();
	}
	else {
		this->setVisible(false);
	}
}

void MainControlDockWidget::reset_status()
{
	CLink360Adapter::Impl()->ResetPTZBoundary();
	//Camera status has changed, notify frontend to clear data
	WSProtocol::BroadcastTokenInfo(this->isVisible());

	WSProtocol::BroadcastReset();
}

bool MainControlDockWidget::is_fireware_updating()
{
	if (m_link_controller) {
		return m_link_controller->is_fireware_updating();
	}

	return false;
}

void MainControlDockWidget::SetButtonImage(QPushButton* button, const char *image) 
{
	auto pixmap = wc::Resource::image(image);
	pixmap = pixmap.scaledToHeight(ui->buttonCompositHalt->height(), Qt::SmoothTransformation);

	QIcon icon(pixmap);
    button->setFlat(true);
    button->setAutoFillBackground(true);
    button->setIconSize(QSize(130, 130));
    button->setIcon(icon);
}

void MainControlDockWidget::CompositionStatusChange(CompositionStyle style, bool enable, bool force) {

	if (style < CompositionStyle::None || style > CompositionStyle::FullBody) {
		//fixed: link2c style == 0x04
		ui->buttonCompositHead->setEnabled(enable);
		ui->buttonCompositHalt->setEnabled(enable);
		if (m_cam_model == wc::WebcamModel::WEBCAM_LINK_2C) {
			//ui->buttonCompositFull->setEnabled(false);
			SetForbiddenCompositionStatus(CompositionStyle::FullBody, false);
		}
		else {
			SetForbiddenCompositionStatus(CompositionStyle::FullBody, true);
			ui->buttonCompositFull->setEnabled(enable);
		}
		return; 
	}
	
	if (m_composition_style == style && (!force)) {
		return;
	}

    switch (style) {
    case CompositionStyle::None: {
        SetButtonImage(ui->buttonCompositHead, "OnlyHead.png");
        SetButtonImage(ui->buttonCompositHalt, "Half.png");
        SetButtonImage(ui->buttonCompositFull, "FullBody.png");
    }break;
    case CompositionStyle::OnlyHead: {
        SetButtonImage(ui->buttonCompositHead, "OnlyHeadSelect.png");
        SetButtonImage(ui->buttonCompositHalt, "Half.png");
        SetButtonImage(ui->buttonCompositFull, "FullBody.png");

    }break;
    case CompositionStyle::HalfBody: {
        SetButtonImage(ui->buttonCompositHead, "OnlyHead.png");
        SetButtonImage(ui->buttonCompositHalt, "HalfSelect.png");
        SetButtonImage(ui->buttonCompositFull, "FullBody.png");
    }break;
    case CompositionStyle::FullBody: {
        SetButtonImage(ui->buttonCompositHead, "OnlyHead.png");
        SetButtonImage(ui->buttonCompositHalt, "Half.png");
        SetButtonImage(ui->buttonCompositFull, "FullBodySelect.png");
    }
    default:
        break;
    }
  
    bool btnEnable = style == CompositionStyle::None ? false : true;
    ui->buttonCompositHead->setEnabled(btnEnable);
    ui->buttonCompositHalt->setEnabled(btnEnable);
	if (m_cam_model == wc::WebcamModel::WEBCAM_LINK_2C) {
		//ui->buttonCompositFull->setEnabled(false);
		SetForbiddenCompositionStatus(CompositionStyle::FullBody, false);
	}
	else {
		SetForbiddenCompositionStatus(CompositionStyle::FullBody, true);
		ui->buttonCompositFull->setEnabled(btnEnable);
	}

	if (btnEnable && (CompositionStyle::FullBody == style)) {
		this->showFullBodyTip();
	}
	else {
		this->hideFullBodyTip();
	}

	m_composition_style = style;
}

void MainControlDockWidget::SetForbiddenCompositionStatus(CompositionStyle style, bool enable)
{
	if (style != CompositionStyle::FullBody) {
		return;
	}

	ui->buttonCompositFull->setEnabled(enable);
	ui->label_18->setEnabled(enable);
	m_composit_full_symbol->setVisible(!enable);
	m_composit_full_text->setVisible(!enable);
}

void MainControlDockWidget::on_button_Enable_GestureSettings(bool enable)
{
	if (CLink360Adapter::Impl()->EnableAllGesture(enable)) {
		ui->buttonGestureTrack->setEnabled(enable);
		ui->buttonGestureZoom->setEnabled(enable);
	}
}

void MainControlDockWidget::on_button_Enable_GestureTrack(bool enable)
{
    CLink360Adapter::Impl()->UVCCEx()->EnableGestureWork(Gesture::Gesture_Palm, enable);
}

void MainControlDockWidget::on_button_Enable_GestureZoom(bool enable)
{
	CLink360Adapter::Impl()->UVCCEx()->EnableGestureWork(Gesture::Gesture_L, enable);
}

void MainControlDockWidget::on_buttonZoomAdd_pressed()
{   
    this->ui->sliderZoom->setValue(this->ui->sliderZoom->value() + this->ui->sliderZoom->singleStep()+5);
	CLink360Adapter::Impl()->UVCC()->SetZoomAbsolute(this->ui->sliderZoom->value());
}


void MainControlDockWidget::on_buttonZoomSubtract_pressed()
{   
    this->ui->sliderZoom->setValue(this->ui->sliderZoom->value() - this->ui->sliderZoom->singleStep()-5);    
	CLink360Adapter::Impl()->UVCC()->SetZoomAbsolute(this->ui->sliderZoom->value());
}

void MainControlDockWidget::on_buttonAddPanPosition_clicked(bool checked)
{
	this->ui->widget_PresetPosition->on_add_event();
}

void MainControlDockWidget::on_comboBoxTrackSpeed_currentIndexChanged(int index)
{
	TrackSpeed speed = (TrackSpeed)ui->comboBoxTrackSpeed->itemData(index).toInt();
	CLink360Adapter::Impl()->UVCCEx()->SetTrackSpeed(speed);
}

void MainControlDockWidget::on_sliderFouceRange_valueChanged(int value)
{
	CLink360Adapter::Impl()->UVCC()->SetFocusAbsolute(value);
}

//------------------------------------------------------------------------
void MainControlDockWidget::on_buttonRemoteAdjust_clicked()
{
	this->open_remote_control();
}

//-------------------------------------------------------------------------------------------
void MainControlDockWidget::on_combCameraResolution_currentIndexChanged(int index)
{
#ifdef __APPLE__
    bool active = obs_frontend_virtualcam_active();
     if(active){
         QString contend = obs_module_text("Webcam.CloseVirtualCam");
         QMessageBox::information(this, "information", contend, QMessageBox::NoButton, QMessageBox::Ok);

         return;
     }
#endif
	QString res            = ui->combCameraResolution->currentText();
	std::string resolution = res.toStdString();
	obs_source_t *source   = UntilRes::FindCurrentDshowSource();
	if (source) {
		UntilRes::ResetCameraResolution(source, resolution);
	}
}

bool MainControlDockWidget::eventFilter(QObject* obj, QEvent* event) {
	if (obj->inherits("QAbstractSpinBox") || obj->inherits("QComboBox") ||
	    obj->inherits("QSlider")) {
		if (event->type() == QEvent::Wheel)
			return true;
	}

	//
	if (obj == ui->closeAutoTrackingLink)
	{
		if (event->type() == QEvent::MouseButtonPress)
		{
			QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event); 
			if (mouseEvent->button() == Qt::LeftButton)
			{
				this->hideAutoTrackingTip(true);
				return true;
			}			
		}
	
		return false;
	}

	if (this->update_help_tips(obj, event))
	{
		return true;
	}

	return QWidget::eventFilter(obj, event);
}


QString MainControlDockWidget::get_help_tips(QObject* obj)
{
	//
	if (obj == ui->chatroomNoticeTip) {
		return obs_module_text("Webcam.SupportWebSites");
	}
	else if (obj == ui->SreenModeTip) {
		return this->getScreenModeTip();
	}
	else if (obj == ui->tips_AudioMode) {
		return obs_module_text("Webcam.AudioModeTips");
	}
	else if (obj == ui->tips_ColorPreset) {
		return obs_module_text("Webcam.ColorPresetTips");
	}
	else if (obj == ui->buttonCompositFull
		|| obj == m_composit_full_symbol
		|| obj == m_composit_full_text
		) {
		return m_cam_model == wc::WebcamModel::WEBCAM_LINK_2C
			? obs_module_text("Webcam.DisableFullBodyTip") : "";
	}

	return "";
}

bool MainControlDockWidget::update_help_tips(QObject* obj, QEvent* event)
{
	QWidget* widget = dynamic_cast<QWidget*>(obj);
	if (widget == nullptr) {
		return false;
	}

	int t = event->type();
	switch (t)
	{		
	case QEvent::MouseMove:
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	case QEvent::MouseButtonPress:
	case QEvent::MouseButtonRelease:
	case QEvent::MouseButtonDblClick:
#endif
	{
		auto tips = this->get_help_tips(obj);
		if (tips.isEmpty()) {
			return false;
		}

		this->m_help_widget = widget;

		if (m_help_timer == nullptr) {
			m_help_timer = new QTimer();
			m_help_timer->setInterval(300);
			connect(m_help_timer, &QTimer::timeout, this, [this]() {
				auto pos = cursor().pos();
				if (pos == m_help_point) {
					m_help_timer->stop();
					m_help_point = QPoint(-1, -1);

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
					if (!QHelpTip::isVisible() && QHelpTip::owner() != this->m_help_widget) {
						QHelpTip::showText(pos, this->get_help_tips(this->m_help_widget), this->m_help_widget);
					}
					else {
						QHelpTip::move(pos);
					}
#else
					QToolTip::showText(pos, this->get_help_tips(this->m_help_widget), widget);
#endif
					
				}
			});
		}
		else {
			m_help_timer->stop();
		}

		m_help_point = cursor().pos();
		m_help_timer->start();

		return true;
	}	
	default:
		break;
	}

	return false;
}

void MainControlDockWidget::LoadConfigWidget(const wc::Camera& camera)
{
	if (!m_link_config) return;

	ui->widget_ColorPresets->set_config(m_link_config);
	ui->widget_PresetPosition->set_config(m_link_config);
}

bool MainControlDockWidget::add_preset_position(wc::TokenRule rule)
{
	return ui->widget_PresetPosition->add(rule);
}

bool MainControlDockWidget::modify_preset_position(wc::TokenRule rule, std::string name)
{
	return ui->widget_PresetPosition->modify(rule, name);
}

bool MainControlDockWidget::update_preset_position(std::string name)
{
	return ui->widget_PresetPosition->update(name);
}

bool MainControlDockWidget::remove_preset_position(std::string name)
{
	return ui->widget_PresetPosition->remove(name);
}

bool MainControlDockWidget::move_up_preset_position(std::string name)
{
	return ui->widget_PresetPosition->move_up(name);
}

bool MainControlDockWidget::add_preset_color(std::string name)
{
	return ui->widget_ColorPresets->add(QString::fromStdString(name));
}

bool MainControlDockWidget::modify_preset_color(std::string old_name, std::string new_name)
{
	return ui->widget_ColorPresets->modify(
		QString::fromStdString(old_name),
		QString::fromStdString(new_name));
}

bool MainControlDockWidget::update_preset_color(std::string name)
{
	return ui->widget_ColorPresets->update(QString::fromStdString(name));
}

bool MainControlDockWidget::remove_preset_color(std::string name)
{
	return ui->widget_ColorPresets->remove(QString::fromStdString(name));
}

bool MainControlDockWidget::move_up_preset_color(std::string name)
{
	return ui->widget_ColorPresets->move_up(QString::fromStdString(name));

}
