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
#include <QCryptographicHash>
#include <QPointer>

#include <algorithm>

using qf::gui::framework::getPlugin;

namespace Event::services::qx {

namespace {
constexpr auto QX_API_TOKEN_META_KEY = "qx_api_token";
// key of the maximal write size in the result of the SHV file node stat method
constexpr int FILE_STAT_MAX_WRITE = 5;
// server inflates and verifies the whole database
constexpr int PUSH_DB_COMMIT_TIMEOUT_MSEC = 60 * 1000;
}

QxEventServiceWidget::QxEventServiceWidget(QWidget *parent)
	: Super(parent)
	, ui(new Ui::QxEventServiceWidget)
{
	setPersistentSettingsId("QxEventServiceWidget");
	ui->setupUi(this);

	setMessage("");

	auto *svc = service();
	Q_ASSERT(svc);
	auto settings = svc->settings();
	ui->edServerUrl->setText(settings.shvBrokerUrl());
	ui->edApiToken->setText(svc->apiToken());
	ui->edEventId->setValue(svc->eventId());
	connect(ui->btTestConnection, &QAbstractButton::clicked, this, &QxEventServiceWidget::testConnection);

	connect(ui->btPushEventDb, &QAbstractButton::clicked, this, &QxEventServiceWidget::pushEventDb);

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
	{
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
			connect(rpc_call, &shv::iotqt::rpc::RpcCall::maybeResult, this, [this, rpc](const ::shv::chainpack::RpcValue &result, const shv::chainpack::RpcError &error) {
				if (error.isValid()) {
					setMessage(tr("Client info discovery error: %1").arg(error.toString()), MessageType::Error);
				}
				else {
					const auto &info = result.asMap();
					auto mount_point = info.value("mountPoint").to<QString>();
					auto event_id = mount_point.section('/', -1, -1).toInt();
					ui->edEventId->setValue(event_id);
					setMessage(tr("Event mounted at: %1, event id: %2").arg(mount_point).arg(event_id));
					bool is_localhost = ui->btLocalhost->isChecked();
					auto *rpc_call2 = shv::iotqt::rpc::RpcCall::create(rpc)
							->setShvPath(QxEventService::eventctlShvPath(is_localhost).toStdString())
							->setMethod("eventData")
							->setParams(RpcValue::Map{{"event_id", event_id}});
					connect(rpc_call2, &shv::iotqt::rpc::RpcCall::maybeResult, this, [this](const ::shv::chainpack::RpcValue &result, const shv::chainpack::RpcError &error) {
						if (error.isValid()) {
							setMessage(tr("Client DB mode discovery error: %1").arg(error.toString()), MessageType::Error);
						}
						else {
							const auto &info = result.asMap();
							auto db_mode = info.value("db_mode").to<QString>();
							ui->edDbMode->setText(db_mode);
						}
					});
					rpc_call2->start();
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

struct QxEventServiceWidget::PushDbUpload
{
	QPointer<shv::iotqt::rpc::DeviceConnection> rpc;
	std::string shvPath;
	shv::chainpack::RpcValue::Map metaData;
	QByteArray zdata;
	QByteArray sha1;
	qsizetype offset = 0;
	qsizetype chunkSize = 0;
};

void QxEventServiceWidget::setPushDbRunning(bool is_running)
{
	ui->btPushEventDb->setDisabled(is_running);
	ui->btTestConnection->setDisabled(is_running);
}

void QxEventServiceWidget::pushEventDb()
{
	using namespace shv::chainpack;

	auto *svc = service();
	if (!svc) {
		return;
	}
	QPointer rpc = svc->rpcConnection();
	if (!rpc || !rpc->isBrokerConnected()) {
		setMessage(tr("Not connected to broker, start the service first"), MessageType::Error);
		return;
	}
	auto event_id = svc->eventId();
	if (event_id == 0) {
		setMessage(tr("Event not opened on the server, start the service first"), MessageType::Error);
		return;
	}
	if (!qf::gui::dialogs::MessageBox::askYesNo(this, tr("Replace database of event %1 on the server with the local event data?").arg(event_id), false)) {
		return;
	}

	setPushDbRunning(true);
	setMessage(tr("Exporting event database"), MessageType::Progress);
	auto *fwk = qf::gui::framework::MainWindow::frameWork();
	QString err_str;
	const QByteArray event_qbe_data = getPlugin<EventPlugin>()->eventQbeData(&err_str, [fwk](const QString &msg, int completed, int total) {
		fwk->showProgress(msg, completed, total);
	});
	fwk->hideProgress();
	if (event_qbe_data.isEmpty()) {
		setMessage(tr("Event database export error: %1").arg(err_str), MessageType::Error);
		setPushDbRunning(false);
		return;
	}

	auto upload = std::make_shared<PushDbUpload>();
	upload->rpc = rpc;
	upload->shvPath = QStringLiteral("%1/%2/pushDb").arg(QxEventService::eventctlShvPath(svc->settings().isLocalBroker())).arg(event_id).toStdString();
	upload->metaData = RpcValue::Map{{QX_API_TOKEN_META_KEY, svc->apiToken().toStdString()}};
	// qCompress() prepends 4 bytes of uncompressed size to the zlib stream, the server expects the plain zlib stream
	upload->zdata = qCompress(event_qbe_data).mid(4);
	upload->sha1 = QCryptographicHash::hash(event_qbe_data, QCryptographicHash::Sha1);

	callPushDbMethod(upload, "stat", {}, [this, upload](const RpcValue &stat) {
		upload->chunkSize = stat.asIMap().value(FILE_STAT_MAX_WRITE).toInt();
		if (upload->chunkSize <= 0) {
			setMessage(tr("Event database push error, invalid max write size in stat result: %1").arg(upload->chunkSize), MessageType::Error);
			setPushDbRunning(false);
			return;
		}
		// an unfinished previous upload might be left on the server
		callPushDbMethod(upload, "truncate", 0, [this, upload](const RpcValue &) {
			pushEventDbChunk(upload);
		});
	});
}

void QxEventServiceWidget::pushEventDbChunk(const std::shared_ptr<PushDbUpload> &upload)
{
	using namespace shv::chainpack;

	if (upload->offset >= upload->zdata.size()) {
		setMessage(tr("Installing event database"), MessageType::Progress);
		RpcValue sha1(reinterpret_cast<const uint8_t*>(upload->sha1.constData()), static_cast<size_t>(upload->sha1.size()));
		callPushDbMethod(upload, "commit", sha1, [this](const RpcValue &) {
			setMessage(tr("Event database pushed OK"));
			setPushDbRunning(false);
		}, PUSH_DB_COMMIT_TIMEOUT_MSEC);
		return;
	}

	setMessage(tr("Uploading event database %1 / %2 kB").arg(upload->offset / 1024).arg(upload->zdata.size() / 1024), MessageType::Progress);
	auto chunk_size = std::min(upload->chunkSize, upload->zdata.size() - upload->offset);
	RpcValue data(reinterpret_cast<const uint8_t*>(upload->zdata.constData() + upload->offset), static_cast<size_t>(chunk_size));
	RpcList params{RpcValue(upload->offset), data};
	upload->offset += chunk_size;
	callPushDbMethod(upload, "write", params, [this, upload](const RpcValue &) {
		pushEventDbChunk(upload);
	});
}

void QxEventServiceWidget::callPushDbMethod(const std::shared_ptr<PushDbUpload> &upload, const char *method, const shv::chainpack::RpcValue &param, const std::function<void (const shv::chainpack::RpcValue &)> &on_success, int timeout_msec)
{
	auto *rpc_call = shv::iotqt::rpc::RpcCall::create(upload->rpc)
			->setShvPath(upload->shvPath)
			->setMethod(method)
			->setParams(param)
			->setTimeout(timeout_msec)
			// methods of this node are flagged UserIDRequired, the broker fills the ID in
			->setUserId(std::string{})
			->setExtraMetaData(upload->metaData);
	connect(rpc_call, &shv::iotqt::rpc::RpcCall::maybeResult, this, [this, method, on_success](const ::shv::chainpack::RpcValue &result, const shv::chainpack::RpcError &error) {
		if (error.isValid()) {
			setMessage(tr("Event database push error, method %1: %2").arg(method).arg(error.toString()), MessageType::Error);
			setPushDbRunning(false);
		}
		else {
			on_success(result);
		}
	});
	rpc_call->start();
}

}
