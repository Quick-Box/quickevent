#ifndef OFEEDCLIENT_H
#define OFEEDCLIENT_H

#pragma once

#include "../service.h"
#include <functional>

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
	bool printEventImageOnReceipt() const;
	void setPrintEventImageOnReceipt(bool on);
	bool printEventQrCodeOnReceipt() const;
	void setPrintEventQrCodeOnReceipt(bool on);
	int receiptImageHeightMm() const;
	void setReceiptImageHeightMm(int height_mm);
	QString receiptEventLinkUrl() const;
	QString defaultReceiptEventLinkUrl() const;
	void setReceiptEventLinkUrl(QString link_url);
	QString receiptEventQrCodeCaption() const;
	QString defaultReceiptEventQrCodeCaption() const;
	void setReceiptEventQrCodeCaption(QString caption);
	bool hasCachedEventImage() const;
	QString cachedEventImageBase64() const;
	QString cachedEventImageFormat() const;
	void refreshEventImageCache(std::function<void(bool success, const QString &message)> callback = nullptr);
	void testConnection(const QString &hostUrl,
						const QString &eventId,
						const QString &eventPassword,
						std::function<void(bool success, const QString &message)> callback);

private:
	QTimer *m_exportTimer = nullptr;
	QNetworkAccessManager *m_networkManager = nullptr;
	const QString OFEED_API_URL = "https://api.orienteerfeed.com";
	bool m_eventImageStartupAttempted = false;

private:
	qf::gui::framework::DialogWidget *createDetailWidget() override;
	void onExportTimerTimeOut();
	void init();
	void ensureEventImageCachedAtStartup();
	QString receiptConfigKey(const QString &suffix) const;
	QVariant receiptConfigValue(const QString &suffix, const QVariant &default_value = QVariant()) const;
	void setReceiptConfigValue(const QString &suffix, const QVariant &value);
	void setCachedEventImage(const QByteArray &raw_data, const QString &format);
	void clearCachedEventImage();
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
