#pragma once

#include <QDateTime>
#include <QString>

namespace Event::services {

struct OFeedConfig
{
    OFeedConfig();
    static OFeedConfig fromVariantMap(const QVariantMap& map);
    QVariantMap toVariantMap() const;

	QString hostUrl;
	QString eventId;
	QString eventPassword;
	QDateTime lastStartChangelogCall;
	QDateTime lastOfficeChangelogCall;
	bool runXmlValidation = true;
	bool runStartChangesProcessing = false;
	bool runOfficeChangesProcessing = false;
	// Non-stage-specific field, stored as ofeed.introTourShowed
	bool introTourShowed = false;
};

}
