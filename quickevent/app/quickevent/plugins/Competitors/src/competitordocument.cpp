#include "competitordocument.h"

#include <plugins/Event/src/eventplugin.h>
#include <plugins/Runs/src/runsplugin.h>

#include <qf/gui/framework/mainwindow.h>
#include <qf/gui/framework/plugin.h>

#include <qf/core/sql/connection.h>
#include <qf/core/sql/query.h>
#include <qf/core/sql/transaction.h>
#include <qf/core/assert.h>

using namespace Competitors;
using qf::gui::framework::getPlugin;
using Event::EventPlugin;
using Runs::RunsPlugin;

CompetitorDocument::CompetitorDocument(QObject *parent)
	: Super(parent)
{
	qf::core::sql::QueryBuilder qb;
	qb.select2("competitors", "*")
			.select("lastName || ' ' || firstName AS name")
			//.select2("classes", "name AS className")
			.from("competitors")
			//.join("competitors.classId", "classes.id")
			.where("competitors.id={{ID}}");
	setQueryBuilder(qb);
}

bool CompetitorDocument::loadData()
{
	m_runsIds.clear();
	if (Super::loadData()) {
		if (auto competitor_id = dataId().toInt(); competitor_id > 0) {
			qf::core::sql::Query q(sqlModel()->connectionName());
			q.exec(QStringLiteral("SELECT id FROM runs WHERE competitorId = %1 ORDER BY stageId").arg(competitor_id));
			while(q.next()) {
				int run_id = q.value(0).toInt();
				m_runsIds << run_id;
			}
		}
		return true;
	}
	return false;
}

bool CompetitorDocument::saveData()
{
	qfLogFuncFrame();
	RecordEditMode old_mode = mode();
	QMap<int, QVariantMap> old_records;
	auto *runs_plugin = getPlugin<RunsPlugin>();
	if (old_mode ==  DataDocument::ModeEdit) {
		auto competitor_id = dataId().toInt();
		qf::core::sql::Query q(sqlModel()->connectionName());
		q.exec(QStringLiteral("SELECT id FROM runs WHERE competitorId = %1").arg(competitor_id));
		while(q.next()) {
			int run_id = q.value(0).toInt();
			old_records[run_id] = runs_plugin->runsRecord(run_id);
		}
	}
	bool siid_dirty = isDirty("competitors.siId");
	bool class_dirty = isDirty("competitors.classId");
	bool ret = Super::saveData();
	qfDebug() << "Super save data:" << ret;
	if(ret) {
		int competitor_id = 0;
		if(old_mode == DataDocument::ModeInsert) {
			// insert runs
			qfDebug() << "inserting runs";
			competitor_id = dataId().toInt();
			int stage_count = getPlugin<EventPlugin>()->stageCount();
			qf::core::sql::Query q(sqlModel()->connectionName());
			q.prepare("INSERT INTO runs (competitorId, stageId, siId) VALUES (:competitorId, :stageId, :siId)");
			m_runsIds.clear();
			for(int i = 0; i < stage_count; i++) {
				q.bindValue(":competitorId", competitor_id);
				q.bindValue(":stageId", i + 1);
				if(siid_dirty)
					q.bindValue(":siId", siid());
				q.exec(qf::core::Exception::Throw);
				m_runsIds << q.lastInsertId().toInt();
			}
			if(m_isEmitDbEventsOnSave) {
				getPlugin<EventPlugin>()->emitDbEvent(Event::EventPlugin::DBEVENT_COMPETITOR_COUNTS_CHANGED);
				for (auto run_id : m_runsIds) {
					auto rec = runs_plugin->runsRecord(run_id);
					getPlugin<EventPlugin>()->emitDbEvent(Event::EventPlugin::DBEVENT_RUN_CHANGED, QVariantList {run_id, rec});
				}
			}
		}
		else if(old_mode == DataDocument::ModeEdit) {
			competitor_id = dataId().toInt();
			if(siid_dirty) {
				qfDebug() << "updating SIID in run tables";
				int competitor_id = dataId().toInt();
				qf::core::sql::Query q(sqlModel()->connectionName());
				q.prepare("UPDATE runs SET siId=:siId WHERE competitorId=:competitorId", qf::core::Exception::Throw);
				q.bindValue(":competitorId", competitor_id);
				q.bindValue(":siId", siid());
				q.exec(qf::core::Exception::Throw);
			}
			if(m_isEmitDbEventsOnSave) {
				if(class_dirty) {
					getPlugin<EventPlugin>()->emitDbEvent(Event::EventPlugin::DBEVENT_COMPETITOR_COUNTS_CHANGED);
				}
				for(const auto &[run_id, old_record] : old_records.asKeyValueRange()) {
					auto record = runs_plugin->runsRecord(run_id);
					auto diff = qf::core::sql::recordDiff(old_record, record);
					if (!diff.isEmpty()) {
						getPlugin<EventPlugin>()->emitDbEvent(Event::EventPlugin::DBEVENT_RUN_CHANGED, QVariantList {run_id, diff});
					}
				}
			}
		}
		else {
			competitor_id = dataId().toInt();
		}
		if(m_isEmitDbEventsOnSave) {
			getPlugin<EventPlugin>()->emitDbEvent(Event::EventPlugin::DBEVENT_COMPETITOR_EDITED, competitor_id);
		}
	}
	return ret;
}

bool CompetitorDocument::dropData()
{
	bool ret = false;
	auto competitor_id = dataId().toInt();
	QList<int> run_ids;
	{
		qf::core::sql::Query q(sqlModel()->connectionName());
		q.exec(QStringLiteral("SELECT id FROM runs WHERE competitorId = %1").arg(competitor_id));
		while(q.next()) {
			run_ids << q.value(0).toInt();
		}
	}
	{
		qf::core::sql::Query q(sqlModel()->connectionName());
		ret = q.exec(QStringLiteral("DELETE FROM runs WHERE competitorId = %1").arg(competitor_id));
		if(!ret)
			qfError() << q.lastError().text();
	}
	if(ret) {
		ret = Super::dropData();
		if(m_isEmitDbEventsOnSave) {
			getPlugin<EventPlugin>()->emitDbEvent(Event::EventPlugin::DBEVENT_COMPETITOR_COUNTS_CHANGED);
			for (auto run_id : run_ids) {
				getPlugin<EventPlugin>()->emitDbEvent(Event::EventPlugin::DBEVENT_RUN_CHANGED, QVariantList {run_id, {}});
			}
		}
	}
	return ret;
}
/*
bool CompetitorDocument::isSaveSiidToRuns() const
{
	return m_saveSiidToRuns;
}

void CompetitorDocument::setSaveSiidToRuns(bool save_siid_to_runs)
{
	m_saveSiidToRuns = save_siid_to_runs;
}
*/
namespace {
const auto SIID = QStringLiteral("competitors.siId");
}
/*
void CompetitorDocument::setSiid(const QVariant &siid, bool save_siid_to_runs)
{
	setValue(SIID, siid);
	m_saveSiidToRuns = save_siid_to_runs;
}
*/
void CompetitorDocument::setSiid(const QVariant &siid)
{
	setValue(SIID, siid);
}

QVariant CompetitorDocument::siid() const
{
	return value(SIID);
}


