#ifndef PANCONTROLWIDGET_H
#define PANCONTROLWIDGET_H

#include <QWidget>
#include <QDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QDebug>
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QPoint>
#include <QTimer>

#define SMALL_CIRCLE_RADIUS 32 //Small circle radius (small circle image resolution is 60*60)
#define BIG_CIRCLE_RADIUS 70 //Large circle radius (large circle image resolution is 180*180)

class PanControlWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PanControlWidget(QWidget *parent = nullptr);
    ~PanControlWidget();

    QPoint BigCir_xy; //Large circle center coordinates
    QPoint SmallCir_xy; //Small circle center coordinates
    QPoint SmallCir_xy_init; //Initial small circle center coordinates
    bool MousePressFlag; //Mouse press flag
    QTimer *timer; //Timer for continuous movement

    void paintEvent(QPaintEvent *event); //Paint event: draw map, large circle and small circle in joystick
    void mouseMoveEvent(QMouseEvent *); //Mouse move event: implement joystick function
    void mousePressEvent(QMouseEvent *); //Mouse press event: get real-time coordinates of joystick
    void mouseReleaseEvent(QMouseEvent *); //Mouse release event: release mouse, reset MousePressFlag, set small circle center to initial value, update paint event, redraw to return small circle (joystick) to original position

signals:
    void OnPanControlUpdatePanTitlValue(int x, int y, bool move);

private:
    QPoint MapRemov_Old;

    QPixmap bigCircle_Pixmap;
    QPixmap smallCircle_Pixmap;

    void Init();
};

#endif // PANCONTROLWIDGET_H

