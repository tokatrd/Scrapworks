#include "NewPositionDialog.h"
#include "ui_NewPositionDialog.h"
#include "PositionTokensData.h"
extern QString GetRuleName();
//extern void GetAllTokenInfo(std::map<std::string, std::shared_ptr<TokenRule>>& rules);

NewPositionDialog::NewPositionDialog(QWidget *parent, std::shared_ptr<TokenRule> rule) :
    QDialog(parent),m_rule(rule),
    ui(new Ui::NewPositionDialog)
{
    ui->setupUi(this);
    QString name = GetRuleName();
    int token = 0 ;
    int duration = 0;
    if (m_rule) {
        name = QString(m_rule->m_position_name.c_str());
        token = m_rule->m_token_value;
        duration = m_rule->m_duration;
        m_oldName = name;
    }
    ui->spinBoxTokens->setRange(1,999999);
    ui->spinBoxDuration->setRange(1, 999999);

    ui->lineEditName->setText(name);
    ui->lineEditName->setMaxLength(30);
    ui->spinBoxTokens->setValue(token);
    ui->spinBoxDuration->setValue(duration);
    ui->labelTip->setStyleSheet("color:red;");

    Qt::WindowFlags flags = windowFlags();
    flags = flags | Qt::WindowStaysOnTopHint;
    setWindowFlags(flags);
}

NewPositionDialog::~NewPositionDialog()
{
    delete ui;
}


void NewPositionDialog::accept() {
    QString name = ui->lineEditName->text();
    if (name.isEmpty()) {
        ui->labelTip->setText("The name cannot be empty!");
        return;
    }
    TokenMap rules;
    GetAllTokenInfo(rules);
    int token = ui->spinBoxTokens->value();


    auto oldValue = rules.find(m_oldName.toStdString());
    for (auto item : rules) {
        if (m_oldName.isEmpty() || (oldValue != rules.end() && item.first != oldValue->first)) {
            if (token > 0 && item.second->m_token_value == token) {
                ui->labelTip->setText("This tip value already exists.");
                return;
            }
            if (item.second->m_position_name == name.toStdString()) {
                ui->labelTip->setText("The duration cannot be zero.");
                return;
            }
        }
    }

    if (!m_rule)
        m_rule = std::make_shared<TokenRule>();
    if (m_rule) {
        m_rule->m_position_name = name.toStdString();
        m_rule->m_token_value = token;
        m_rule->m_duration = ui->spinBoxDuration->value();
    }
    QDialog::accept();
}

int NewPositionDialog::exec() {
    return QDialog::exec();
}
