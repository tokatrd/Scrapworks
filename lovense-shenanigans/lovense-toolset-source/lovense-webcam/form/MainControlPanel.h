#ifndef MAINCONTROLPANEL_H
#define MAINCONTROLPANEL_H

#include <QWidget>

namespace Ui {
class MainControlPanel;
}

class MainControlPanel : public QWidget
{
    Q_OBJECT

public:
    explicit MainControlPanel(QWidget *parent = nullptr);
    ~MainControlPanel();

private:
    Ui::MainControlPanel *ui;
};

#endif // MAINCONTROLPANEL_H
