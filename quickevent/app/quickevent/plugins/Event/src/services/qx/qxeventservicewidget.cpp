#include "qxeventservicewidget.h"
#include "ui_qxeventservicewidget.h"

#include "qxeventservice.h"

#include <plugins/Event/src/eventplugin.h>

#include <qf/gui/framework/mainwindow.h>
#include <qf/gui/dialogs/messagebox.h>
#include <qf/core/assert.h>

#include <shv/iotqt/rpc/deviceconnection.h>
#include <shv/iotqt/rpc/rpccall.h>
#include <shv/coreqt/rpc.h>

#include <QFileDialog>
#include <QUrlQuery>
#include <QClipboard>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>

using qf::gui::framework::getPlugin;

namespace Event::services::qx {

QxEventServiceWidget::QxEventServiceWidget(QWidget *parent)
	: Super(parent)
	, ui(new Ui::QxEventServiceWidget)
{
	setPersistentSettingsId("QxEventServiceWidget");
	ui->setupUi(this);
	connect(ui->edServerUrl, &QLineEdit::textChanged, this, &QxEventServiceWidget::updateOCheckListPostUrl);
	connect(ui->edApiToken, &QLineEdit::textChanged, this, &QxEventServiceWidget::updateOCheckListPostUrl);

	setMessage("");

	auto *svc = service();
	Q_ASSERT(svc);
	auto *event_plugin = getPlugin<EventPlugin>();
	auto current_stage = event_plugin->currentStageId();
	auto settings = svc->settings();
	ui->edServerUrl->setText(settings.shvBrokerUrl());
	ui->edApiToken->setText(svc->apiToken());
	ui->edCurrentStage->setValue(current_stage);
	ui->edEventId->setValue(svc->eventId());
	connect(ui->btTestConnection, &QAbstractButton::clicked, this, &QxEventServiceWidget::testConnection);
	connect(ui->btExportEventInfo, &QAbstractButton::clicked, this, &QxEventServiceWidget::exportEventInfo);
	connect(ui->btExportStartList, &QAbstractButton::clicked, this, &QxEventServiceWidget::exportStartList);
	connect(ui->btExportRuns, &QAbstractButton::clicked, this, &QxEventServiceWidget::exportRuns);
}

QxEventServiceWidget::~QxEventServiceWidget()
{
	delete ui;
}

void QxEventServiceWidget::setMessage(const QString &msg, MessageType msg_type)
{
	if (msg.isEmpty()) {
		ui->lblStatus->setStyleSheet({});
	}
	else {
		switch (msg_type) {
		case MessageType::Ok:
		ui->lblStatus->setStyleSheet("background: lightgreen");
		break;
		case MessageType::Error:
		ui->lblStatus->setStyleSheet("background: salmon");
		break;
		case MessageType::Progress:
		ui->lblStatus->setStyleSheet("background: orange");
		break;
		}
	}
	ui->lblStatus->setText(msg);
}

bool QxEventServiceWidget::acceptDialogDone(int result)
{
	if(result == QDialog::Accepted) {
		if(!saveSettings()) {
			return false;
		}
	}
	return true;
}

QxEventService *QxEventServiceWidget::service()
{
	auto *svc = qobject_cast<QxEventService*>(Service::serviceByName(QxEventService::serviceId()));
	QF_ASSERT(svc, QxEventService::serviceId() + " doesn't exist", return nullptr);
	return svc;
}

bool QxEventServiceWidget::saveSettings()
{
	auto *svc = service();
	if(svc) {
		auto ss = svc->settings();
		ss.setShvBrokerUrl(ui->edServerUrl->text());
		svc->setSettings(ss);
		auto *event_plugin = getPlugin<EventPlugin>();

		auto current_stage = event_plugin->currentStageId();
		auto stage_data = event_plugin->stageData(current_stage);
		stage_data.setQxApiToken(ui->edApiToken->text());
		event_plugin->setStageData(current_stage, stage_data);
	}
	return true;
}

void QxEventServiceWidget::updateOCheckListPostUrl()
{
	auto url = QStringLiteral("%1/api/event/current/oc").arg(ui->edServerUrl->text());
	ui->edOChecklistUrl->setText(url);
	ui->edOChecklistUrlHeader->setText(QStringLiteral("qx-api-token=%1").arg(ui->edApiToken->text()));
}

void QxEventServiceWidget::testConnection()
{
	using namespace shv::iotqt::rpc;
	using namespace shv::chainpack;

	delete findChild<DeviceConnection*>();

	auto *rpc = new DeviceConnection("QuickEventTest", this);
	rpc->setConnectionString(ui->edServerUrl->text());
	RpcValue::Map opts;
	RpcValue::Map device;
	device["deviceId"] = ui->edApiToken->text().toStdString();
	opts["device"] = device;
	rpc->setConnectionOptions(opts);

	connect(rpc, &ClientConnection::brokerConnectedChanged, this, [this, rpc](bool is_connected) {
		if (is_connected) {
			setMessage(tr("Broker connected OK"));
			auto *rpc_call = shv::iotqt::rpc::RpcCall::create(rpc)
					->setShvPath(".broker/currentClient")
					->setMethod("info");
			connect(rpc_call, &shv::iotqt::rpc::RpcCall::maybeResult, this, [this](const ::shv::chainpack::RpcValue &result, const shv::chainpack::RpcError &error) {
				if (error.isValid()) {
					setMessage(tr("Client info discovery error: %1").arg(error.toString()), MessageType::Error);
				}
				else {
					const auto &info = result.asMap();
					auto mount_point = info.value("mountPoint").to<QString>();
					auto event_id = mount_point.section('/', -1, -1).toInt();
					ui->edEventId->setValue(event_id);
					setMessage(tr("Event mounted at: %1, event id: %2").arg(mount_point).arg(event_id));
				}
			});
			rpc_call->start();
		}
	});
	connect(rpc, &ClientConnection::socketError, this, [this](const QString &error) {
		setMessage(tr("Connection error: %1").arg(error), MessageType::Error);
	});
	connect(rpc, &ClientConnection::brokerLoginError, this, [this](const auto &error) {
		setMessage(tr("Login error: %1").arg(QString::fromStdString(error.toString())), MessageType::Error);
	});
	rpc->open();
}

void QxEventServiceWidget::exportEventInfo()
{
	auto *svc = service();
	Q_ASSERT(svc);
	auto *reply = svc->postEventInfo(ui->edServerUrl->text(), ui->edApiToken->text());
	connect(reply, &QNetworkReply::finished, this, [this, reply]() {
		auto data = reply->readAll();
		if (reply->error() == QNetworkReply::NetworkError::NoError) {
			auto doc = QJsonDocument::fromJson(data);
			EventInfo event_info(doc.toVariant().toMap());
			ui->edEventId->setValue(event_info.id());
			setMessage(tr("Event info updated OK"));
		}
		else {
			setMessage(tr("Event info update error: %1\n%2").arg(reply->errorString()).arg(QString::fromUtf8(data)), MessageType::Error);
		}
		reply->deleteLater();
	});
}

void QxEventServiceWidget::exportStartList()
{
	auto *svc = service();
	Q_ASSERT(svc);
	saveSettings();
	setMessage(tr("Start list export started ..."), MessageType::Progress);
	svc->postStartListIofXml3(this, [this](auto err) {
		if (err.isEmpty()) {
			setMessage(tr("Start list exported Ok"));
		}
		else {
			setMessage(err, MessageType::Error);
		}
	});
}

void QxEventServiceWidget::exportRuns()
{
	auto *svc = service();
	Q_ASSERT(svc);
	saveSettings();
	setMessage(tr("Runs export started ..."), MessageType::Progress);
	svc->postRuns(this, [this](auto err) {
		if (err.isEmpty()) {
			setMessage(tr("Runs exported Ok"));
		}
		else {
			setMessage(err, MessageType::Error);
		}
	});
}

}

