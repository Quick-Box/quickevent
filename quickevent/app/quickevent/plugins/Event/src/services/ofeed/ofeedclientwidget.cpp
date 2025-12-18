#include "ofeedclientwidget.h"
#include "ui_ofeedclientwidget.h"
#include "ofeedclient.h"

#include <qf/gui/framework/mainwindow.h>
#include <qf/gui/dialogs/messagebox.h>

#include <qf/core/assert.h>
#include <qf/core/log.h>

#include <QFileDialog>

#include <plugins/Event/src/eventplugin.h>

namespace Event::services {

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
		ui->edChangelogOrigin->setText(svc->changelogOrigin());
		ui->additionalSettingsRunXmlValidation->setChecked(svc->runXmlValidation());
		ui->processChangesOnOffButton->setText(svc->runChangesProcessing() ? tr("ON") : tr("OFF"));
		ui->processChangesOnOffButton->setChecked(svc->runChangesProcessing());
		ui->processChangesOnOffButton->setStyleSheet(
		"QPushButton {"
		"  padding: 5px;"
		"  border-radius: 4px;"
		"  border: 2px solid gray;"
		"}"
		"QPushButton:checked {"
		"  border: 2px solid green;"
		"  color: green;"
		"}"
		"QPushButton:!checked {"
		"  border: 2px solid red;"
		"  color: red;"
		"}"
		);
		ui->processChangesOnOffLabel->setText(svc->runChangesProcessing() ? tr("Changes are automatically processed") : tr("Processing changes is deactivated"));
	}

	connect(ui->btExportResultsXml30, &QPushButton::clicked, this, &OFeedClientWidget::onBtExportResultsXml30Clicked);
	connect(ui->btExportStartListXml30, &QPushButton::clicked, this, &OFeedClientWidget::onBtExportStartListXml30Clicked);
	connect(ui->processChangesOnOffButton, &QPushButton::clicked,this, &OFeedClientWidget::onProcessChangesOnOffButtonClicked);
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
	auto *svc = qobject_cast<OFeedClient*>(Service::serviceByName(OFeedClient::serviceName()));
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
		svc->setChangelogOrigin(ui->edChangelogOrigin->text().trimmed());
		svc->setRunXmlValidation(ui->additionalSettingsRunXmlValidation->isChecked());
		svc->setSettings(ss);
	}
	return true;
}

void OFeedClientWidget::onBtExportResultsXml30Clicked()
{
	OFeedClient *svc = service();
	if(svc) {
		saveSettings();
		qfInfo() << OFeedClient::serviceName() + " [results - manual upload]";
		svc->exportResultsIofXml3();
	}
}

void OFeedClientWidget::onBtExportStartListXml30Clicked()
{
	OFeedClient *svc = service();
	if(svc) {
		saveSettings();
		qfInfo() << OFeedClient::serviceName() + " [startlist - manual upload]";
		svc->exportStartListIofXml3();
	}
}

void OFeedClientWidget::onProcessChangesOnOffButtonClicked()
{
	OFeedClient *svc = service();
    if (!svc)
        return;

    bool newState = !svc->runChangesProcessing();
    svc->setRunChangesProcessing(newState);

    // Update button text or icon
    ui->processChangesOnOffButton->setText(newState ? tr("ON") : tr("OFF"));
	ui->processChangesOnOffButton->setChecked(svc->runChangesProcessing());
	ui->processChangesOnOffLabel->setText(svc->runChangesProcessing() ? tr("Changes are automatically processed") : tr("Processing changes is deactivated"));
}
}