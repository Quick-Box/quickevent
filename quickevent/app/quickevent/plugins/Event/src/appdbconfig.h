#pragma once

#include "eventconfig.h"
#include "plugins/Event/src/stageconfig.h"
#include "services/oresultsconfig.h"
#include "services/ofeed/ofeedconfig.h"
#include <plugins/Receipts/src/receiptsconfig.h>

#include <QDateTime>
#include <QObject>
#include <QSet>
#include <QVariantMap>
#include <qcontainerfwd.h>

namespace Event {

struct QxConfig {
	static QxConfig fromVariantMap(const QVariantMap &map);
	QVariantMap toVariantMap() const;

	QString apiToken;
};

class AppDbConfig {
public:
	explicit AppDbConfig() = default;

public:
	void load();

	int dbVersion() const { return m_dbVersion; }
	const EventConfig &eventConfig() const { return m_eventConfig; }
	void setEventConfig(const EventConfig &config);
	const StageConfig &stageConfig(int stage_id) const;
	void setStageConfig(int stage_id, const StageConfig &config);
	const services::OResultsConfig &oresultsConfig(int stage_id) const;
	void setOresultsConfig(int stage_id, const services::OResultsConfig &config);
	const Receipts::ReceiptsConfig &receiptsConfig(int stage_id) const;
	void setReceiptsConfig(int stage_id, const Receipts::ReceiptsConfig &config);
	const services::OFeedConfig &ofeedConfig(int stage_id) const;
	void setOfeedConfig(int stage_id, const services::OFeedConfig &config);
	const QxConfig &qxConfig() const { return m_qxConfig; }
	void setQxConfig(const QxConfig &config);

private:
	void save(const QString &prefix, const QVariantMap &data);
	void save(const QString &prefix, int stage_id, const QVariantMap &data);

private:
    std::map<int, StageConfig> m_stagesConfig;
	std::map<int, services::OFeedConfig> m_ofeedConfig;
	std::map<int, Receipts::ReceiptsConfig> m_receiptsConfig;
	std::map<int, services::OResultsConfig> m_oresultsConfig;
	EventConfig m_eventConfig;
	QxConfig m_qxConfig;
	int m_dbVersion = 0;
};

} // namespace Event
