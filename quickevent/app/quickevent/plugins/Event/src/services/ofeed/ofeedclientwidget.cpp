#include "ofeedclientwidget.h"
#include "ui_ofeedclientwidget.h"
#include "ofeedclient.h"
#include <qf/qmlwidgets/framework/mainwindow.h>

#include <qf/qmlwidgets/dialogs/messagebox.h>

#include <qf/core/assert.h>
#include <qf/core/log.h>

#include <QFileDialog>

#include <plugins/Event/src/eventplugin.h>
using qf::qmlwidgets::framework::getPlugin;

namespace Event {
namespace services {

OFeedClientWidget::OFeedClientWidget(QWidget *parent)
	: Super(parent)
    , ui(new Ui::OFeedClientWidget)
{
    setPersistentSettingsId("OFeedClientWidget");
	ui->setupUi(this);

    OFeedClient *svc = service();
	if(svc) {
        OFeedClientSettings ss = svc->settings();
		ui->edExportInterval->setValue(ss.exportIntervalSec());
        ui->edHostUrl->setText(svc->hostUrl());
        ui->edEventId->setText(svc->eventId());
        ui->edEventPassword->setText(svc->eventPassword());
	}

    connect(ui->btExportResultsXml30, &QPushButton::clicked, this, &OFeedClientWidget::onBtExportResultsXml30Clicked);
    connect(ui->btExportStartListXml30, &QPushButton::clicked, this, &OFeedClientWidget::onBtExportStartListXml30Clicked);
}

OFeedClientWidget::~OFeedClientWidget()
{
	delete ui;
}

bool OFeedClientWidget::acceptDialogDone(int result)
{
	if(result == QDialog::Accepted) {
		if(!saveSettings()) {
			return false;
		}
	}
	return true;
}

OFeedClient *OFeedClientWidget::service()
{
    OFeedClient *svc = qobject_cast<OFeedClient*>(Service::serviceByName(OFeedClient::serviceName()));
    QF_ASSERT(svc, OFeedClient::serviceName() + " doesn't exist", return nullptr);
	return svc;
}

bool OFeedClientWidget::saveSettings()
{
    OFeedClient *svc = service();
	if(svc) {
        OFeedClientSettings ss = svc->settings();
		ss.setExportIntervalSec(ui->edExportInterval->value());
        svc->setHostUrl(ui->edHostUrl->text().trimmed());
        svc->setEventId(ui->edEventId->text().trimmed());
        svc->setEventPassword(ui->edEventPassword->text().trimmed());
		svc->setSettings(ss);
	}
	return true;
}

void OFeedClientWidget::onBtExportResultsXml30Clicked()
{
    OFeedClient *svc = service();
	if(svc) {
		saveSettings();
		qfInfo() << "OFeed [results - manual upload]";
		svc->exportResultsIofXml3();
	}
}

void OFeedClientWidget::onBtExportStartListXml30Clicked()
{
    OFeedClient *svc = service();
	if(svc) {
		saveSettings();
		qfInfo() << "OFeed [startlist - manual upload]";
		svc->exportStartListIofXml3();
	}
}
}}

