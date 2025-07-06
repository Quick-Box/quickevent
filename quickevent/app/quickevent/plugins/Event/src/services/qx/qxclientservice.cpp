#include "qxclientservice.h"
#include "qxclientservicewidget.h"

#include "../../eventplugin.h"
#include "../../../../Runs/src/runsplugin.h"

#include <qf/qmlwidgets/framework/mainwindow.h>
#include <qf/core/log.h>
#include <qf/core/sql/query.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QHttpPart>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonArray>
#include <QUrlQuery>

using namespace qf::core;
using namespace qf::qmlwidgets;
using namespace qf::qmlwidgets::dialogs;
using namespace qf::core::sql;
using qf::qmlwidgets::framework::getPlugin;
using Event::EventPlugin;
using Runs::RunsPlugin;

namespace Event::services::qx {
//===============================================
// QxClientServiceSettings
//===============================================
// QString QxClientServiceSettings::eventKey() const
// {
// 	auto *event_plugin = getPlugin<EventPlugin>();
// 	auto *cfg = event_plugin->eventConfig();
// 	auto key = cfg->apiKey();
// 	auto current_stage = cfg->currentStageId();
// 	return QStringLiteral("%1%2").arg(key).arg(current_stage);
// }

//===============================================
// QxClientService
//===============================================
QxClientService::QxClientService(QObject *parent)
	: Super(QxClientService::serviceId(), parent)
{
	auto *event_plugin = getPlugin<EventPlugin>();
	connect(event_plugin, &Event::EventPlugin::dbEventNotify, this, &QxClientService::onDbEventNotify, Qt::QueuedConnection);
}

QString QxClientService::serviceDisplayName() const
{
	return tr("QE Exchange");
}

QString QxClientService::serviceId()
{
	return QStringLiteral("qx");
}

void QxClientService::run() {
	auto ss = settings();
	auto *reply = getRemoteEventInfo(ss.exchangeServerUrl(), apiToken());
	connect(reply, &QNetworkReply::finished, this, [this, reply, ss]() {
		if (reply->error() == QNetworkReply::NetworkError::NoError) {
			auto data = reply->readAll();
			auto doc = QJsonDocument::fromJson(data);
			EventInfo event_info(doc.toVariant().toMap());
			setStatusMessage(event_info.name() + (event_info.stage_count() > 1? QStringLiteral(" E%1").arg(event_info.stage()): QString()));
			m_eventId = event_info.id();
			connectToSSE(m_eventId);
			if (!m_pollChangesTimer) {
				m_pollChangesTimer = new QTimer(this);
				connect(m_pollChangesTimer, &QTimer::timeout, this, &QxClientService::pollQxChanges);
			}
			pollQxChanges();
			m_pollChangesTimer->start(10000);
			Super::run();
		}
		else {
			qfWarning() << "Cannot run QX service, network error:" << reply->errorString();
		}
	});
}

void QxClientService::stop()
{
	disconnectSSE();
	if (m_pollChangesTimer) {
		m_pollChangesTimer->stop();
	}
	Super::stop();
}

qf::qmlwidgets::framework::DialogWidget *QxClientService::createDetailWidget()
{
	auto *w = new QxClientServiceWidget();
	return w;
}

void QxClientService::loadSettings()
{
	Super::loadSettings();
	auto ss = settings();
	if (ss.exchangeServerUrl().isEmpty()) {
		ss.setExchangeServerUrl("http://localhost:8000");
	}
	m_settings = ss;
}

void QxClientService::onDbEventNotify(const QString &domain, int connection_id, const QVariant &data)
{
	Q_UNUSED(connection_id)
	Q_UNUSED(data)
	if (!isRunning()) {
		return;
	}
	if(domain == QLatin1String(Event::EventPlugin::DBEVENT_CARD_PROCESSED_AND_ASSIGNED)) {
		//auto checked_card = quickevent::core::si::CheckedCard(data.toMap());
		//int competitor_id = getPlugin<RunsPlugin>()->competitorForRun(checked_card.runId());
		//onCompetitorChanged(competitor_id);
	}
	else if(domain == QLatin1String(Event::EventPlugin::DBEVENT_COMPETITOR_EDITED)) {
		//int competitor_id = data.toInt();
		//onCompetitorChanged(competitor_id);
	}
	else if (domain == Event::EventPlugin::DBEVENT_RUN_CHANGED) {
		auto lst = data.toList();
		auto run_id = lst.value(0).toInt();
		auto ei = eventInfo();
		bool this_stage = false;
		qf::core::sql::Query q;
		q.execThrow(QStringLiteral("SELECT COUNT(*) FROM runs WHERE id=%1 AND stageId=%2").arg(run_id).arg(ei.stage()));
		if (q.next()) {
			this_stage = q.value(0).toInt() == 1;
		}
		if (!this_stage) {
			return;
		}
		auto qe_run_rec = lst.value(1).toMap();
		auto qx_run_rec = QVariantMap();
		auto remap_key = [&qe_run_rec, &qx_run_rec](const QString &qe_key, const QString &qx_key) {
			if (qe_run_rec.contains(qe_key)) {
				qx_run_rec[qx_key] = qe_run_rec.value(qe_key);
			}
		};

		qfInfo() << "DBEVENT_RUN_CHANGED run id:" << run_id << "data:" << QString::fromUtf8(QJsonDocument::fromVariant(qe_run_rec).toJson(QJsonDocument::Compact));

		remap_key("runs.siId", "si_id");
		remap_key("competitors.registration", "registration");
		if (!qx_run_rec.isEmpty()) {
			httpPostJson( "/api/event/current/changes/run-updated", QStringLiteral("run_id=%1").arg(run_id), qx_run_rec);
		}
	}
}

QNetworkAccessManager *QxClientService::networkManager()
{
	if (!m_networkManager) {
		m_networkManager = new QNetworkAccessManager(this);
	}
	return m_networkManager;
}

QNetworkReply *QxClientService::getRemoteEventInfo(const QString &qxhttp_host, const QString &api_token)
{
	auto *nm = networkManager();
	QNetworkRequest request;
	QUrl url(qxhttp_host);
	url.setPath("/api/event/current");
	request.setUrl(url);
	request.setRawHeader(QX_API_TOKEN, api_token.toUtf8());
	return nm->get(request);
}

QNetworkReply *QxClientService::postEventInfo(const QString &qxhttp_host, const QString &api_token)
{
	auto *nm = networkManager();
	QNetworkRequest request;
	QUrl url(qxhttp_host);
	// qfInfo() << "url " << url.toString();
	url.setPath("/api/event/current");
	// qfInfo() << "GET " << url.toString();
	request.setUrl(url);
	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	request.setRawHeader(QX_API_TOKEN, api_token.toUtf8());
	auto ei = eventInfo();
	auto data = QJsonDocument::fromVariant(ei).toJson();
	return nm->post(request, data);
}

void QxClientService::exportStartListIofXml3(QObject *context, std::function<void (QString)> call_back)
{
	auto *ep = getPlugin<EventPlugin>();
	int current_stage = ep->currentStageId();
	bool is_relays = ep->eventConfig()->isRelays();
	if (!is_relays) {
		auto xml = getPlugin<RunsPlugin>()->startListStageIofXml30(current_stage);
		uploadSpecFile(SpecFile::StartListIofXml3, xml.toUtf8(), context, call_back);
	}
}

void QxClientService::exportRuns(QObject *context, std::function<void (QString)> call_back)
{
	auto *ep = getPlugin<EventPlugin>();
	int current_stage = ep->currentStageId();
	bool is_relays = ep->eventConfig()->isRelays();
	if (!is_relays) {
		auto runs = getPlugin<RunsPlugin>()->qxExportRunsCsv(current_stage);
		uploadSpecFile(SpecFile::RunsCsv, runs.toUtf8(), context, call_back);
	}
}

QNetworkReply* QxClientService::loadQxChanges(int from_id)
{
	auto url = exchangeServerUrl();

	url.setPath(QStringLiteral("/api/event/%1/changes").arg(eventId()));
	url.setQuery(QStringLiteral("from_id=%1").arg(from_id));
	QNetworkRequest request;
	request.setUrl(url);
	return networkManager()->get(request);
}

int QxClientService::eventId() const
{
	if (m_eventId == 0) {
		throw qf::core::Exception(tr("Event ID is not loaded, service is not probably running."));
	}
	return m_eventId;
}

QByteArray QxClientService::apiToken() const
{
	// API token must not be cached to enable service point
	// always to current stage event on qxhttpd
	auto *event_plugin = getPlugin<EventPlugin>();
	auto current_stage = event_plugin->currentStageId();
	return event_plugin->stageData(current_stage).qxApiToken().toUtf8();
}

QUrl QxClientService::exchangeServerUrl() const
{
	auto ss = settings();
	return QUrl(ss.exchangeServerUrl());
}

void QxClientService::postFileCompressed(std::optional<QString> path, std::optional<QString> name, QByteArray data, QObject *context , std::function<void (QString)> call_back)
{
	auto url = exchangeServerUrl();

	url.setPath(path.value_or("/api/event/current/file"));
	if (name.has_value()) {
		url.setQuery(QStringLiteral("name=%1").arg(name.value()));
	}
	QNetworkRequest request;
	request.setUrl(url);
	request.setRawHeader(QX_API_TOKEN, apiToken());
	request.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/zip"));
	auto zdata = zlibCompress(data);
	QNetworkReply *reply = networkManager()->post(request, zdata);

	connect(reply, &QNetworkReply::finished, context, [reply, name, call_back]() {
		QString err;
		if(reply->error()) {
			err = reply->errorString();
			qfWarning() << "Post file:" << name.value_or("SPEC") << "error:" << err;
		}
		if (call_back) {
			call_back(err);
		}
		reply->deleteLater(); // should be called by Qt anyway
	});
}

void QxClientService::uploadSpecFile(SpecFile file, QByteArray data, QObject *context, const std::function<void (QString)> &call_back)
{
	switch (file) {
	case SpecFile::StartListIofXml3:
		postFileCompressed({}, "startlist-iof3.xml", data, context, call_back);
		break;
	case SpecFile::RunsCsv:
		postFileCompressed({}, "runs.csv", data, context, call_back);
		break;
	}
}

QByteArray QxClientService::zlibCompress(QByteArray data)
{
	QByteArray compressedData = qCompress(data);
	// strip the 4-byte length put on by qCompress
	// internally qCompress uses zlib
	compressedData.remove(0, 4);
	return compressedData;
}

void QxClientService::httpPostJson(const QString &path, const QString &query, QVariantMap json, QObject *context, const std::function<void (QString)> &call_back)
{
	if (!isRunning()) {
		return;
	}
	auto url = exchangeServerUrl();

	url.setPath(path);
	url.setQuery(query);

	QNetworkRequest request;
	request.setUrl(url);
	request.setRawHeader(QX_API_TOKEN, apiToken());
	request.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/json"));
	auto data = QJsonDocument::fromVariant(json).toJson(QJsonDocument::Compact);
	qfInfo() << "HTTP POST JSON:" << url.toString() << "data:" << QString::fromUtf8(data);
	QNetworkReply *reply = networkManager()->post(request, data);
	if (context) {
		connect(reply, &QNetworkReply::finished, context, [reply, url, call_back]() {
			QString err;
			if(reply->error()) {
				err = reply->errorString();
				qfWarning() << "HTTP POST:" << url.toString() << "error:" << err;
			}
			if (call_back) {
				call_back(err);
			}
			reply->deleteLater();
		});
	}
	else {
		connect(reply, &QNetworkReply::finished, [reply, url, call_back]() {
			if(reply->error()) {
				qfWarning() << "HTTP POST:" << url.toString() << "error:" << reply->errorString();
			}
			reply->deleteLater();
		});
	}
}

void QxClientService::connectToSSE(int event_id)
{
	Q_UNUSED(event_id);
	// auto url = exchangeServerUrl();
	// url.setPath(QStringLiteral("/api/event/%1/run/changes/sse").arg(event_id));
	// QNetworkRequest request(url);
	// request.setRawHeader(QByteArray("Accept"), QByteArray("text/event-stream"));
	// request.setHeader(QNetworkRequest::UserAgentHeader, "QuickEvent");
	// request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
	// request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork); // Events shouldn't be cached

	// qfInfo() << "Connecting to SSE:" << url.toString();
	// m_replySSE = networkManager()->get(request);
	// qfInfo() << "Connected";
	// connect(m_replySSE, &QNetworkReply::readyRead, this, [this]() {
	// 	auto data = m_replySSE->readAll();
	// 	qfInfo() << "DATA:" << data.toStdString();
	// });
	// connect(m_replySSE, &QNetworkReply::finished, this, [this]() {
	// 	qfInfo() << "SSE finished:" << m_replySSE->errorString();
	// });
}

void QxClientService::disconnectSSE()
{
	if (m_replySSE) {
		qfInfo() << "Disconnecting SSE:" << m_replySSE;
		m_replySSE->deleteLater();
		m_replySSE = nullptr;
	}
}

void QxClientService::pollQxChanges()
{
	auto event_plugin = getPlugin<EventPlugin>();
	if(!getPlugin<EventPlugin>()->isEventOpen()) {
		return;
	}
	int stage_id = event_plugin->currentStageId();
	try {
		int max_change_id = 0;
		qf::core::sql::Query q;
		q.execThrow("SELECT MAX(change_id) FROM qxchanges");
		if (q.next()) {
			max_change_id = q.value(0).toInt();
		}
		auto *reply = loadQxChanges(max_change_id + 1);
		connect(reply, &QNetworkReply::finished, this, [reply, stage_id]() {
			QString err;
			if(reply->error()) {
				err = reply->errorString();
				qfWarning() << "Load qxchanges error:" << err;
			}
			else {
				QJsonParseError err;
				auto data = reply->readAll();
				auto json = QJsonDocument::fromJson(data, &err);
				if (err.error != QJsonParseError::NoError) {
					qfWarning() << "Parse qxchanges error:" << err.errorString();
				}
				else {
					auto records = json.array().toVariantList();
					qf::core::sql::Query q;
					q.prepare("INSERT INTO qxchanges (data_type, data, data_id, source, user_id, status, status_message, stage_id, change_id, created)"
							  " VALUES (:data_type, :data, :data_id, :source, :user_id, :status, :status_message, :stage_id, :change_id, :created)"
							  " RETURNING id");
					for (const auto &v : records) {
						auto rec = v.toMap();
						auto ba = QJsonDocument::fromVariant(rec.value("data")).toJson(QJsonDocument::Compact);
						auto data = QString::fromUtf8(ba);
						q.bindValue(":data_type", rec.value("data_type"));
						q.bindValue(":data", data);
						q.bindValue(":data_id", rec.value("data_id"));
						q.bindValue(":source", rec.value("source"));
						q.bindValue(":user_id", rec.value("user_id"));
						q.bindValue(":status", rec.value("status"));
						q.bindValue(":status_message", rec.value("status_message"));
						q.bindValue(":stage_id", stage_id);
						auto change_id = rec.value("id").toInt();
						Q_ASSERT(change_id > 0);
						q.bindValue(":change_id", change_id);
						auto created = QDateTime::fromString(rec.value("created").toString(), Qt::ISODate);
						qfDebug() << "created:" << created.toString(Qt::ISODate);
						q.bindValue(":created", created);
						// may fail on prikey violation if more clients are inserting simultaneously
						if (q.exec()) {
							if (q.next()) {
								auto id = q.value(0).toInt();
								qfDebug() << "insert ID:" << id;
								getPlugin<EventPlugin>()->emitDbEvent(Event::EventPlugin::DBEVENT_QX_CHANGE_RECEIVED, id, true);
							}
						}
						else {
							qfInfo() << "sql error:" << q.lastErrorText();

						}
					}
				}
			}
			reply->deleteLater();
		});
	}
	catch (const qf::core::Exception &e) {
		qfWarning() << "Load qxchanges error:" << e.message();
	}
}

EventInfo QxClientService::eventInfo() const
{
	auto *event_plugin = getPlugin<EventPlugin>();
	auto *event_config = event_plugin->eventConfig();
	EventInfo ei;
	ei.set_stage(event_plugin->currentStageId());
	ei.set_stage_count(event_plugin->stageCount());
	ei.set_name(event_config->eventName());
	ei.set_place(event_config->eventPlace());
	ei.set_start_time(event_plugin->stageStartDateTime(event_plugin->currentStageId()).toString(Qt::ISODate));
	return ei;
}

} // namespace Event::services::qx
