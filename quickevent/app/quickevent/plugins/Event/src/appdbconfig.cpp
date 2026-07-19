#include "appdbconfig.h"

#include "eventplugin.h"
#include "eventconfig.h"

#include <qcontainerfwd.h>
#include <qf/core/assert.h>
#include <qf/core/utils.h>
#include <qf/core/sql/connection.h>
#include <qf/core/sql/query.h>
#include <qf/core/sql/querybuilder.h>
#include <qf/core/log.h>

#include <qf/gui/framework/mainwindow.h>
#include <qf/gui/dialogs/messagebox.h>

#include <QDateTime>

using namespace Event;

namespace {

constexpr auto EVENT = "event";
constexpr auto STAGE = "stage";
constexpr auto RECEIPTS = "receipts";
constexpr auto OFEED = "ofeed";
constexpr auto ORESULTS = "oresults";
constexpr auto QX = "qx";

QVariantMap changedValues(const QVariantMap &old_values, const QVariantMap &new_values)
{
	QVariantMap ret;
	for (const auto &[key, val] : new_values.asKeyValueRange()) {
		if(old_values.value(key) != val)
			ret.insert(key, val);
	}
	return ret;
}

}

QxConfig QxConfig::fromVariantMap(const QVariantMap &map)
{
	QxConfig ret;
	ret.apiToken = map.value("apiToken").toString();
	return ret;
}

QVariantMap QxConfig::toVariantMap() const
{
	QVariantMap ret;
	ret["apiToken"] = apiToken;
	return ret;
}

void AppDbConfig::load()
{
	using namespace qf::core::sql;
	Connection conn = Connection::forName();
	Q_ASSERT(conn.isOpen());

	QVariantMap config;
	const auto set_group_value = [&config](const QString &group, const QString &key, const QVariant &value) {
		auto m = config.value(group).toMap();
		m[key] = value;
		config[group] = m;
	};
	const auto set_group_stage_value = [&config](const QString &group, int stage_id, const QString &key, const QVariant &value) {
		auto stage = QString::number(stage_id);
		auto m1 = config.value(group).toMap();
		auto m2 = m1.value(stage).toMap();
		m2[key] = value;
		m1[stage] = m2;
		config[group] = m1;
	};

	Query q(conn);
	QueryBuilder qb;
	qb.select("ckey, cvalue, ctype").from("config");
	if(q.exec(qb.toString(), qf::core::Exception::Throw)) {
		while(q.next()) {
			QString key = q.value(0).toString();
			QVariant val = q.value(1);
			QString type = q.value(2).toString();
			QVariant v = qf::core::Utils::retypeStringValue(val.toString(), type);
			const auto group = key.section('.', 0, 0);
			if (group == EVENT || group == QX) {
				set_group_value(group, key.section('.', 1), v);
			} else if (group == STAGE || group == RECEIPTS || group == ORESULTS || group == OFEED) {
				auto stage_id = key.section('.', 1, 1).toInt();
				set_group_stage_value(group, stage_id, key.section('.', 2), v);
			} else {
				config[key] = v;
			}

		}
	}
	m_dbVersion = config.value("db.version", 0).toInt();
	Q_ASSERT(m_dbVersion > 0);

	m_qxConfig = QxConfig::fromVariantMap(config.value(QX).toMap());
	m_eventConfig = EventConfig::fromVariantMap(config.value(EVENT).toMap());

	const auto load_stage_config = [&config](const QString &group, auto &target, auto fromVariantMap) {
		for ( const auto &[stage, val] : config.value(group).toMap().asKeyValueRange()) {
			target[stage.toInt()] = fromVariantMap(val.toMap());
		}
	};
	{
		auto stages = config.value(STAGE).toMap();
		Query stages_q(conn);
		stages_q.exec("SELECT * FROM stages ORDER BY id", !qf::core::Exception::Throw);
		while(stages_q.next()) {
			const auto stage_id = stages_q.value("id").toString();
			if(stages.contains(stage_id))
				continue;
			set_group_stage_value(STAGE, stage_id.toInt(), "startDateTime", stages_q.value("startDateTime").toDateTime());
			set_group_stage_value(STAGE, stage_id.toInt(), "useAllMaps", stages_q.value("useAllMaps").toBool());
			set_group_stage_value(STAGE, stage_id.toInt(), "drawingConfig", stages_q.value("drawingConfig").toString());
		}
	}
	load_stage_config(STAGE, m_stagesConfig, StageConfig::fromVariantMap);
	load_stage_config(RECEIPTS, m_receiptsConfig, Receipts::ReceiptsConfig::fromVariantMap);
	load_stage_config(ORESULTS, m_oresultsConfig, services::OResultsConfig::fromVariantMap);
	load_stage_config(OFEED, m_ofeedConfig, services::OFeedConfig::fromVariantMap);
}

void AppDbConfig::save(const QString &prefix, int stage_id, const QVariantMap &data)
{
	save(prefix + "." + QString::number(stage_id), data);
}

void AppDbConfig::save(const QString &prefix, const QVariantMap &data)
{
    if(data.isEmpty()) {
        return;
    }

	using namespace qf::core::sql;
	Connection conn = Connection::forName();

	Query q_up(conn);
	q_up.prepare("UPDATE config SET cvalue=:val WHERE ckey=:key", qf::core::Exception::Throw);
	Query q_ins(conn);
	q_ins.prepare("INSERT INTO config (ckey, cvalue, ctype) VALUES (:key, :val, :type)", qf::core::Exception::Throw);
	for (const auto &[key, val] : data.asKeyValueRange()) {
		QString val_str;
		if(val.typeId() == qMetaTypeId<QDate>())
			val_str = val.toDate().toString(Qt::ISODate);
		else if(val.typeId() == qMetaTypeId<QTime>())
			val_str = val.toTime().toString(Qt::ISODate);
		else if(val.typeId() == qMetaTypeId<QDateTime>())
			val_str = val.toDateTime().toString(Qt::ISODate);
		else if(val.typeId() == qMetaTypeId<QVariantMap>())
			val_str = qf::core::Utils::qvariantToJson(val);
		else
			val_str = val.toString();
		auto full_key = prefix.isEmpty() ? key : prefix + "." + key;
		q_up.bindValue(":key", full_key);
		q_up.bindValue(":val", val_str);
		q_up.exec(qf::core::Exception::Throw);
		if(q_up.numRowsAffected() < 1) {
			QString type = val.typeName();
			q_ins.bindValue(":key", full_key);
			q_ins.bindValue(":type", type);
			q_ins.bindValue(":val", val_str);
			q_ins.exec(qf::core::Exception::Throw);
		}
	}
}

void AppDbConfig::setEventConfig(const EventConfig &config)
{
	const QVariantMap changed_values = changedValues(m_eventConfig.toVariantMap(), config.toVariantMap());
	m_eventConfig = config;
	save(EVENT, changed_values);
}

const services::OResultsConfig& AppDbConfig::oresultsConfig(int stage_id) const
{
	if (m_oresultsConfig.contains(stage_id)) {
		return m_oresultsConfig.at(stage_id);
	}
	static services::OResultsConfig defaultCfg;
	return defaultCfg;
}

const StageConfig& AppDbConfig::stageConfig(int stage_id) const
{
	if (m_stagesConfig.contains(stage_id)) {
		return m_stagesConfig.at(stage_id);
	}
	static StageConfig defaultCfg;
	return defaultCfg;
}

void AppDbConfig::setStageConfig(int stage_id, const StageConfig &cfg)
{
	const QVariantMap changed_values = changedValues(stageConfig(stage_id).toVariantMap(), cfg.toVariantMap());
	m_stagesConfig[stage_id] = cfg;
	save(STAGE, stage_id, changed_values);
}

void AppDbConfig::setOresultsConfig(int stage_id, const services::OResultsConfig &cfg)
{
	const QVariantMap changed_values = changedValues(oresultsConfig(stage_id).toVariantMap(), cfg.toVariantMap());
	m_oresultsConfig[stage_id] = cfg;
	save(ORESULTS, stage_id, changed_values);
}

const Receipts::ReceiptsConfig& AppDbConfig::receiptsConfig(int stage_id) const
{
	if (m_receiptsConfig.contains(stage_id)) {
		return m_receiptsConfig.at(stage_id);
	}
	static Receipts::ReceiptsConfig defaultCfg;
	return defaultCfg;
}

void AppDbConfig::setReceiptsConfig(int stage_id, const Receipts::ReceiptsConfig &cfg)
{
	const QVariantMap changed_values = changedValues(receiptsConfig(stage_id).toVariantMap(), cfg.toVariantMap());
	m_receiptsConfig[stage_id] = cfg;
	save(RECEIPTS, stage_id, changed_values);
}

const services::OFeedConfig& AppDbConfig::ofeedConfig(int stage_id) const
{
	if (m_ofeedConfig.contains(stage_id)) {
		return m_ofeedConfig.at(stage_id);
	}
	static services::OFeedConfig defaultCfg;
	return defaultCfg;
}

void AppDbConfig::setOfeedConfig(int stage_id, const services::OFeedConfig &cfg)
{
	const QVariantMap changed_values = changedValues(ofeedConfig(stage_id).toVariantMap(), cfg.toVariantMap());
	m_ofeedConfig[stage_id] = cfg;
	save(OFEED, stage_id, changed_values);
}

void AppDbConfig::setQxConfig(const QxConfig &config)
{
	const QVariantMap changed_values = changedValues(m_qxConfig.toVariantMap(), config.toVariantMap());
	m_qxConfig = config;
	save(QX, changed_values);
}
