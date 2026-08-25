#include "runstablemodel.h"

#include "runsplugin.h"
#include "../../Event/src/eventplugin.h"

#include <qnamespace.h>
#include <quickevent/core/og/timems.h>
#include <quickevent/core/si/siid.h>

#include <qf/gui/log.h>
#include <qf/gui/framework/mainwindow.h>
#include <qf/gui/framework/application.h>

#include <qf/core/sql/qxrecchng.h>
#include <qf/core/sql/query.h>
#include <qf/core/sql/connection.h>
#include <qf/core/sql/transaction.h>
#include <qf/core/assert.h>

#include <QMimeData>
#include <QTimer>

using qf::gui::framework::getPlugin;
using Event::EventPlugin;

RunsTableModel::RunsTableModel(QObject *parent)
	: Super(parent)
{
	setIdColumnName("runs.id");

	clearColumns(col_COUNT);
	setColumn(col_runs_isRunning, ColumnDefinition("runs.isRunning", tr("Running")));
	setColumn(col_runs_id, ColumnDefinition("runs.id", tr("id")).setReadOnly(true));
	setColumn(col_relays_name, ColumnDefinition("relayName", tr("Relay")));
	setColumn(col_runs_leg, ColumnDefinition("runs.leg", tr("Leg")));
	setColumn(col_classes_name, ColumnDefinition("classes.name", tr("Class")));
	setColumn(col_startNumber, ColumnDefinition("startNumber", tr("SN", "start number")).setToolTip(tr("Start number")));
	setColumn(col_course_id, ColumnDefinition("runs.courseId", tr("Course")));
	setColumn(col_competitors_siId, ColumnDefinition("competitors.siId", tr("SI reg")).setToolTip(tr("Registered SI")).setReadOnly(true));
	setColumn(col_competitorName, ColumnDefinition("competitorName", tr("Name")));
	setColumn(col_registration, ColumnDefinition("registration", tr("Reg")));
	setColumn(col_runs_license, ColumnDefinition("licence", tr("Lic")).setToolTip(tr("License")));
	setColumn(col_runs_siId, ColumnDefinition("runs.siId", tr("SI")).setToolTip(tr("Run SI")).setCastType(qMetaTypeId<quickevent::core::si::SiId>()));
	setColumn(col_runs_corridorTime, ColumnDefinition("runs.corridorTime", tr("Corridor")).setToolTip(tr("Time when the competitor entered start corridor")).setFormat(QStringLiteral("dd.MM.yyyy hh:mm:ss")));
	setColumn(col_runs_checkTimeMs, ColumnDefinition("runs.checkTimeMs", tr("Check")).setCastType(qMetaTypeId<quickevent::core::og::TimeMs>()));
	setColumn(col_runs_startTimeMs, ColumnDefinition("runs.startTimeMs", tr("Start")).setCastType(qMetaTypeId<quickevent::core::og::TimeMs>()));
	setColumn(col_runs_startGateTime, ColumnDefinition("runs.startGateTime", tr("Start gate")));
	setColumn(col_runs_finishTimeMs, ColumnDefinition("runs.finishTimeMs", tr("Finish")).setCastType(qMetaTypeId<quickevent::core::og::TimeMs>()));
	setColumn(col_runs_finishGateTime, ColumnDefinition("runs.finishGateTime", tr("Finish gate")));
	setColumn(col_runs_penaltyTimeMs, ColumnDefinition("runs.penaltyTimeMs", tr("Penalty")).setCastType(qMetaTypeId<quickevent::core::og::TimeMs>()));
	setColumn(col_runs_timeMs, ColumnDefinition("runs.timeMs", tr("Time")).setCastType(qMetaTypeId<quickevent::core::og::TimeMs>()));
	setColumn(col_runFlags, ColumnDefinition("runFlags", tr("Run flags")).setReadOnly(true));
	setColumn(col_cardFlags, ColumnDefinition("cardFlags", tr("Card flags")).setReadOnly(true));
	setColumn(col_runs_rankingPos, ColumnDefinition("ranking", tr("Ranking pos")).setToolTip(tr("Runner's position in CZ ranking.")).setReadOnly(false));
	setColumn(col_iofId, ColumnDefinition("iofId", tr("IOF ID")).setReadOnly(true));
	setColumn(col_competitors_note, ColumnDefinition("competitors.note", tr("Note")));

	connect(this, &RunsTableModel::dataChanged, this, &RunsTableModel::onDataChanged, Qt::QueuedConnection);
	connect(qf::gui::framework::Application::instance(), &qf::gui::framework::Application::qxRecChng, this, &RunsTableModel::onQxRecChng, Qt::QueuedConnection);
}

void RunsTableModel::load(int stage_id, int class_id, bool show_offrace)
{
	m_stageId = stage_id;
	auto qb = getPlugin<Runs::RunsPlugin>()->runsQuery(stage_id, class_id, show_offrace);
	qb.orderBy("runs.id");
	qfDebug() << qb.toString();
	setQueryBuilder(qb, false);
	reload();
}

Qt::ItemFlags RunsTableModel::flags(const QModelIndex &index) const
{
	Qt::ItemFlags flgs = Super::flags(index);
	ColumnDefinition cd = columnDefinition(index.column());
	if(index.column() == col_runs_startTimeMs) {
		flgs = Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled | flgs;
		//qfInfo() << flgs;
	}
	if(index.column() == col_course_id) {
		if (getPlugin<Event::EventPlugin>()->appDbConfig().eventConfig().isRelays()) {
			flgs &= ~Qt::ItemIsEditable;
		}
	}
	return flgs;
}

QVariant RunsTableModel::data(const QModelIndex &index, int role) const
{
	if(index.column() == col_course_id && role == Qt::DisplayRole) {
		if (getPlugin<Event::EventPlugin>()->appDbConfig().eventConfig().isRelays()) {
			auto start_number = value(index.row(), "startNumber").toInt();
			auto leg = value(index.row(), "runs.leg").toInt();
			return QStringLiteral("%1.%2").arg(start_number).arg(leg);
		}
		return Super::data(index, role);
	}
	if((index.column() == col_runs_startTimeMs
	|| index.column() == col_runs_checkTimeMs
	|| index.column() == col_runs_finishTimeMs
	|| index.column() == col_runs_timeMs)
	&& role == Qt::ToolTipRole) {
		QVariant raw = Super::data(index, Qt::EditRole);
		if(raw.isNull() || !raw.isValid())
			return QVariant();
		auto *event_plugin = getPlugin<EventPlugin>();
		auto dt = event_plugin->stageStartDateTime(m_stageId)
			.addMSecs(raw.toInt())
			.toLocalTime();
		return dt.time().toString(Qt::ISODateWithMs);
	}

	if((index.column() == col_runs_corridorTime
	|| index.column() == col_runs_startGateTime
	|| index.column() == col_runs_finishGateTime)
	&& role == Qt::ToolTipRole) {
		QVariant raw = Super::data(index, Qt::EditRole);
		if(raw.isNull() || !raw.isValid())
			return QVariant();
		auto time = raw.toDateTime().toLocalTime().time();
		if (time.msec() == 0)
			return time.toString(Qt::ISODateWithMs);
		return time.addMSecs(raw.toInt()).toString(Qt::ISODateWithMs);
	}

	if((index.column() == col_runs_corridorTime
	|| index.column() == col_runs_startGateTime
	|| index.column() == col_runs_finishGateTime)) {
		auto *event_plugin = getPlugin<EventPlugin>();
		if (role == Qt::DisplayRole) {
			QVariant raw = Super::data(index, Qt::EditRole);
			auto dt = raw.toDateTime();
			if(!dt.isValid())
				return QVariant();
			QDateTime stage_start = event_plugin->stageStartDateTime(m_stageId);
			if(!stage_start.isValid())
				return QVariant();
			qint64 offset_ms = stage_start.msecsTo(dt);
			return quickevent::core::og::TimeMs(static_cast<int>(offset_ms)).toString();
		}
		if(index.column() == col_runs_startGateTime || index.column() == col_runs_finishGateTime) {
			if (role == Qt::BackgroundRole) {
				const auto &config = event_plugin->appDbConfig().eventConfig();
				const bool is_start = index.column() == col_runs_startGateTime;
				const int time_col = is_start ? col_runs_startTimeMs : col_runs_finishTimeMs;
				const int tolerance = is_start ? config.startGateToleranceMs : config.finishGateToleranceMs;
				auto check_gate = [&](const QModelIndex &idx, int tc, int tol) -> QVariant {
					auto gate_time = Super::data(idx, Qt::EditRole).toDateTime();
					if(!gate_time.isValid()) {
						return QVariant();
					}
					auto time_v = Super::data(idx.sibling(idx.row(), tc), Qt::EditRole);
					if(!time_v.isValid()) {
						return QVariant();
					}
					auto time_msec = time_v.toInt();
					auto ref_time = event_plugin->stageStartDateTime(m_stageId).addMSecs(time_msec);
					const bool is_ok = std::abs(gate_time.msecsTo(ref_time)) <= tol;
					return is_ok ? QColor("lightgreen") : QColor("salmon");
				};
				return check_gate(index, time_col, tolerance);
			}
		}
		return Super::data(index, role);
	}

	return Super::data(index, role);
}

QVariant RunsTableModel::value(int row_ix, int column_ix) const
{
	if(column_ix == col_runFlags) {
		qf::core::utils::TableRow row = tableRow(row_ix);
		bool mis_punch = row.value(QStringLiteral("runs.misPunch")).toBool();
		bool bad_check = row.value(QStringLiteral("runs.badCheck")).toBool();
		bool not_start = row.value(QStringLiteral("runs.notStart")).toBool();
		bool not_finish = row.value(QStringLiteral("runs.notFinish")).toBool();
		bool is_disqualified_by_organizer = row.value(QStringLiteral("runs.disqualifiedByOrganizer")).toBool();
		bool over_time = row.value(QStringLiteral("runs.overTime")).toBool();
		bool not_competing = row.value(QStringLiteral("runs.notCompeting")).toBool();
		QStringList sl;
		if(not_competing)
			sl << tr("NC", "NotCompeting");
		if(mis_punch)
			sl << tr("MP", "MisPunch");
		if(bad_check)
			sl << tr("BC", "BadCheck");
		if(not_start)
			sl << tr("DNS", "DidNotStart");
		if(not_finish)
			sl << tr("DNF", "DidNotFinish");
		if(is_disqualified_by_organizer)
			sl << tr("DO", "disqualifiedByOrganizer");
		if(over_time)
			sl << tr("OT", "OverTime");
		if(sl.isEmpty())
			return QStringLiteral("");
		return sl.join(',');
	}
	if(column_ix == col_cardFlags) {
		qf::core::utils::TableRow row = tableRow(row_ix);
		bool card_rent_requested = row.value(QStringLiteral("runs.cardLent")).toBool();
		bool card_returned = row.value(QStringLiteral("runs.cardReturned")).toBool();
		bool card_in_lent_table = row.value(QStringLiteral("cardInLentTable")).toBool();
		QStringList sl;
		if(card_rent_requested)
			sl << tr("CR", "Card rent requested");
		if(card_in_lent_table)
			sl << tr("CT", "Card in lent cards table");
		if(card_returned)
			sl << tr("RET", "Card returned");
		if(sl.isEmpty())
			return QStringLiteral("");
		return sl.join(',');
	}
	if(column_ix == col_runs_isRunning) {
		bool is_running = Super::value(row_ix, column_ix).toBool();
		return is_running;
	}
	return Super::value(row_ix, column_ix);
}



bool RunsTableModel::setValue(int row_ix, int column_ix, const QVariant &val)
{
	//qfInfo() << column_ix << val << val.typeName() << "is null:" << val.isNull();
	if(column_ix == col_runs_isRunning) {
		bool is_running = val.toBool();
		if(!is_running) {
			int finish_ms = value(row_ix, col_runs_finishTimeMs).toInt();
			if(finish_ms > 0) {
				emit badDataInput(tr("Cannot set not running flag for competitor with valid finish time."));
				return false;
			}
		}
		return Super::setValue(row_ix, column_ix, is_running);
	}
	if(column_ix == col_runs_finishTimeMs
			|| column_ix == col_runs_startTimeMs
			|| column_ix == col_runs_penaltyTimeMs
			|| column_ix == col_runs_startGateTime
			|| column_ix == col_runs_finishGateTime) {
		bool ret = Super::setValue(row_ix, column_ix, val);
		const int run_id = value(row_ix, col_runs_id).toInt();
		QTimer::singleShot(0, this, [run_id]() {
			// computeStageTime takes values from SQL
			// wait to let model post edited value
			getPlugin<Runs::RunsPlugin>()->computeStageTime(run_id);
		});
		return ret;
	}
	return Super::setValue(row_ix, column_ix, val);
}

static const auto MIME_TYPE = QStringLiteral("application/quickevent.startTime");

QStringList RunsTableModel::mimeTypes() const
{
	QStringList types;
	types << MIME_TYPE;
	return types;
}

QMimeData *RunsTableModel::mimeData(const QModelIndexList &indexes) const
{
	qfLogFuncFrame();
	auto *mimeData = new QMimeData();
	QByteArray encoded_data = QString::number(indexes.value(0).row()).toUtf8();
	mimeData->setData(MIME_TYPE, encoded_data);
	return mimeData;
}

Qt::DropActions RunsTableModel::supportedDropActions() const
{
	//qfLogFuncFrame();
	return Qt::MoveAction;// | Qt::CopyAction;
}

bool RunsTableModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent)
{
	// qfLogFuncFrame() << "row:" << row << "col:" << column << "parent:" << parent;
	if (!canDropMimeData(data, action, row, column, parent))
		return false;

	if (action == Qt::IgnoreAction)
		return true;
	if (!parent.isValid()) {
		// not drop on item
		return false;
	}
	int r1 = QString::fromUtf8(data->data(MIME_TYPE)).toInt();
	int r2 = parent.row();
	qfDebug() << "switch:" << r1 << "and" << r2;
	switchStartTimes(r1, r2);
	return true;
}

void RunsTableModel::switchStartTimes(int r1, int r2)
{
	qfLogFuncFrame() << r1 << r2;
	int col_stime = columnIndex("startTimeMs");
	QF_ASSERT(col_stime >= 0, "Bad startTimeMs column!", return);

	int id1 = value(r1, "runs.id").toInt();
	int id2 = value(r2, "runs.id").toInt();
	QString err_msg;
	QModelIndex ix1 = index(r1, col_stime);
	QVariant v1 = ix1.data(Qt::EditRole);
	QModelIndex ix2 = index(r2, col_stime);
	QVariant v2 = ix2.data(Qt::EditRole);

	bool is_single_user = sqlConnection().driverName().endsWith(QLatin1String("SQLITE"), Qt::CaseInsensitive);
	if(is_single_user) {
		setData(ix1, v2);
		setData(ix2, v1);
		postRow(r1, true);
		postRow(r2, true);
	}
	else {
		//qf::core::sql::Transaction transaction(sqlConnection());
		auto t1 = v1.value<quickevent::core::og::TimeMs>();
		auto t2 = v2.value<quickevent::core::og::TimeMs>();
		int msec1 = -1, msec2 = -1;
		qf::core::sql::Query q(sqlConnection());
		QString qs = "SELECT id, startTimeMs FROM runs WHERE id IN (" QF_IARG(id1) ", " QF_IARG(id2) ")";
		q.exec(qs, qf::core::Exception::Throw);
		while(q.next()) {
			int id = q.value("id").toInt();
			if(id == id1)
				msec1 = q.value("startTimeMs").toInt();
			else if(id == id2)
				msec2 = q.value("startTimeMs").toInt();
		}
		qfDebug() << t1.msec() << msec1 << t2.msec() << msec2;
		if(msec1 == t1.msec() && msec2 == t2.msec()) {
			setData(ix1, v2);
			setData(ix2, v1);
			postRow(r1, qf::core::Exception::Throw);
			postRow(r2, qf::core::Exception::Throw);
			//transaction.commit();
		}
		else {
			err_msg = tr("Mid-air collision switching start times, reload table and try it again.");
		}
	}
	emit startTimesSwitched(id1, id2, err_msg);
}

void RunsTableModel::onDataChanged(const QModelIndex &top_left, const QModelIndex &bottom_right, const QVector<int> &roles)
{
	Q_UNUSED(roles)
	if(top_left.column() <= RunsTableModel::col_runs_siId && bottom_right.column() >= RunsTableModel::col_runs_siId) {
		emit runnerSiIdEdited();
	}
}

void RunsTableModel::onQxRecChng(const qf::core::sql::QxRecChng &recchng, QObject *source)
{
	handleQxRecChng(recchng, source);
}

bool RunsTableModel::postRow(int row_no, bool throw_exc)
{
	int run_id = value(row_no, col_runs_id).toInt();
	if(!getPlugin<EventPlugin>()->isSingleUser()) {
		if(isDirty(row_no, col_runs_startTimeMs)) {
			int orig_msec = origValue(row_no, col_runs_startTimeMs).toInt();
			int db_msec = 0;
			QString qs = "SELECT id, startTimeMs FROM runs WHERE id=" QF_IARG(run_id); // " FOR UPDATE";
			qf::core::sql::Query q(sqlConnection());
			q.exec(qs, qf::core::Exception::Throw);
			if(q.next()) {
				db_msec = q.value("startTimeMs").toInt();
			}
			if(orig_msec != db_msec) {
				QString err_msg = tr("Mid-air collision setting start time, reload table and try it again.");
				setValue(row_no, col_runs_startTimeMs, db_msec);
				if(throw_exc)
					QF_EXCEPTION(err_msg);
				return false;
			}
		}
	}
	auto dirty_vals = tableRow(row_no).dirtyValuesMap();
	auto ret = Super::postRow(row_no, throw_exc);
	if (!dirty_vals.isEmpty()) {
		getPlugin<EventPlugin>()->emitDbEvent(Event::EventPlugin::DBEVENT_RUN_CHANGED, QVariantList {run_id, dirty_vals});
	}
	return ret;
}
