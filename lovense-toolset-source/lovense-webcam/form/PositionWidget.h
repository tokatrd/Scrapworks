#ifndef POSITIONWIDGET_H
#define POSITIONWIDGET_H

#include <QWidget>
#include <QTimer>
#include <QColor>
#include <map>
#include <memory>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
//#include "WSProtocol.h"
#include "PositionTokensData.h"

class QRenameDialog : public QDialog {
    Q_OBJECT
public:
    explicit QRenameDialog(QWidget* parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());

    ~QRenameDialog() {}
    virtual void accept();

    QLineEdit* m_editName = nullptr;
    QLabel* m_nameExistlabel = nullptr;
    QString m_oldName;
};

class PositionWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PositionWidget(QWidget *parent = 0, QPushButton* addBtn = nullptr, std::shared_ptr<TokenRule> rule = nullptr);
    ~PositionWidget(){}

signals:


public slots:


public Q_SLOTS:


public:
    QPushButton* m_btnAddPositon = nullptr;;
    virtual bool eventFilter(QObject *obj, QEvent *event);

    private:
    QPushButton* m_btn_name;
    QLabel* m_labelToken;
    QLabel* m_labelTime;

};



#endif // SWITCHBUTTON_H
