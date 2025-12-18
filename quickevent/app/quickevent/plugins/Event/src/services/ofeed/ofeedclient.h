#ifndef OFEEDCLIENT_H
#define OFEEDCLIENT_H

#pragma once

#include "../service.h"

class QTimer;
class QNetworkAccessManager;

namespace Event {
namespace services {

class OFeedClientSettings : public ServiceSettings
{
	using Super = ServiceSettings;

	QF_VARIANTMAP_FIELD2(int, e, setE, xportIntervalSec, 60)
public:
	OFeedClientSettings(const QVariantMap &o = QVariantMap()) : Super(o) {}
};

class OFeedClient : public Service
{
	Q_OBJECT

	using Super = Service;
public:
	OFeedClient(QObject *parent);

	void run() override;
	void stop() override;
	OFeedClientSettings settings() const {return OFeedClientSettings(m_settings);}

	static QString serviceName();
	static bool isInsertFromOFeed;

	void exportResultsIofXml3();
	void exportStartListIofXml3(std::function<void()> on_success = nullptr);
	void loadSettings() override;
	void onDbEventNotify(const QString &domain, int connection_id, const QVariant &data);

	QString hostUrl() const;
	void setHostUrl(QString eventId);
	QString eventId() const;
	void setEventId(QString eventId);
	QString eventPassword() const;
	void setEventPassword(QString eventPassword);
	QString changelogOrigin() const;
	void setChangelogOrigin(QString changelogOrigin);
	QDateTime lastChangelogCall();
	void setLastChangelogCall(QDateTime lastChangelogCall);
	bool runXmlValidation();
	void setRunXmlValidation(bool runXmlValidation);
	bool runChangesProcessing();
	void setRunChangesProcessing(bool runChangesProcessing);

	private:
	QTimer *m_exportTimer = nullptr;
	QNetworkAccessManager *m_networkManager = nullptr;
	private:
	qf::gui::framework::DialogWidget *createDetailWidget() override;
	void onExportTimerTimeOut();
	void init();
	void sendFile(QString name, QString request_path, QString file, std::function<void()> on_success = nullptr);
	void sendCompetitorUpdate(QString json_body, int competitor_id, bool usingExternalId);
	void sendCompetitorAdded(QString json_body);
	void sendCompetitorDeleted(int run_id);
	void onCompetitorAdded(int competitor_id);
	void onCompetitorEdited(int competitor_id);
	void onCompetitorReadOut(int competitor_id);
	void sendGraphQLRequest(const QString &query, const QJsonObject &variables, std::function<void(QJsonObject)> callback, bool withAuthorization);
	void getChangesByOrigin();
	void processCompetitorsChanges(QJsonArray data_array);
	void processCardChange(int runs_id, const QString &new_value);
	void processStatusChange(int runs_id, const QString &new_value);
	void processNoteChange(int runs_id, const QString &new_value);
	void processNewRunner(int ofeed_competitor_id);
	void storeChange(const QJsonObject &change);
	QByteArray zlibCompress(QByteArray data);
};

}}

#endif // OFEEDCLIENT_H
