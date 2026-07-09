#pragma execution_character_set("utf-8")
#include "PositionWidget.h"
#include <QHBoxLayout>
#include <QPushButton>
#include <QSpinBox>
#include <QMenu>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLineEdit>
#include <QMessageBox>
#include <QDialog>
#include <QLabel>
#include <QDialogButtonBox>
#include "CLink360Adapter.h"
#include "WSProtocol.h"
#include "NewPositionDialog.h"
#include "PositionTokensData.h"

//std::map<std::string, std::shared_ptr<TokenRule>> s_globalRule;
TokenMap s_globalRule;
static int s_name_index = 1;
 QString GetRuleName() {

	 std::string name = "Position ";
	 std::string tName;
	 for (int i = 1; i < 999; i++) {
		 tName = name + std::to_string(i);
		 if (s_globalRule.find(tName) == s_globalRule.end())
		 break;
	 }

	return QString(tName.c_str());
}

int GetNewPositionTokenIndex()
{
	int index = (int)s_globalRule.size() - 1;
	for (auto it : s_globalRule)
	{
		if (it.second && it.second->m_index > index)
		{
			index = it.second->m_index;
		}
	}

	return index + 1;
}


void GetAllTokenInfo(TokenMap& rules) {
	rules = s_globalRule;
}

void GetAllTokenInfo(TokenVector& rules)
{
	for (auto it : s_globalRule)
	{
		rules.push_back(std::make_pair(it.first, it.second));
	}

	std::sort(rules.begin(), rules.end(),
		[](const TokenVectorItem& l, const TokenVectorItem& r) {
			return l.second->m_index <= r.second->m_index;
		});
}

int GetTokenCount() {
	return s_globalRule.size();
}


QRenameDialog::QRenameDialog(QWidget* parent , Qt::WindowFlags f):QDialog(parent,f){

}

void QRenameDialog::accept() {
	QString text = m_editName->text();
	if (s_globalRule.find(text.toStdString()) != s_globalRule.end()) {
		if (text == m_oldName) {
			done(QDialog::DialogCode::Accepted);
			return;
		}
		m_nameExistlabel->setText("The name already exists");
		m_nameExistlabel->setStyleSheet("color:red;");
	}
	else
		done(QDialog::DialogCode::Accepted);
}



PositionWidget::PositionWidget(QWidget *parent,QPushButton *addButton, std::shared_ptr<TokenRule> rule) : QWidget(parent),m_btnAddPositon(addButton)
{
	if (!rule)
		return;
	QHBoxLayout* layout = new QHBoxLayout();
	layout->setContentsMargins(0,3,0,3);

	//layout->SetMinimumSize(75, 75);

	this->setLayout(layout);
	QString name = QString(rule->m_position_name.c_str());
	m_btn_name = new QPushButton(name, this);
	m_btn_name->setText(name);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	m_btn_name->setMinimumSize(90, 20);
#else
	m_btn_name->setMinimumSize(95, 20);
#endif
	layout->addWidget(m_btn_name);
	connect(m_btn_name, &QPushButton::clicked, [=]
	{
		CLink360Adapter* adapter = CLink360Adapter::GetInstance();
		if (adapter->DeviceAalid()) {
			QString title = m_btn_name->text();
			std::string strtitle = title.toStdString();
			auto item = s_globalRule.find(title.toStdString());
			if (item != s_globalRule.end()) {
				adapter->GetImpl()->UVCCEx()->SetPanTitlAbsolute(item->second->m_panvalue, item->second->m_titlvalue);
				adapter->GetImpl()->UVCC()->SetZoomAbsolute(item->second->m_zoomvalue);
			}
		}
	});

	/*
	QSpinBox* spinboxToken = new QSpinBox(this);
	layout->addWidget(spinboxToken);
	spinboxToken->setMinimum(0);
	connect(spinboxToken, (void(QSpinBox::*)(int)) & QSpinBox::valueChanged, [=](int value)
	{
		QString title = m_btn_name->text();
		auto item = s_globalRule.find(title.toStdString());
		if (item != s_globalRule.end()) {
			item->second->m_token_value = value;
		}
		WSProtocol::BroadcastTokenInfo();
	});

	QSpinBox* spinboxTime = new QSpinBox(this);
	layout->addWidget(spinboxTime);
	spinboxTime->setMinimum(0);
	connect(spinboxTime, (void(QSpinBox::*)(int))&QSpinBox::valueChanged, [=](int value)
	{
		QString title = m_btn_name->text();
		auto item = s_globalRule.find(title.toStdString());
		if (item != s_globalRule.end()) {
			item->second->m_duration = value;
		}
		WSProtocol::BroadcastTokenInfo();
	});

	*/
	m_labelToken = new QLabel(this);
	m_labelToken->setAlignment(Qt::AlignCenter);
	layout->addWidget(m_labelToken);
	m_labelToken->setText(QString("%1").arg(rule->m_token_value));
	m_labelToken->installEventFilter(this);
	m_labelToken->setStyleSheet(
		"QLabel{background-color:rgb(60,64,75); border-width: 1px;border-style: solid;border-color: rgb(255, 170, 0);}"); 

	m_labelToken->setMaximumHeight(28);
	m_labelToken->setFrameShape(QFrame::Box);
	m_labelToken->setMinimumSize(75, 20);

	m_labelTime = new QLabel(this);
	m_labelTime->setAlignment(Qt::AlignCenter);
	layout->addWidget(m_labelTime);
	m_labelTime->setText(QString("%1").arg(rule->m_duration));
	m_labelTime->installEventFilter(this);
	m_labelTime->setStyleSheet(
		"QLabel{background-color:rgb(60,64,75); border-width: 1px;border-style: solid;border-color: rgb(255, 170, 0);}"); 
	m_labelTime->setMaximumHeight(28);
	m_labelTime->setFrameShape(QFrame::Box);
	m_labelTime->setMinimumSize(75, 20);

	QMenu* menu = new QMenu(this);
	QAction* pdelete = new QAction(menu);
	pdelete->setText("Delete");
	connect(pdelete, &QAction::triggered, [=]() {
		QString title = m_btn_name->text();
		auto item = s_globalRule.find(title.toStdString());
		if (item != s_globalRule.end()) {
			s_globalRule.erase(item);
		}
		WSProtocol::BroadcastTokenInfo();
		if (m_btnAddPositon && s_globalRule.size() < 5)
			m_btnAddPositon->setEnabled(true);
		this->deleteLater();
	});
	

	QAction* rename = new QAction(menu);
	rename->setText("Modify");
	connect(rename, &QAction::triggered,this, [&]() {
		QString name = this->m_btn_name->text();
		auto item = s_globalRule.find(name.toStdString());
		if (item == s_globalRule.end()) {
			return;
		}
		NewPositionDialog dlg(this,item->second);
		dlg.setWindowTitle("Modify");
		int ret = dlg.exec();
		if (ret == 1) {
			std::shared_ptr<TokenRule> newRule = dlg.GetRule();
			this->m_btn_name->setText(newRule->m_position_name.c_str());
			this->m_labelToken->setText(QString("%1").arg(newRule->m_token_value));
			m_labelTime->setText(QString("%1").arg(newRule->m_duration));
			if (item->first != newRule->m_position_name) {
				if (item != s_globalRule.end()) {
					s_globalRule.insert(std::make_pair(newRule->m_position_name, newRule));
					s_globalRule.erase(item);
				}
			}
			WSProtocol::BroadcastTokenInfo();
		}
	});

	QAction* update = new QAction(menu);
	update->setText("Update");
	connect(update, &QAction::triggered, [=]() {
		CLink360Adapter* adapter = CLink360Adapter::GetInstance();
		if (adapter->DeviceAalid()) {

			QString title = m_btn_name->text();
			auto item = s_globalRule.find(title.toStdString());
			if (item != s_globalRule.end()) {
				adapter->GetImpl()->UVCCEx()->GetPanTiltAbsoluteValue(item->second->m_panvalue, item->second->m_titlvalue);
				adapter->GetImpl()->UVCC()->GetZoomAbsolute(item->second->m_zoomvalue);
				WSProtocol::BroadcastTokenInfo();
			}
		}
	});
	menu->addAction(pdelete);
	menu->addAction(rename); 
	menu->addAction(update);


	m_btn_name->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_btn_name, &QPushButton::customContextMenuRequested, [=](const QPoint& pos)
	{
		menu->exec(QCursor::pos());
	});

	//Insert a token info;
	//std::shared_ptr<TokenRule> rule = std::make_shared<TokenRule>();
	//rule->m_position_name = name.toStdString();
	//std::string str = rule->m_position_name;
	CLink360Adapter* adapter = CLink360Adapter::GetInstance();
	if (adapter->DeviceAalid()) {
		
		//adapter->GetImpl()->UVCCEx()->GetPanTiltAbsoluteValue(rule->m_panvalue, rule->m_titlvalue);
		//adapter->GetImpl()->UVCC()->GetZoomAbsolute(rule->m_zoomvalue);
	}
	if (rule->m_index < 0) {
		rule->m_index = GetNewPositionTokenIndex();
	}
	s_globalRule.insert(std::make_pair(name.toStdString(),rule));
	WSProtocol::BroadcastTokenInfo();
	if (m_btnAddPositon && s_globalRule.size() >= 5)
		m_btnAddPositon->setEnabled(false);
}
#include <QEvent>
bool PositionWidget::eventFilter(QObject* obj, QEvent* event) {
	if (obj == m_labelToken || obj == m_labelTime) {
		if (event->type() == QEvent::MouseButtonDblClick) // For more events, please check the source code
		{
			QString name = this->m_btn_name->text();
			auto item = s_globalRule.find(name.toStdString());
			if (item == s_globalRule.end()) {
				return true;
			}
			NewPositionDialog dlg(this, item->second);
			dlg.setWindowTitle("Modify");
			int ret = dlg.exec();
			if (ret == 1) {
				std::shared_ptr<TokenRule> newRule =
					dlg.GetRule();
				this->m_btn_name->setText(
					newRule->m_position_name.c_str());
				this->m_labelToken->setText(QString("%1").arg(
					newRule->m_token_value));
				m_labelTime->setText(
					QString("%1").arg(newRule->m_duration));
				if (item->first != newRule->m_position_name) {
					if (item != s_globalRule.end()) {
						s_globalRule.insert(std::make_pair(
							newRule->m_position_name,
							newRule));
						s_globalRule.erase(item);
					}
				}
				WSProtocol::BroadcastTokenInfo();
			}
		}
	}
	return QWidget::eventFilter(obj, event);
 }
