#include "PanControlWidget.h"
#include "CLink360Adapter.h"
#include "UntilHelp.h"
#include "json/CJsonObject.hpp"
#include <QApplication>

PanControlWidget::PanControlWidget(QWidget *parent)
    : QWidget(parent)
{
    Init(); //Initialization function
}

PanControlWidget::~PanControlWidget()
{
}

void PanControlWidget::Init()
{
    //Initialization function
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint); //Set window borderless and always on top
    BigCir_xy = QPoint(200, 200); //Set large circle center position
    SmallCir_xy = BigCir_xy; //Set small circle center position, same as large circle
    SmallCir_xy_init = BigCir_xy; //Set initial small circle center position
    setFixedSize(400, 400); //Set window fixed size to 400*400
    MousePressFlag = false; //Initialize mouse click flag
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this]() {
        OnPanControlUpdatePanTitlValue(SmallCir_xy.x() - BigCir_xy.x(), SmallCir_xy.y() - BigCir_xy.y(), true);
    });
}

void PanControlWidget::paintEvent(QPaintEvent *event)
{
    //Paint event: draw map, large circle and small circle in joystick
    Q_UNUSED(event);
    QPainter painter(this); //Create painter
    painter.setRenderHint(QPainter::Antialiasing); //Enable anti-aliasing
    painter.setRenderHint(QPainter::SmoothPixmapTransform); //Enable smooth pixmap transformation
    painter.drawPixmap(BigCir_xy.x() - BIG_CIRCLE_RADIUS, BigCir_xy.y() - BIG_CIRCLE_RADIUS, bigCircle_Pixmap); //Draw large circle in joystick
    painter.drawPixmap(SmallCir_xy.x() - SMALL_CIRCLE_RADIUS, SmallCir_xy.y() - SMALL_CIRCLE_RADIUS, smallCircle_Pixmap); //Draw small circle in joystick
}

void PanControlWidget::mouseMoveEvent(QMouseEvent *event)
{
    //Mouse move event: implement joystick function
    if (MousePressFlag)
    {
        SmallCir_xy = event->pos();
        double distance = sqrt(pow(SmallCir_xy.x() - BigCir_xy.x(), 2) + pow(SmallCir_xy.y() - BigCir_xy.y(), 2));
        if (distance > BIG_CIRCLE_RADIUS)
        {
            double angle = atan2(SmallCir_xy.y() - BigCir_xy.y(), SmallCir_xy.x() - BigCir_xy.x());
            SmallCir_xy.setX(BigCir_xy.x() + BIG_CIRCLE_RADIUS * cos(angle));
            SmallCir_xy.setY(BigCir_xy.y() + BIG_CIRCLE_RADIUS * sin(angle));
        }
        OnPanControlUpdatePanTitlValue(SmallCir_xy.x() - BigCir_xy.x(), SmallCir_xy.y() - BigCir_xy.y(), true);
        update();
    }
}

void PanControlWidget::mousePressEvent(QMouseEvent *event)
{
    //Mouse press event: get real-time coordinates of joystick
    if (event->button() == Qt::LeftButton)
    {
        MousePressFlag = true;
        SmallCir_xy = event->pos();
        double distance = sqrt(pow(SmallCir_xy.x() - BigCir_xy.x(), 2) + pow(SmallCir_xy.y() - BigCir_xy.y(), 2));
        if (distance > BIG_CIRCLE_RADIUS)
        {
            double angle = atan2(SmallCir_xy.y() - BigCir_xy.y(), SmallCir_xy.x() - BigCir_xy.x());
            SmallCir_xy.setX(BigCir_xy.x() + BIG_CIRCLE_RADIUS * cos(angle));
            SmallCir_xy.setY(BigCir_xy.y() + BIG_CIRCLE_RADIUS * sin(angle));
        }
        OnPanControlUpdatePanTitlValue(SmallCir_xy.x() - BigCir_xy.x(), SmallCir_xy.y() - BigCir_xy.y(), true);
        update();
    }
}

void PanControlWidget::mouseReleaseEvent(QMouseEvent *event)
{
    //Mouse release event: release mouse, reset MousePressFlag, set small circle center to initial value, update paint event, redraw to return small circle (joystick) to original position
    if (event->button() == Qt::LeftButton)
    {
        MousePressFlag = false;
        SmallCir_xy = SmallCir_xy_init;
        OnPanControlUpdatePanTitlValue(0, 0, false);
        update();
    }
}

void PanControlWidget::UpdatePanTitl(int x, int y, bool stop) {

    CLink360Adapter::OnPanTiltUpdate(x, y, !stop);
}

