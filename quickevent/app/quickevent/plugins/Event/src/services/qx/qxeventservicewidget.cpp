#include "qxeventservicewidget.h"
#include "ui_qxeventservicewidget.h"

#include "qxeventservice.h"

#include "../../eventplugin.h"

#include <qf/gui/framework/mainwindow.h>
#include <qf/gui/dialogs/filedialog.h>
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
#include <QSaveFile>

#include <algorithm>

using qf::gui::framework::getPlugin;

namespace Event::services::qx {

namespace {
constexpr auto QX_API_TOKEN_META_KEY = "qx_api_token";
// keys of the result of the SHV file node stat method
constexpr int FILE_STAT_SIZE = 1;
constexpr int FILE_STAT_MAX_WRITE = 5;
constexpr int FILE_STAT_MAX_READ = 6;
// server inflates and verifies the whole database
constexpr int PUSH_DB_COMMIT_TIMEOUT_MSEC = 60 * 1000;
// server copies, respectively hashes the whole database
constexpr int PULL_DB_SNAPSHOT_TIMEOUT_MSEC = 60 * 1000;
constexpr int PULL_DB_SHA1_TIMEOUT_MSEC = 60 * 1000;
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
	connect(ui->btPullEventDb, &QAbstractButton::clicked, this, &QxEventServiceWidget::pullEventDb);

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

struct QxEventServiceWidget::DbTransfer
{
	QPointer<shv::iotqt::rpc::DeviceConnection> rpc;
	std::string shvPath;
	shv::chainpack::RpcValue::Map metaData;
	int eventId = 0;
	// zlib compressed database being transferred
	QByteArray data;
	// SHA-1 of the uncompressed database to upload
	QByteArray sha1;
	qsizetype offset = 0;
	qsizetype size = 0;
	qsizetype chunkSize = 0;
};

std::shared_ptr<QxEventServiceWidget::DbTransfer> QxEventServiceWidget::createDbTransfer(const char *node_name)
{
	using namespace shv::chainpack;

	auto *svc = service();
	if (!svc) {
		return {};
	}
	QPointer rpc = svc->rpcConnection();
	if (!rpc || !rpc->isBrokerConnected()) {
		setMessage(tr("Not connected to broker, start the service first"), MessageType::Error);
		return {};
	}
	auto event_id = svc->eventId();
	if (event_id == 0) {
		setMessage(tr("Event not opened on the server, start the service first"), MessageType::Error);
		return {};
	}
	auto transfer = std::make_shared<DbTransfer>();
	transfer->rpc = rpc;
	transfer->eventId = event_id;
	transfer->shvPath = QStringLiteral("%1/%2/%3").arg(QxEventService::eventctlShvPath(svc->settings().isLocalBroker())).arg(event_id).arg(QString::fromLatin1(node_name)).toStdString();
	transfer->metaData = RpcValue::Map{{QX_API_TOKEN_META_KEY, svc->apiToken().toStdString()}};
	return transfer;
}

void QxEventServiceWidget::setDbTransferRunning(bool is_running)
{
	ui->btPushEventDb->setDisabled(is_running);
	ui->btPullEventDb->setDisabled(is_running);
	ui->btTestConnection->setDisabled(is_running);
}

void QxEventServiceWidget::pushEventDb()
{
	using namespace shv::chainpack;

	auto upload = createDbTransfer("pushDb");
	if (!upload) {
		return;
	}
	if (!qf::gui::dialogs::MessageBox::askYesNo(this, tr("Replace database of event %1 on the server with the local event data?").arg(upload->eventId), false)) {
		return;
	}

	setDbTransferRunning(true);
	setMessage(tr("Exporting event database"), MessageType::Progress);
	auto *fwk = qf::gui::framework::MainWindow::frameWork();
	QString err_str;
	const QByteArray event_qbe_data = getPlugin<EventPlugin>()->eventQbeData(&err_str, [fwk](const QString &msg, int completed, int total) {
		fwk->showProgress(msg, completed, total);
	});
	fwk->hideProgress();
	if (event_qbe_data.isEmpty()) {
		setMessage(tr("Event database export error: %1").arg(err_str), MessageType::Error);
		setDbTransferRunning(false);
		return;
	}

	// qCompress() prepends 4 bytes of uncompressed size to the zlib stream, the server expects the plain zlib stream
	upload->data = qCompress(event_qbe_data).mid(4);
	upload->sha1 = QCryptographicHash::hash(event_qbe_data, QCryptographicHash::Sha1);

	callDbFileMethod(upload, "stat", {}, [this, upload](const RpcValue &stat) {
		upload->chunkSize = stat.asIMap().value(FILE_STAT_MAX_WRITE).toInt();
		if (upload->chunkSize <= 0) {
			setMessage(tr("Event database push error, invalid max write size in stat result: %1").arg(upload->chunkSize), MessageType::Error);
			setDbTransferRunning(false);
			return;
		}
		// an unfinished previous upload might be left on the server
		callDbFileMethod(upload, "truncate", 0, [this, upload](const RpcValue &) {
			pushEventDbChunk(upload);
		});
	});
}

void QxEventServiceWidget::pushEventDbChunk(const std::shared_ptr<DbTransfer> &upload)
{
	using namespace shv::chainpack;

	if (upload->offset >= upload->data.size()) {
		setMessage(tr("Installing event database"), MessageType::Progress);
		RpcValue sha1(reinterpret_cast<const uint8_t*>(upload->sha1.constData()), static_cast<size_t>(upload->sha1.size()));
		callDbFileMethod(upload, "commit", sha1, [this](const RpcValue &) {
			setMessage(tr("Event database pushed OK"));
			setDbTransferRunning(false);
		}, PUSH_DB_COMMIT_TIMEOUT_MSEC);
		return;
	}

	setMessage(tr("Uploading event database %1 / %2 kB").arg(upload->offset / 1024).arg(upload->data.size() / 1024), MessageType::Progress);
	auto chunk_size = std::min(upload->chunkSize, upload->data.size() - upload->offset);
	RpcValue data(reinterpret_cast<const uint8_t*>(upload->data.constData() + upload->offset), static_cast<size_t>(chunk_size));
	RpcList params{RpcValue(upload->offset), data};
	upload->offset += chunk_size;
	callDbFileMethod(upload, "write", params, [this, upload](const RpcValue &) {
		pushEventDbChunk(upload);
	});
}

void QxEventServiceWidget::pullEventDb()
{
	using namespace shv::chainpack;

	auto download = createDbTransfer("pullDb");
	if (!download) {
		return;
	}
	setDbTransferRunning(true);
	setMessage(tr("Creating event database snapshot"), MessageType::Progress);
	callDbFileMethod(download, "snapshot", {}, [this, download](const RpcValue &) {
		callDbFileMethod(download, "stat", {}, [this, download](const RpcValue &stat) {
			const auto &stat_map = stat.asIMap();
			download->size = stat_map.value(FILE_STAT_SIZE).toInt64();
			download->chunkSize = stat_map.value(FILE_STAT_MAX_READ).toInt64();
			if (download->size <= 0 || download->chunkSize <= 0) {
				setMessage(tr("Event database pull error, invalid stat result: %1").arg(QString::fromStdString(stat.toCpon())), MessageType::Error);
				setDbTransferRunning(false);
				return;
			}
			download->data.reserve(download->size);
			pullEventDbChunk(download);
		});
	}, PULL_DB_SNAPSHOT_TIMEOUT_MSEC);
}

void QxEventServiceWidget::pullEventDbChunk(const std::shared_ptr<DbTransfer> &download)
{
	using namespace shv::chainpack;

	if (download->offset >= download->size) {
		verifyPulledEventDb(download);
		return;
	}

	setMessage(tr("Downloading event database %1 / %2 kB").arg(download->offset / 1024).arg(download->size / 1024), MessageType::Progress);
	auto chunk_size = std::min(download->chunkSize, download->size - download->offset);
	RpcList params{RpcValue(download->offset), RpcValue(chunk_size)};
	callDbFileMethod(download, "read", params, [this, download](const RpcValue &result) {
		const auto &chunk = result.asBlob();
		// the server can return less data than requested, but no data means that the file got shorter
		if (chunk.empty()) {
			setMessage(tr("Event database pull error, unexpected end of data at offset %1 of %2").arg(download->offset).arg(download->size), MessageType::Error);
			setDbTransferRunning(false);
			return;
		}
		download->data.append(reinterpret_cast<const char*>(chunk.data()), static_cast<qsizetype>(chunk.size()));
		download->offset = download->data.size();
		pullEventDbChunk(download);
	});
}

void QxEventServiceWidget::verifyPulledEventDb(const std::shared_ptr<DbTransfer> &download)
{
	using namespace shv::chainpack;

	setMessage(tr("Verifying event database"), MessageType::Progress);
	callDbFileMethod(download, "sha1", {}, [this, download](const RpcValue &result) {
		const auto &server_sha1 = result.asBlob();
		const auto sha1 = QCryptographicHash::hash(download->data, QCryptographicHash::Sha1);
		// the snapshot can be replaced during the download by another client pulling the same event
		if (sha1 != QByteArray(reinterpret_cast<const char*>(server_sha1.data()), static_cast<qsizetype>(server_sha1.size()))) {
			setMessage(tr("Event database pull error, downloaded data does not match the server SHA-1"), MessageType::Error);
			setDbTransferRunning(false);
			return;
		}
		setDbTransferRunning(false);
		// qUncompress() expects 4 bytes of uncompressed size before the zlib stream, the server sends the plain zlib stream.
		// Zero size is just a hint, the output buffer grows as needed.
		download->data = qUncompress(QByteArray(4, '\0') + download->data);
		if (download->data.isEmpty()) {
			setMessage(tr("Event database pull error, cannot decompress downloaded data"), MessageType::Error);
			return;
		}
		savePulledEventDb(download);
	}, PULL_DB_SHA1_TIMEOUT_MSEC);
}

void QxEventServiceWidget::savePulledEventDb(const std::shared_ptr<DbTransfer> &download)
{
	setMessage(tr("Event database pulled OK"));
	const QString ext = QStringLiteral(".qbe");
	QString file_name = qf::gui::dialogs::FileDialog::getSaveFileName(this, tr("Save event database as"), QStringLiteral("event-%1%2").arg(download->eventId).arg(ext), tr("Quick Event files *%1 (*%1)").arg(ext));
	if (file_name.isEmpty()) {
		setMessage();
		return;
	}
	if (!file_name.endsWith(ext, Qt::CaseInsensitive)) {
		file_name += ext;
	}
	QSaveFile f(file_name);
	if (f.open(QFile::WriteOnly) && f.write(download->data) == download->data.size() && f.commit()) {
		setMessage(tr("Event database saved to %1").arg(file_name));
	}
	else {
		setMessage(tr("Cannot write file %1: %2").arg(file_name, f.errorString()), MessageType::Error);
	}
}

void QxEventServiceWidget::callDbFileMethod(const std::shared_ptr<DbTransfer> &transfer, const char *method, const shv::chainpack::RpcValue &param, const std::function<void (const shv::chainpack::RpcValue &)> &on_success, int timeout_msec)
{
	auto *rpc_call = shv::iotqt::rpc::RpcCall::create(transfer->rpc)
			->setShvPath(transfer->shvPath)
			->setMethod(method)
			->setParams(param)
			->setTimeout(timeout_msec)
			// methods of this node are flagged UserIDRequired, the broker fills the ID in
			->setUserId(std::string{})
			->setExtraMetaData(transfer->metaData);
	connect(rpc_call, &shv::iotqt::rpc::RpcCall::maybeResult, this, [this, method, on_success](const ::shv::chainpack::RpcValue &result, const shv::chainpack::RpcError &error) {
		if (error.isValid()) {
			setMessage(tr("Event database transfer error, method %1: %2").arg(method).arg(error.toString()), MessageType::Error);
			setDbTransferRunning(false);
		}
		else {
			on_success(result);
		}
	});
	rpc_call->start();
}

}
