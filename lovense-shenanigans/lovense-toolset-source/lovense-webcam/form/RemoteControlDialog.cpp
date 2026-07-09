#include "RemoteControlDialog.h"
#include "ui_RemoteControlDialog.h"
#include "UntilHelp.h"
#include <QHostInfo>
#include <QNetworkInterface>
#include <QTcpSocket>
#include "../QrCode.hpp"
#include <QPainter>
#include "lovense_common.hpp"
#include "lovense_websocket_proc.hpp"

RemoteControlDialog::RemoteControlDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::RemoteControlDialog)
{
    ui->setupUi(this);
    this->setWindowTitle(QObject::tr("Webcam.RemoteAdjust"));
    Qt::WindowFlags flags = windowFlags();
    flags = flags & (~Qt::WindowContextHelpButtonHint) | Qt::WindowStaysOnTopHint;
    setWindowFlags(flags);
    this->setFixedSize(this->width(), this->height());

    QPixmap pixmap;
    bool ret{ false };
#if 0
    ret = pixmap.load(UntilRes::GetResourceImage("LearnScanQRCode.png").c_str());
    ui->labelLearnScanPic->setPixmap(pixmap);
#endif
    QPixmap pixmapZoom;
    ret = pixmapZoom.load(UntilRes::GetResourceImage("LearnScanQRCode.png").c_str());
    ui->labelQRCodePic->setPixmap(pixmap);

 //   const QHostAddress& localhost = QHostAddress(QHostAddress::LocalHost);
	//QString ipV4Address;
 //   for (const QHostAddress& address : QNetworkInterface::allAddresses()) {
	//	if (address.protocol() == QAbstractSocket::IPv4Protocol && address != localhost) {
	//		ipV4Address = address.toString();
	//		qDebug() << ipV4Address;
	//		qDebug() << address;
	//		//break;
	//	}
 //   }
    QTcpSocket socket;
    QString ipV4Address;
    socket.connectToHost("8.8.8.8",53); // google DNS, or something else reliable
    if (socket.waitForConnected(2000)) {
	    qDebug() << "local IPv4 address for Internet connectivity is"
		     << socket.localAddress();
	    ipV4Address = socket.localAddress().toString();
    } else {
	    qWarning() << "could not determine local IPv4 address:"
		       << socket.errorString();
    }
	qDebug()<<ipV4Address;
	QPixmap map(180, 180);
	map.fill(Qt::white);
	QPainter painter(&map);
	//ipV4Address = "";
	
	QString qrcodeContent = "https://apps.lovense.com/UploadFiles/camera-remote-control/index.html?lovenseflag=1&ip=";
	qrcodeContent += QString(ipV4Address);
	qrcodeContent += QString("&port=");

	auto wss_port = lovense::ws::proc_wss_GetPort();
	qrcodeContent += QString(std::to_string(wss_port).c_str());

	qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(qrcodeContent.toStdString().c_str(), qrcodegen::QrCode::Ecc::MEDIUM);
	const int s = qr.getSize() > 0 ? qr.getSize() : 1;
	const double w = map.width();
	const double h = map.height();
	const double aspect = w / h;
	const double size = ((aspect > 1.0) ? h : w);
	const double scale = size / (s + 2);
	painter.setPen(Qt::NoPen);
	painter.setBrush(Qt::black);

	for (int y = 0; y < s; y++) {
		for (int x = 0; x < s; x++) {
			const int color = qr.getModule(x, y);
			if (0x0 != color) {
				const double ry1 = (y + 1) * scale;
				const double rx1 = (x + 1) * scale;
				QRectF r(rx1, ry1, scale, scale);
				painter.drawRects(&r, 1);
			}
		}
	}

	ui->labelQRCodePic->setPixmap(map);
	ui->lableIpAdress->setText(ipV4Address);
	ui->lableIpAdress->setVisible(false);
	ui->label_2->setVisible(false);
}

RemoteControlDialog::~RemoteControlDialog()
{
    delete ui;
}
