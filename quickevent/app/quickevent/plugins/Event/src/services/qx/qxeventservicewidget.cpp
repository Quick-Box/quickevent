#include "qxeventservicewidget.h"
#include "ui_qxeventservicewidget.h"

#include "qxeventservice.h"

#include "../../eventplugin.h"

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

	setMessage("");

	auto *svc = service();
	Q_ASSERT(svc);
	auto *event_plugin = getPlugin<EventPlugin>();
	auto current_stage = event_plugin->currentStageId();
	auto settings = svc->settings();
	ui->edServerUrl->setText(settings.shvBrokerUrl());
	ui->chkExportDatabase->setChecked(settings.isExportDatabase());
	ui->edApiToken->setText(svc->apiToken());
	ui->edCurrentStage->setValue(current_stage);
	ui->edEventId->setValue(svc->eventId());
	connect(ui->btTestConnection, &QAbstractButton::clicked, this, &QxEventServiceWidget::testConnection);

	connect(ui->btQxQxOrg, &QRadioButton::toggled, this, &QxEventServiceWidget::setConnectionType);
	connect(ui->btLocalhost, &QRadioButton::toggled, this, &QxEventServiceWidget::setConnectionType);
	ui->btQxQxOrg->setChecked(!settings.isLocalBroker());
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
	// delete testing connection if any
	delete findChild<shv::iotqt::rpc::DeviceConnection*>();
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
		bool is_localhost = ui->btLocalhost->isChecked();
		if (is_localhost) {
			ss.setShvBrokerUrl(ui->edServerUrl->text());
		}
		ss.setLocalBroker(is_localhost);
		ss.setExportDatabase(ui->chkExportDatabase->isChecked());
		svc->setSettings(ss);

		auto &app_config = getPlugin<EventPlugin>()->appDbConfig();
		auto qx_config = app_config.qxConfig();
		qx_config.apiToken = ui->edApiToken->text().trimmed();
		app_config.setQxConfig(qx_config);
	}
	return true;
}

void QxEventServiceWidget::testConnection()
{
	using namespace shv::iotqt::rpc;
	using namespace shv::chainpack;

	delete findChild<DeviceConnection*>();

	auto *rpc = new DeviceConnection("QuickEventTest", this);
	rpc->setConnectionString(ui->edServerUrl->text());
	if (ui->chkExportDatabase->isChecked()) {
		RpcValue::Map opts;
		RpcValue::Map device;
		device["deviceId"] = ui->edApiToken->text().toStdString();
		opts["device"] = device;
		rpc->setConnectionOptions(opts);
	}

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

void QxEventServiceWidget::setConnectionType()
{
	bool is_localhost = ui->btLocalhost->isChecked();
	if (is_localhost) {
		ui->lblUrl->setVisible(true);
		ui->edServerUrl->setVisible(true);
	} else {
		ui->lblUrl->setVisible(false);
		ui->edServerUrl->setVisible(false);
	}
}

QString QxEventServiceWidget::brokerUrl() const
{
	bool is_localhost = ui->btLocalhost->isChecked();
	if (is_localhost) {
		return ui->edServerUrl->text();
	}
	auto url = QStringLiteral("tcp://%1?user=qe&%2=%3");
	url = url.arg("qxqx.org");
	url = url.arg("password");
	url = url.arg("2ddf6394ea6");
	return url;
}

}
