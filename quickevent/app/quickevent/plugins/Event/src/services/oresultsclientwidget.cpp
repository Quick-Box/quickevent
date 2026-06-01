#include "oresultsclientwidget.h"
#include "ui_oresultsclientwidget.h"
#include "oresultsclient.h"
#include <qf/gui/framework/mainwindow.h>

#include <qf/gui/dialogs/messagebox.h>

#include <qf/core/assert.h>

#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>

#include <qf/core/log.h>

namespace Event::services {

OResultsClientWidget::OResultsClientWidget(QWidget *parent)
	: Super(parent)
	, ui(new Ui::OResultsClientWidget)
{
	setPersistentSettingsId("OResultsClientWidget");
	ui->setupUi(this);

	OResultsClient *svc = service();
	if(svc) {
		OResultsClientSettings ss = svc->settings();
		ui->edExportInterval->setValue(ss.exportIntervalSec());

		m_loadedApiKey = svc->apiKey();
		ui->edApiKey->setText(m_loadedApiKey);

		updateTestStatus(svc->eventName());
	}

	connect(ui->btExportResultsXml30, &QPushButton::clicked, this, &OResultsClientWidget::onBtExportResultsXml30Clicked);
	connect(ui->btExportStartListXml30, &QPushButton::clicked, this, &OResultsClientWidget::onBtExportStartListXml30Clicked);
	connect(ui->btTestApiKey, &QPushButton::clicked, this, &OResultsClientWidget::onBtTestApiKeyClicked);

	// connect textChanged AFTER loading initial values to avoid spurious clears
	connect(ui->edApiKey, &QLineEdit::textChanged, this, [this](const QString &text) {
		OResultsClient *svc = service();
		if (!svc) return;
		updateTestStatus(text.trimmed() == m_loadedApiKey ? svc->eventName() : QString{});
	});
}

OResultsClientWidget::~OResultsClientWidget()
{
	delete ui;
}

bool OResultsClientWidget::acceptDialogDone(int result)
{
	if (result == QDialog::Accepted) {
		if (m_testRunning)
			return false;
		QString key = ui->edApiKey->text().trimmed();
		if (!key.isEmpty() && !m_testPassed) {
			runTest(key, [this](bool ok) {
				if (ok) {
					saveSettings();
					if (auto *dlg = qobject_cast<QDialog*>(window()))
						dlg->accept();
				}
			});
			return false;
		}
		if (!saveSettings())
			return false;
	}
	return true;
}

OResultsClient *OResultsClientWidget::service()
{
	auto *svc = qobject_cast<OResultsClient*>(Service::serviceByName(OResultsClient::serviceName()));
	QF_ASSERT(svc, OResultsClient::serviceName() + " doesn't exist", return nullptr);
	return svc;
}

bool OResultsClientWidget::saveSettings()
{
	OResultsClient *svc = service();
	if(svc) {
		OResultsClientSettings ss = svc->settings();
		ss.setExportIntervalSec(ui->edExportInterval->value());
		if (!m_testPassed)
			svc->setEventName({});
		svc->setApiKey(ui->edApiKey->text().trimmed());
		svc->setSettings(ss);
	}
	return true;
}

void OResultsClientWidget::runTest(const QString &key, std::function<void(bool)> onDone)
{
	ui->lblApiKeyStatus->setText(tr("Testing..."));
	ui->btTestApiKey->setEnabled(false);
	m_testRunning = true;
	OResultsClient *svc = service();
	if (!svc) {
		m_testRunning = false;
		onDone(false);
		return;
	}
	auto *reply = svc->getEventInfo(key);
	connect(reply, &QNetworkReply::finished, this, [this, reply, key, svc, onDone]() {
		ui->btTestApiKey->setEnabled(true);
		m_testRunning = false;
		bool ok = false;
		if (reply->error() == QNetworkReply::NoError) {
			auto doc = QJsonDocument::fromJson(reply->readAll());
			QString name = doc.object().value("name").toString();
			if (!name.isEmpty()) {
				updateTestStatus(name);
				svc->setEventName(name);
				m_loadedApiKey = key;
				ok = true;
			} else {
				qfError() << "OResults: API key lookup returned empty name for key" << key;
				ui->lblApiKeyStatus->setText(tr("Not found"));
				m_testPassed = false;
			}
		} else {
			int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
			if (httpStatus == 404) {
				qfError() << "OResults: event not found for key" << key;
				ui->lblApiKeyStatus->setText(tr("Not found"));
			} else {
				qfError() << "OResults: connection test failed:" << reply->errorString();
				ui->lblApiKeyStatus->setText(tr("Error"));
			}
			m_testPassed = false;
		}
		reply->deleteLater();
		onDone(ok);
	});
}

void OResultsClientWidget::updateTestStatus(const QString &name)
{
	if (!name.isEmpty()) {
		ui->lblApiKeyStatus->setText(name);
		m_testPassed = true;
	} else {
		ui->lblApiKeyStatus->setText(tr("Not verified — click 'Test connection'"));
		m_testPassed = false;
	}
}

void OResultsClientWidget::onBtTestApiKeyClicked()
{
	QString key = ui->edApiKey->text().trimmed();
	if (key.isEmpty() || m_testRunning)
		return;
	runTest(key, [](bool) {});
}

void OResultsClientWidget::onBtExportResultsXml30Clicked()
{
	OResultsClient *svc = service();
	if(svc) {
		saveSettings();
		svc->exportResultsIofXml3();
	}
}

void OResultsClientWidget::onBtExportStartListXml30Clicked()
{
	OResultsClient *svc = service();
	if(svc) {
		saveSettings();
		svc->exportStartListIofXml3();
	}
}
}
