#include "cardreaderplugin.h"
#include "cardcheckerclassiccpp.h"
#include "cardcheckerfreeordercpp.h"
#include "cardreaderwidget.h"
#include "cardreadersettingspage.h"
#include "services/racomclient.h"
#include "services/qropunch.h"

#include "../../Core/src/coreplugin.h"
#include "../../Core/src/widgets/settingsdialog.h"

#include <quickevent/core/og/timems.h>
#include <quickevent/core/si/punchrecord.h>
#include <quickevent/core/si/checkedcard.h>
#include <quickevent/core/si/readcard.h>

#include <qf/gui/framework/mainwindow.h>

#include <qf/core/log.h>
#include <qf/core/assert.h>
#include <qf/core/exception.h>
#include <qf/core/sql/query.h>
#include <qf/core/sql/connection.h>
#include <qf/core/sql/transaction.h>
#include <qf/core/sql/querybuilder.h>
#include <plugins/Event/src/eventplugin.h>
#include <plugins/Runs/src/runsplugin.h>
#include <QJSValue>
#include <QMetaObject>
#include <QSqlRecord>

//#define QF_TIMESCOPE_ENABLED
#include <qf/core/utils/timescope.h>

namespace qff = qf::gui::framework;
namespace qfs = qf::core::sql;
using ::PartWidget;
using qff::getPlugin;
using Event::EventPlugin;
using Runs::RunsPlugin;

namespace CardReader {

CardReaderPlugin::CardReaderPlugin(QObject *parent)
	: Super("CardReader", parent)
{
	connect(this, &CardReaderPlugin::installed, this, &CardReaderPlugin::onInstalled);
}

//QString CardReaderPlugin::settingsPrefix()
//{
//	static const QString s = CardReaderPlugin::SETTINGS_PREFIX;
//	return s;
//}

void CardReaderPlugin::onInstalled()
{
	m_cardCheckers << new CardCheckerClassicCpp(this);
	m_cardCheckers << new CardCheckerFreeOrderCpp(this);

	qff::initPluginWidget<CardReaderWidget, PartWidget>(tr("Card reader"), featureId());

	auto *racom_client = new services::RacomClient(this);
	Event::services::Service::addService(racom_client);
	auto *qr_o_punch = new services::QrOPunch(this);
	Event::services::Service::addService(qr_o_punch);

	auto core_plugin = getPlugin<Core::CorePlugin>();
	core_plugin->settingsDialog()->addPage(new CardReaderSettingsPage());
}

QQmlListProperty<CardReader::CardChecker> CardReaderPlugin::cardCheckersListProperty()
{
	/// Generally this constructor should not be used in production code, as a writable QList violates QML's memory management rules.
	/// However, this constructor can be very useful while prototyping.
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
	return QQmlListProperty<CardReader::CardChecker>(this, m_cardCheckers);
#else
	return QQmlListProperty<CardReader::CardChecker>(this, &m_cardCheckers);
#endif
}

CardChecker *CardReaderPlugin::currentCardChecker()
{
	CardReaderSettings settings;
	for (int i = 0; i < m_cardCheckers.count(); ++i) {
		if(m_cardCheckers[i]->nameId() == settings.cardCheckType())
			return m_cardCheckers[i];
	}
	QF_ASSERT(false, QString("No card checker for name '%1' installed").arg(settings.cardCheckType()), return nullptr);
	return nullptr;
}

int CardReaderPlugin::currentStageId()
{
	return getPlugin<EventPlugin>()->currentStageId();
}

int CardReaderPlugin::cardIdToSiId(int card_id)
{
	qf::core::sql::Query q = qf::core::sql::Query::fromExec("SELECT siId FROM cards WHERE id=" QF_IARG(card_id) , qf::core::Exception::Throw);
	if(q.next())
		return q.value(0).toInt();
	return 0;
}

int CardReaderPlugin::findRunId(int si_id, int si_finish_time, QString *err_msg)
{
	int run_id = 0;
	int prev_run_id = 0;
	if(err_msg)
		*err_msg = QString();
	int si_finish_time_msec = getPlugin<EventPlugin>()->msecToStageStartAM(si_finish_time);
	qf::core::sql::Query q;
	bool is_relays = getPlugin<EventPlugin>()->eventConfig()->isRelays();
	if(is_relays) {
		q.exec("SELECT id, leg, startTimeMs, finishTimeMs FROM runs WHERE siId=" QF_IARG(si_id)
			" AND isRunning"
			" ORDER BY leg DESC"
, qf::core::Exception::Throw);
	}
	else {
		int stage_no = currentStageId();
		q.exec("SELECT id, startTimeMs, finishTimeMs FROM runs WHERE stageId=" QF_IARG(stage_no)
			" AND siId=" QF_IARG(si_id)
			" AND isRunning"
			//" ORDER BY finishTimeMs"
			, qf::core::Exception::Throw);

	}
	while(q.next()) {
		run_id = q.value("id").toInt();
		int run_start = q.value("startTimeMs").toInt();
		int run_finish = q.value("finishTimeMs").toInt();
		if(si_finish_time == siut::SICard::INVALID_SI_TIME) {
			/// No finnishTime given, cannot guess between runners with duplicate SI Card
			/// used for radio punches
			return run_id;
		}
		if(run_start > si_finish_time_msec) {
			/// start in future, this run cannot have this siid
			qfInfo() << tr("skipping assign of SI: %1 to run_id: %2; start in future, this run cannot have this siid").arg(si_id).arg(run_id);
			continue;
		}
		if(run_finish == si_finish_time_msec) {
			/// reading the same Si card with no data change
			qfInfo() << tr("Multiple reads of SI: %1").arg(si_id);
		}
		if(run_finish > 0 && run_finish != si_finish_time_msec) {
			/// finnish time is different, manual assign required
			if(err_msg)
				*err_msg = tr("Multiple reads of SI: %1 with different finish time, manual assign required").arg(si_id);
			return 0;
		}
		if(prev_run_id != 0) {
			/// second possible run, give it up
			if(err_msg)
				*err_msg = tr("More competitors with SI: %1, run1 id: %2, run2 id: %3").arg(si_id).arg(prev_run_id).arg(run_id);
			return 0;
		}
		prev_run_id = run_id;
	}
	if(run_id == 0 && err_msg) {
		*err_msg = tr("Cannot find competitor with SI: %1").arg(si_id);
	}
	return run_id;
}

bool CardReaderPlugin::isCardLent(int si_id, int si_finish_time, int run_id)
{
	bool card_lent = false;
	bool card_returned = false;
	if(run_id == 0)
		run_id = findRunId(si_id, si_finish_time);
	qf::core::sql::Query q;
	if(run_id > 0) {
		q.exec("SELECT cardLent, cardReturned FROM runs WHERE id=" QF_IARG(run_id) );
		if(q.next()) {
			card_lent = q.value(0).toBool();
			card_returned = q.value(1).toBool();
		}
	}
	if(!card_lent && !card_returned) {
		q.exec("SELECT siId FROM lentcards WHERE NOT ignored AND siid=" QF_IARG(si_id) );
		if(q.next())
			card_lent = true;
	}
	return (card_lent && !card_returned);
}

quickevent::core::si::ReadCard CardReaderPlugin::readCard(int card_id)
{
	qfLogFuncFrame() << "card id:" << card_id;
	qf::core::sql::Query q;
	if(q.exec("SELECT * FROM cards WHERE id=" QF_IARG(card_id))) {
		if(q.next()) {
			quickevent::core::si::ReadCard rc(q.record());
			return rc;
		}
	}
	qfWarning() << "Cannot find card record for id:" << card_id;
	return quickevent::core::si::ReadCard();
}

quickevent::core::si::CheckedCard CardReaderPlugin::checkCard(int card_id, int run_id)
{
	qfLogFuncFrame() << "run id:" << run_id << "card id:" << card_id;
	QF_TIME_SCOPE("checkCard()");
	quickevent::core::si::ReadCard rc = readCard(card_id);
	if(!rc.isEmpty()) {
		if(run_id > 0)
			rc.setRunId(run_id);
		return checkCard(rc);
	}
	return quickevent::core::si::CheckedCard();
}

quickevent::core::si::CheckedCard CardReaderPlugin::checkCard(const quickevent::core::si::ReadCard &read_card)
{
	qfLogFuncFrame() << "run id:" << read_card.runId();
	QF_TIME_SCOPE("checkCard()");
	quickevent::core::si::CheckedCard cc;
	CardReader::CardChecker *chk = currentCardChecker();
	QF_ASSERT(chk != nullptr, "CardChecker is NULL", return quickevent::core::si::CheckedCard());
	auto *cpp_chk = qobject_cast<CardReader::CardCheckerCpp*>(chk);
	if(cpp_chk) {
		cc = cpp_chk->checkCard(read_card);
	}
	else {
		QVariant ret_val;
		QMetaObject::invokeMethod(chk, "checkCard", Qt::DirectConnection,
								  Q_RETURN_ARG(QVariant, ret_val),
								  Q_ARG(QVariant, read_card));
		auto jsv = ret_val.value<QJSValue>();
		QVariant v = jsv.toVariant();
		QVariantMap m = v.toMap();
		cc = quickevent::core::si::CheckedCard(m);
	}
	cc.setRunId(read_card.runId());
	cc.setCardNumber(read_card.cardNumber());
	qfDebug() << cc.toString();
	return cc;
}

int CardReaderPlugin::saveCardToSql(const quickevent::core::si::ReadCard &read_card)
{
	{
		// create fake punch from finish station for speaker if it doesn't exists already
		quickevent::core::si::PunchRecord punch;
		punch.setsiid(read_card.cardNumber());
		punch.setrunid(read_card.runId());
		punch.settime(read_card.finishTime());
		punch.setcode(quickevent::core::CodeDef::FINISH_PUNCH_CODE);
		int punch_id = savePunchRecordToSql(punch);
		if(punch_id > 0) {
			punch.setid(punch_id);
			getPlugin<EventPlugin>()->emitDbEvent(Event::EventPlugin::DBEVENT_PUNCH_RECEIVED, punch, true);
		}
	}
	int ret = 0;
	QStringList punches;
	for(const auto &v : read_card.punches()) {
		quickevent::core::si::ReadPunch p(v.toMap());
		punches << p.toJsonArrayString();
	}
	qf::core::sql::Query q;
	q.prepare(QStringLiteral("INSERT INTO cards (stationNumber, siId, checkTime, startTime, finishTime, punches, runId, stageId, readerConnectionId, runIdAssignError)"
							 " VALUES (:stationNumber, :siId, :checkTime, :startTime, :finishTime, :punches, :runId, :stageId, :readerConnectionId, :runIdAssignError)")
			  , qf::core::Exception::Throw);
	q.bindValue(QStringLiteral(":stationNumber"), read_card.stationNumber());
	q.bindValue(QStringLiteral(":siId"), read_card.cardNumber());
	q.bindValue(QStringLiteral(":checkTime"), read_card.checkTime());
	q.bindValue(QStringLiteral(":startTime"), read_card.startTime());
	q.bindValue(QStringLiteral(":finishTime"), read_card.finishTime());
	q.bindValue(QStringLiteral(":punches"), '[' + punches.join(", ") + ']');
	q.bindValue(QStringLiteral(":runId"), read_card.runId());
	q.bindValue(QStringLiteral(":stageId"), currentStageId());
	q.bindValue(QStringLiteral(":readerConnectionId"), qf::core::sql::Connection::defaultConnection().connectionId());
	q.bindValue(QStringLiteral(":runIdAssignError"), read_card.runIdAssignError());
	if(q.exec()) {
		ret = q.lastInsertId().toInt();
	}
	else {
		qfError() << tr("Save card ERROR: %1").arg(q.lastErrorText());
	}
	return ret;
}

int CardReaderPlugin::savePunchRecordToSql(const quickevent::core::si::PunchRecord &punch_record)
{
	//qfInfo() << "PUNCH:" << punch_record.toString();
	int ret = 0;
	quickevent::core::si::PunchRecord punch = punch_record;
	punch.setstageid(currentStageId());

	int time_msec = getPlugin<EventPlugin>()->msecToStageStartAM(punch_record.time(), punch_record.msec());
	punch.settimems(time_msec);
	qf::core::sql::Query q;
	int run_id = punch.runid();
	if(run_id > 0) {
		q.exec("SELECT startTimeMs FROM runs WHERE id=" QF_IARG(run_id), qf::core::Exception::Throw);
		if(q.next()) {
			QVariant v = q.value(0);
			if(!v.isNull()) {
				punch.setruntimems(time_msec - v.toInt());
			}
		}
	}

	int code = resolveAltCode(punch.code(), punch.stageid());

	// Check if punch isn't duplicate of existing saved punch
	// The reason is, we don't want have duplicity in punches table. Because of Speaker module, which uses this table.
	// If we don't check for duplicity on inserting in db, we need to check it on every shown in module Speaker.
	// I think it is better to resolve duplicity on write, and not on every read.
	if (q.exec("SELECT * FROM punches WHERE siId=" QF_IARG(punch.siid()) " AND code=" QF_IARG(code)
			   " AND timeMs=" QF_IARG(punch.timems()) " AND stageId=" QF_IARG(punch.stageid()))) {
		if(q.next()) {
			return 0;
		}
	}

	q.prepare(QStringLiteral("INSERT INTO punches (siId, code, time, msec, runId, stageId, timeMs, runTimeMs)"
							 " VALUES (:siId, :code, :time, :msec, :runId, :stageId, :timeMs, :runTimeMs)")
							, qf::core::Exception::Throw);
	q.bindValue(QStringLiteral(":siId"), punch.siid());
	q.bindValue(QStringLiteral(":code"), code);
	q.bindValue(QStringLiteral(":time"), punch.time());
	q.bindValue(QStringLiteral(":msec"), punch.msec());
	q.bindValue(QStringLiteral(":runId"), punch.runid());
	q.bindValue(QStringLiteral(":stageId"), punch.stageid());
	q.bindValue(QStringLiteral(":timeMs"), punch.timems());
	q.bindValue(QStringLiteral(":runTimeMs"), punch.runtimems_isset()? punch.runtimems(): QVariant());
	/// it is not possible to save punch time as date-time to be independent on start00 since it depends on start00 due to 12H time format
	if(q.exec()) {
		ret = q.lastInsertId().toInt();
	}
	else {
		qfError() << tr("Save punch record ERROR: %1").arg(q.lastErrorText());
	}
	return ret;
}

void CardReaderPlugin::updateCheckedCardValuesSql(const quickevent::core::si::CheckedCard &checked_card) noexcept(false)
{
	QF_TIME_SCOPE("updateCheckedCardValuesSql()");
	int run_id = checked_card.runId();
	if(run_id <= 0)
		QF_EXCEPTION("Card doesn't contain runId information!");
	auto cc = qf::core::sql::Connection::forName();
	qf::core::sql::Query q(cc);
	{
		QF_TIME_SCOPE("DELETE FROM runlaps");
		q.exec("DELETE FROM runlaps WHERE runId=" + QString::number(run_id), qf::core::Exception::Throw);
	}
	q.prepare(QStringLiteral("INSERT INTO runlaps (runId, position, code, stpTimeMs, lapTimeMs)"
							 " VALUES (:runId, :position, :code, :stpTimeMs, :lapTimeMs)"), qf::core::Exception::Throw);
	auto punch_list = checked_card.punches();
	if(punch_list.count()) {
		QF_TIME_SCOPE("INSERT INTO runlaps, records cnt: " + QString::number(punch_list.count()));
		int position = 0;
		for(const auto &v : punch_list) {
			position++;
			quickevent::core::si::CheckedPunch cp(v.toMap());
			//qfInfo() << run_id << position << cp;
			if(cp.stpTimeMs() > 0) {
				q.bindValue(QStringLiteral(":runId"), run_id);
				q.bindValue(QStringLiteral(":code"), cp.code());
				q.bindValue(QStringLiteral(":position"), position);
				q.bindValue(QStringLiteral(":stpTimeMs"), cp.stpTimeMs());
				q.bindValue(QStringLiteral(":lapTimeMs"), cp.lapTimeMs());
				q.exec(qf::core::Exception::Throw);
			}
		}
	}
	//bool should_be_disq = false;
	//{
	//	q.execThrow("SELECT * FROM runs WHERE id=" + QString::number(run_id));
	//	if(q.next()) {
	//		if(q.value("disqualifiedByOrganizer").toBool()) should_be_disq = true;
	//		if(q.value("notStart").toBool()) should_be_disq = true;
	//		if(q.value("notFinish").toBool()) should_be_disq = true;
	//	}
	//}
	QString qs = "UPDATE runs SET checkTimeMs=:checkTimeMs, timeMs=:timeMs, finishTimeMs=:finishTimeMs, penaltyTimeMs=NULL,"
				 " misPunch=:misPunch, badCheck=:badCheck,"
				 " notStart=:notStart"
				 " WHERE id=" + QString::number(run_id);
	q.prepare(qs, qf::core::Exception::Throw);
	q.bindValue(QStringLiteral(":checkTimeMs"), checked_card.checkTimeMs());
	q.bindValue(QStringLiteral(":timeMs"), checked_card.timeMs());
	q.bindValue(QStringLiteral(":finishTimeMs"), checked_card.finishTimeMs());
	q.bindValue(QStringLiteral(":misPunch"), checked_card.isMisPunch());
	q.bindValue(QStringLiteral(":badCheck"), checked_card.isBadCheck());
	q.bindValue(QStringLiteral(":notStart"), false);
	q.exec(qf::core::Exception::Throw);
	if(q.numRowsAffected() != 1) {
		qfError() << "Update runs error, query:" << qs;
	}
}

void CardReaderPlugin::updateCardToRunAssignmentInPunches(int stage_id, int card_id, int run_id)
{
	qfLogFuncFrame();
	int si_id = cardIdToSiId(card_id);
	if(si_id > 0) {
		QString qs = "UPDATE punches SET runId=" QF_IARG(run_id)
					" WHERE stageId=" QF_IARG(stage_id)
					" AND siId=" QF_IARG(si_id)
					" AND (runId IS NULL OR runId=0)";
		qfDebug() << qs;
		qf::core::sql::Query q;
		q.execThrow(qs);
	}
}

bool CardReaderPlugin::saveCardAssignedRunnerIdSql(int card_id, int run_id)
{
	QF_TIME_SCOPE("saveCardAssignedRunnerIdSql()");
	auto cc = qf::core::sql::Connection::forName();
	qf::core::sql::Query q(cc);
	QString now = QStringLiteral("now()");
	if(cc.driverName().endsWith("SQLITE", Qt::CaseInsensitive))
		now = QStringLiteral("CURRENT_TIMESTAMP");
	bool ret = q.exec("UPDATE cards SET runId=" QF_IARG(run_id) ", runIdAssignTS=" + now + " WHERE id=" QF_IARG(card_id), !qf::core::Exception::Throw);
	return ret;
}

bool CardReaderPlugin::reloadTimesFromCard(int card_id, int run_id, bool in_transaction)
{
	qfLogFuncFrame() << "card id:" << run_id;
	QF_TIME_SCOPE("reloadTimesFromCard()");
	if(card_id == 0)
		return false;
	if(run_id == 0) {
		qf::core::sql::Query q;
		if(q.exec("SELECT runId FROM cards WHERE id=" QF_IARG(card_id))) {
			if(q.next()) {
				run_id = q.value(0).toInt();
			}
			else {
				qfWarning() << "Cannot find card record for id:" << card_id;
			}
		}
	}
	if(run_id == 0) {
		qfWarning() << "Cannot find runs id for card id:" << card_id;
		return false;
	}
	if(in_transaction) {
		try {
			qf::core::sql::Transaction transaction;
			processCardToRunAssignment(card_id, run_id);
			transaction.commit();
			return true;
		}
		catch (const qf::core::Exception &e) {
			qfError() << "reloadTimesFromCard ERROR:" << e.message();
		}
	}
	else {
		return processCardToRunAssignment(card_id, run_id);
	}
	return false;
}

void CardReaderPlugin::assignCardToRun(int card_id, int run_id)
{
	updateCardToRunAssignmentInPunches(currentStageId(), card_id, run_id);
	saveCardAssignedRunnerIdSql(card_id, run_id);
	processCardToRunAssignment(card_id, run_id);
}

void CardReaderPlugin::setStartTime(int relay_id, int leg, int start_time) {
	qf::core::sql::Query q;
	q.execThrow("UPDATE runs SET startTimeMs=" + QString::number(start_time)
		   + " WHERE relayId=" + QString::number(relay_id)
		   + " AND leg=" + QString::number(leg)
		   + " AND COALESCE(startTimeMs, 0)=0");
}

bool CardReaderPlugin::processCardToRunAssignment(int card_id, int run_id)
{
	qfLogFuncFrame();
	bool is_relays = getPlugin<EventPlugin>()->eventConfig()->isRelays();
	if(is_relays) {
		qf::core::sql::Query q;
		q.execThrow("SELECT relayId, leg, startTimeMs FROM runs WHERE id=" + QString::number(run_id));
		if(!q.next()) {
			qfError() << "Run not found, id:" << run_id;
			return false;
		}
		int relay_id = q.value(0).toInt();
		int leg = q.value(1).toInt();
		QVariant start_time = q.value(2);
		if(start_time.isNull() && leg > 1) {
			/// if start time not set, take start time from previous leg
			q.execThrow("SELECT finishTimeMs FROM runs"
					 " WHERE relayId=" + QString::number(relay_id)
				   + " AND leg=" + QString::number(leg-1));
			if(q.next()) {
				int prev_finish_time = q.value(0).toInt();
				if(prev_finish_time > 0) {
					setStartTime(relay_id, leg, prev_finish_time);
				}
			}
		}
		quickevent::core::si::CheckedCard checked_card = checkCard(card_id, run_id);
		//qfDebug() << checked_card.toString();
		updateCheckedCardValuesSql(checked_card);
		getPlugin<EventPlugin>()->emitDbEvent(Event::EventPlugin::DBEVENT_CARD_PROCESSED_AND_ASSIGNED, checked_card, true);

		q.execThrow("SELECT id, startTimeMs, finishTimeMs FROM runs"
				 " WHERE relayId=" + QString::number(relay_id)
			   + " AND leg=" + QString::number(leg+1));
		if(q.next()) {
			int next_leg_run_id = q.value(0).toInt();
			QVariant next_leg_start_time = q.value(1);
			int next_leg_finish_time = q.value(2).toInt();
			if(next_leg_start_time.isNull()) {
				// if next leg is finished and has not start time set, proces it too
				// This covers cases when next leg is read-out before this one
				if(next_leg_finish_time > 0) {
					int next_leg_card_id = getPlugin<RunsPlugin>()->cardForRun(next_leg_run_id);
					processCardToRunAssignment(next_leg_card_id, next_leg_run_id);
				}
				// set start time for next leg, and publish the change
				else {
					int new_next_leg_start_time = checked_card.finishTimeMs();
					setStartTime(relay_id, leg + 1, new_next_leg_start_time);
					int competitor_id = getPlugin<RunsPlugin>()->competitorForRun(next_leg_run_id);
					getPlugin<EventPlugin>()->emitDbEvent(Event::EventPlugin::DBEVENT_COMPETITOR_EDITED, competitor_id);
					QVariantList param {
						next_leg_run_id,
						QVariantMap {
							{"runs.startTimeMs", new_next_leg_start_time},
						}
					};
					getPlugin<EventPlugin>()->emitDbEvent(Event::EventPlugin::DBEVENT_RUN_CHANGED, param);
				}
			}
		}
	}
	else {
		quickevent::core::si::CheckedCard checked_card = checkCard(card_id, run_id);
		updateCheckedCardValuesSql(checked_card);
		getPlugin<EventPlugin>()->emitDbEvent(Event::EventPlugin::DBEVENT_CARD_PROCESSED_AND_ASSIGNED, checked_card, true);
	}
	return true;
}

int CardReaderPlugin::resolveAltCode(int maybe_alt_code, int stage_id)
{
	int resolved_code = 0;
	qfs::QueryBuilder qb;
	qb.select2("codes", "code")
			.from("codes")
			.joinRestricted("codes.id", "coursecodes.codeId", "codes.altCode=" QF_IARG(maybe_alt_code), qfs::QueryBuilder::INNER_JOIN)
			.joinRestricted("coursecodes.courseId", "classdefs.courseId", "classdefs.stageId=" QF_IARG(stage_id), qfs::QueryBuilder::INNER_JOIN);
	qfDebug() << qb.toString();
	qfs::Query q;
	q.exec(qb, qf::core::Exception::Throw);
	while (q.next()) {
		if(resolved_code) {
			qfError() << "duplicate alt code" << maybe_alt_code << "in stage:" << stage_id;
			return maybe_alt_code;
		}
		resolved_code = q.value(0).toInt();
	}
	if(resolved_code > 0)
		qfDebug() << "alt code:" << maybe_alt_code << "resolved to:" << resolved_code;
	return (resolved_code > 0)? resolved_code: maybe_alt_code;
}

}
