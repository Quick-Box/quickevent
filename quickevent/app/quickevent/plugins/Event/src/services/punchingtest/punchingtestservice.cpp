#include "punchingtestservice.h"
#include "punchingtestservicewidget.h"

#include "../../eventplugin.h"

#include <plugins/CardReader/src/cardreaderplugin.h>
#include <plugins/Runs/src/runsplugin.h>

#include <siut/sicard.h>
#include <siut/sipunch.h>
#include <siut/sitask.h>

#include <quickevent/core/codedef.h>
#include <quickevent/core/coursedef.h>

#include <qf/gui/framework/mainwindow.h>
#include <qf/core/sql/query.h>
#include <qf/core/log.h>

#include <QMessageBox>
#include <QTimer>
#include <QRandomGenerator>

#include <algorithm>

using qf::gui::framework::getPlugin;

namespace Event::services {

PunchingTestService::PunchingTestService(QObject *parent)
	: Super(serviceName(), parent)
{
}

QString PunchingTestService::serviceName()
{
	return QStringLiteral("PunchingTest");
}

QString PunchingTestService::serviceDisplayName() const
{
	return tr("Punching Test");
}

void PunchingTestService::run()
{
	// Asked also on auto start, the service must never resume silently on a real event.
	if (!isRunning()) {
		QMessageBox mbx(QMessageBox::Warning,
						serviceDisplayName(),
						tr("Do you really want to start the %1 service?").arg(serviceDisplayName()),
						QMessageBox::Yes | QMessageBox::No,
						qf::gui::framework::MainWindow::frameWork());
		mbx.setInformativeText(tr("The service generates test readout data into the currently open event."
								  " It is dedicated for testing and development purposes only"
								  " and must not be started for a real event."));
		mbx.setDefaultButton(QMessageBox::No);
		if (mbx.exec() != QMessageBox::Yes) {
			return;
		}
	}

	PunchingTestServiceSettings ss = settings();
	int interval_sec = ss.punchInterval();
	if (interval_sec <= 0)
		interval_sec = 10;

	if (!m_timer) {
		m_timer = new QTimer(this);
		connect(m_timer, &QTimer::timeout, this, &PunchingTestService::onTimerTick);
	}
	m_timer->start(interval_sec * 1000);
	setStatusMessage(tr("Running, interval: %1 s").arg(interval_sec));
	Super::run();
}

void PunchingTestService::stop()
{
	if (m_timer)
		m_timer->stop();
	Super::stop();
}

int PunchingTestService::shameStartTimeMs(int stage_id)
{
	// The shame start is announced for the whole event once the top 3 positions are decided
	// in every relay class. Legs with no SI assigned are left out of the counting, such a
	// relay can never finish and would block the announcement forever.
	qf::core::sql::Query q;
	if (!q.exec(QStringLiteral(
			"SELECT relays.classId, MAX(runs.finishTimeMs) AS lastFinishMs, COUNT(*) AS legCount,"
			" SUM(CASE WHEN COALESCE(runs.finishTimeMs, 0)>0 THEN 1 ELSE 0 END) AS finishedCount"
			" FROM runs"
			" JOIN relays ON relays.id=runs.relayId"
			" WHERE runs.stageId=%1"
			" AND runs.isRunning"
			" AND relays.isRunning"
			" AND runs.siId>0"
			" GROUP BY relays.classId, runs.relayId").arg(stage_id))) {
		qfWarning() << "PunchingTestService: cannot query relay finish times";
		return 0;
	}

	QMap<int, int> relay_counts; // class id -> number of relays
	QMap<int, QList<int>> finish_times; // class id -> finish times of the finished relays
	while (q.next()) {
		int class_id = q.value(QStringLiteral("classId")).toInt();
		relay_counts[class_id]++;
		if (q.value(QStringLiteral("legCount")).toInt() == q.value(QStringLiteral("finishedCount")).toInt())
			finish_times[class_id] << q.value(QStringLiteral("lastFinishMs")).toInt();
	}

	int shame_start_ms = 0;
	for (auto it = relay_counts.cbegin(); it != relay_counts.cend(); ++it) {
		// Classes with less than 3 relays are decided by their last finisher
		int needed = qMin(3, it.value());
		QList<int> &times = finish_times[it.key()];
		if (times.count() < needed)
			return 0;
		std::sort(times.begin(), times.end());
		shame_start_ms = qMax(shame_start_ms, times[needed - 1]);
	}
	if (shame_start_ms == 0)
		return 0;
	return shame_start_ms + settings().shameStartOffsetMin() * 60 * 1000;
}

void PunchingTestService::onTimerTick()
{
	auto *event_plugin = getPlugin<EventPlugin>();
	if (!event_plugin->isEventOpen()) {
		setStatusMessage(tr("No event open"));
		return;
	}
	int stage_id = event_plugin->currentStageId();
	bool is_relays = event_plugin->eventConfig().isRelays();
	auto &rng = *QRandomGenerator::global();
	const PunchingTestServiceSettings ss = settings();

	// A relay leg starts at the handover, that is at the previous leg finish, so the legs
	// must be generated in the leg order.
	QString relay_cols = is_relays
		? QStringLiteral(", COALESCE(runs.leg, 1) AS leg"
			", (SELECT COALESCE(MAX(prev.finishTimeMs), 0) FROM runs prev"
			" WHERE prev.relayId=runs.relayId AND prev.leg=runs.leg-1) AS prevLegFinishMs")
		: QString();
	int shame_start_ms = (is_relays && ss.shameStartEnabled())? shameStartTimeMs(stage_id): 0;

	qf::core::sql::Query q;
	// Classes without start interval have a free (or mass) start, their competitors punch
	// the start unit. Competitors of classes with drawn start times start on a signal and
	// have no start punch in the card.
	if (!q.exec(QStringLiteral(
			"SELECT runs.id, runs.siId, runs.startTimeMs,"
			" COALESCE(classdefs.startIntervalMin, 0)=0 AS isFreeStart,"
			" COALESCE(competitors.lastName, '') || ' ' || COALESCE(competitors.firstName, '') AS competitorName"
			"%2"
			" FROM runs"
			" LEFT JOIN competitors ON competitors.id=runs.competitorId"
			" LEFT JOIN relays ON relays.id=runs.relayId"
			" LEFT JOIN classdefs ON (classdefs.classId=competitors.classId OR classdefs.classId=relays.classId)"
			" AND classdefs.stageId=runs.stageId"
			" WHERE runs.stageId=%1"
			" AND runs.isRunning"
			" AND runs.siId>0"
			" AND (runs.finishTimeMs IS NULL OR runs.finishTimeMs=0)").arg(stage_id).arg(relay_cols))) {
		qfWarning() << "PunchingTestService: cannot query runs";
		return;
	}

	struct Candidate {
		int run_id;
		int si_id;
		int start_time_ms; // ms relative to stage start
		bool is_free_start;
		QString competitor_name;
	};
	QList<Candidate> candidates;
	int waiting_for_handover = 0;
	while (q.next()) {
		Candidate cand;
		cand.run_id = q.value(QStringLiteral("id")).toInt();
		cand.si_id = q.value(QStringLiteral("siId")).toInt();
		cand.start_time_ms = q.value(QStringLiteral("startTimeMs")).toInt();
		// In relays the leg start time is taken over from the previous leg finish (handover),
		// a start punch in the card would overwrite it, see CardReaderPlugin::processCardToRunAssignment().
		cand.is_free_start = q.value(QStringLiteral("isFreeStart")).toBool() && !is_relays;
		cand.competitor_name = q.value(QStringLiteral("competitorName")).toString().trimmed();
		if (is_relays && q.value(QStringLiteral("leg")).toInt() > 1) {
			// Handover, or the shame start for the legs whose previous leg is still on the course
			int prev_leg_finish_ms = q.value(QStringLiteral("prevLegFinishMs")).toInt();
			cand.start_time_ms = (prev_leg_finish_ms > 0)? prev_leg_finish_ms: shame_start_ms;
			if (cand.start_time_ms <= 0) {
				waiting_for_handover++;
				continue;
			}
		}
		candidates << cand;
	}

	if (candidates.isEmpty()) {
		setStatusMessage(waiting_for_handover > 0
						 ? tr("Waiting for handover, %1 legs").arg(waiting_for_handover)
						 : tr("No eligible runners left"));
		return;
	}

	int ix = static_cast<int>(rng.bounded(static_cast<quint32>(candidates.size())));
	const Candidate &cand = candidates[ix];
	int run_id = cand.run_id;
	int si_id = cand.si_id;
	int start_time_ms = cand.start_time_ms;
	bool is_free_start = cand.is_free_start;
	QString competitor_name = cand.competitor_name;

	auto *runs_plugin = getPlugin<Runs::RunsPlugin>();
	quickevent::core::CourseDef course = runs_plugin->courseCodesForRunId(run_id);
	QVariantList codes = course.codes();

	int stage_start_ms = event_plugin->stageStartMsec(stage_id);

	// Relay first legs have a mass start in waves, their start time is always known,
	// 0 means the stage start.
	if (start_time_ms <= 0 && !is_relays) {
		// Runner has no drawn start — place randomly within the past 60 minutes
		start_time_ms = static_cast<int>(rng.bounded(60U * 60U * 1000U));
	}

	int abs_start_ms = stage_start_ms + start_time_ms;
	// Finish 20..30 minutes after start
	int running_ms = (20 * 60 + static_cast<int>(rng.bounded(10U * 60U))) * 1000;
	int abs_finish_ms = abs_start_ms + running_ms;

	// SI card times are in seconds within 12-hour AM window (0..43199)
	static constexpr int SI_HALF_DAY_SEC = 12 * 3600;
	auto toSiSec = [](int abs_ms) {
		// Use positive modulo to handle pre-midnight edge cases
		return ((abs_ms / 1000) % SI_HALF_DAY_SEC + SI_HALF_DAY_SEC) % SI_HALF_DAY_SEC;
	};

	// Start punch exists only in classes with a free start, otherwise the card start time
	// must stay empty, so that the drawn start time in runs is not overwritten.
	int si_start_sec = is_free_start ? toSiSec(abs_start_ms) : siut::SICard::INVALID_SI_TIME;
	int si_finish_sec = toSiSec(abs_finish_ms);

	// Original SI and name are read before the swap below, they tell the operator whom the
	// punches belong to on a manual assign.
	QString generated_test_data_note = tr("TEST");
	if (rng.bounded(static_cast<quint32>(ss.unknownCardRate())) == 0) {
		generated_test_data_note = tr("TEST, data for: %1, SI %2").arg(competitor_name).arg(si_id);
		si_id = 1000000 + static_cast<int>(rng.bounded(8000000U));
	}

	if (rng.bounded(static_cast<quint32>(ss.missingStartRate())) == 0)
		si_start_sec = siut::SICard::INVALID_SI_TIME;

	if (rng.bounded(static_cast<quint32>(ss.missingFinishRate())) == 0)
		si_finish_sec = siut::SICard::INVALID_SI_TIME;

	// Check time: respect the event's "Card check" max-advance setting.
	// When enabled: 1/badCheckRate chance the runner checked too early (triggers bad-check).
	// When disabled: natural 1–2 minute window, no bad-check possible.
	int check_offset_sec;
	if (auto cfg = event_plugin->appDbConfig().eventConfig().maximumCardCheckAdvanceSec(); cfg.has_value()) {
		int max_sec = cfg.value();
		if (rng.bounded(static_cast<quint32>(ss.badCheckRate())) == 0) {
			// Bad check: checked too early — [max+1, max*3] seconds before start
			check_offset_sec = max_sec + 1 + static_cast<int>(rng.bounded(static_cast<quint32>(max_sec * 2)));
		} else {
			check_offset_sec = 1 + static_cast<int>(rng.bounded(static_cast<quint32>(max_sec)));
		}
	} else {
		check_offset_sec = 60 + static_cast<int>(rng.bounded(60U)); // 1–2 minutes
	}
	int si_check_sec = toSiSec(abs_start_ms - check_offset_sec * 1000);

	quickevent::core::CodeDef start_cd = course.startCode();
	quickevent::core::CodeDef finish_cd = course.finishCode();
	int n_controls = codes.size();

	// Build per-leg distances: [start→ctrl0, ctrl0→ctrl1, …, ctrl[n-1]→finish]
	// Fall back to unit weights when GPS coordinates are absent.
	double prev_lat = start_cd.latitude(), prev_lon = start_cd.longitude();
	bool has_coords = (prev_lat != 0.0 || prev_lon != 0.0);

	QVector<double> leg_dist;
	leg_dist.reserve(n_controls + 1);
	for (int k = 0; k < n_controls; ++k) {
		quickevent::core::CodeDef cd(codes[k].toMap());
		if (has_coords) {
			double clat = cd.latitude(), clon = cd.longitude();
			leg_dist << Runs::RunsPlugin::latlng_distance(prev_lat, prev_lon, clat, clon);
			prev_lat = clat;
			prev_lon = clon;
		} else {
			leg_dist << 1.0;
		}
	}
	if (has_coords)
		leg_dist << Runs::RunsPlugin::latlng_distance(prev_lat, prev_lon, finish_cd.latitude(), finish_cd.longitude());
	else
		leg_dist << 1.0;

	// Noisy weights: leg_distance × U[0.8, 1.2] — simulates uneven terrain/navigation
	QVector<double> weights;
	weights.reserve(leg_dist.size());
	double total_weight = 0.0;
	for (double d : leg_dist) {
		double w = (d > 0.0 ? d : 1.0) * (0.8 + rng.generateDouble() * 0.4);
		weights << w;
		total_weight += w;
	}

	siut::SICard::PunchList punches;
	double cumulative_w = 0.0;
	for (int k = 0; k < n_controls; ++k) {
		cumulative_w += weights[k];
		if (rng.bounded(static_cast<quint32>(ss.mispunchRate())) == 0)
			continue;
		quickevent::core::CodeDef cd(codes[k].toMap());
		double t = cumulative_w / total_weight;
		int ctrl_ms = abs_start_ms + static_cast<int>(t * (abs_finish_ms - abs_start_ms));
		int ctrl_sec = (ctrl_ms / 1000) % SI_HALF_DAY_SEC;
		punches << siut::SIPunch(cd.code(), ctrl_sec);
	}

	// Extra punch: a wrong control inserted at a chronologically correct position
	if (rng.bounded(static_cast<quint32>(ss.extraPunchRate())) == 0) {
		QSet<int> course_codes;
		for (const auto &v : punches)
			course_codes.insert(v.code);
		int extra_code = 0;
		const int code_range = quickevent::core::CodeDef::PUNCH_CODE_MAX
			- quickevent::core::CodeDef::PUNCH_CODE_MIN + 1;
		for (int attempt = 0; attempt < 20 && extra_code == 0; ++attempt) {
			int candidate = quickevent::core::CodeDef::PUNCH_CODE_MIN
				+ static_cast<int>(rng.bounded(static_cast<quint32>(code_range)));
			if (!course_codes.contains(candidate))
				extra_code = candidate;
		}
		if (extra_code > 0) {
			double t = rng.generateDouble();
			int extra_ms = abs_start_ms + static_cast<int>(t * (abs_finish_ms - abs_start_ms));
			int extra_sec = (extra_ms / 1000) % SI_HALF_DAY_SEC;
			siut::SIPunch extra_punch(extra_code, extra_sec);
			// Insert at the position that keeps the list in chronological order
			int insert_at = punches.size();
			for (int i = 0; i < punches.size(); ++i) {
				if (extra_sec < punches[i].time) {
					insert_at = i;
					break;
				}
			}
			punches.insert(insert_at, extra_punch);
		}
	}

	siut::SICard card;
	card.cardNumber = si_id;
	card.checkTime  = si_check_sec;
	card.startTime  = si_start_sec;
	card.finishTime = si_finish_sec;
	card.punches    = punches;
	card.generatedTestDataNote = generated_test_data_note;

	setStatusMessage(tr("Card SI %1, %2 controls").arg(si_id).arg(punches.size()));

	getPlugin<CardReader::CardReaderPlugin>()->emitSiTaskFinished(
		static_cast<int>(siut::SiTask::Type::CardRead),
		card.toVariantMap());
}

qf::gui::framework::DialogWidget *PunchingTestService::createDetailWidget()
{
	return new PunchingTestServiceWidget();
}

} // namespace Event::services
