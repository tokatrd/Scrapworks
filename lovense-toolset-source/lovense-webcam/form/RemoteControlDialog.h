#ifndef REMOTECONTROLDIALOG_H
#define REMOTECONTROLDIALOG_H

#include <QDialog>

namespace Ui {
class RemoteControlDialog;
}

class RemoteControlDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RemoteControlDialog(QWidget *parent = nullptr);
    ~RemoteControlDialog();

private:
    Ui::RemoteControlDialog *ui;
};

#endif // REMOTECONTROLDIALOG_H
