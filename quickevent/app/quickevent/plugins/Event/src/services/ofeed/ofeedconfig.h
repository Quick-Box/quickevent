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
	QString changelogOrigin;
	QDateTime lastChangelogCall;
	bool runXmlValidation = true;
	bool runChangesProcessing = false;
	// Non-stage-specific field, stored as ofeed.introTourShowed
	bool introTourShowed = false;
};

}
