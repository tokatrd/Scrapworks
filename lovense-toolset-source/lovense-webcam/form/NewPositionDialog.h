#ifndef NEWPOSITIONDIALOG_H
#define NEWPOSITIONDIALOG_H

#include <QDialog>
#include "WSProtocol.h"
namespace Ui {
class NewPositionDialog;
}

class TokenRule;

class NewPositionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NewPositionDialog(QWidget *parent = nullptr,std::shared_ptr<TokenRule> rule = nullptr);
    ~NewPositionDialog();

    virtual void accept();
    virtual int exec();
    std::shared_ptr<TokenRule> GetRule() { return m_rule; }
private:
    Ui::NewPositionDialog *ui;
    std::shared_ptr<TokenRule> m_rule = nullptr;
    QString m_oldName;
};

#endif // NEWPOSITIONDIALOG_H
