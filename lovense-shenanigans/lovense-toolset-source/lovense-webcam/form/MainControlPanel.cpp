#include "MainControlPanel.h"
#include "ui_MainControlPanel.h"

MainControlPanel::MainControlPanel(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MainControlPanel)
{
    ui->setupUi(this);
}

MainControlPanel::~MainControlPanel()
{
    delete ui;
}
