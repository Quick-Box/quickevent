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

#include <QTimer>
#include <QRandomGenerator>

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

void PunchingTestService::onTimerTick()
{
	auto *event_plugin = getPlugin<EventPlugin>();
	if (!event_plugin->isEventOpen()) {
		setStatusMessage(tr("No event open"));
		return;
	}
	int stage_id = event_plugin->currentStageId();

	qf::core::sql::Query q;
	if (!q.exec(QStringLiteral(
			"SELECT id, siId, startTimeMs FROM runs"
			" WHERE stageId=%1"
			" AND isRunning"
			" AND siId>0"
			" AND (finishTimeMs IS NULL OR finishTimeMs=0)").arg(stage_id))) {
		qfWarning() << "PunchingTestService: cannot query runs";
		return;
	}

	QList<QVariantList> candidates;
	while (q.next())
		candidates << QVariantList{q.value(0), q.value(1), q.value(2)};

	if (candidates.isEmpty()) {
		setStatusMessage(tr("No eligible runners left"));
		return;
	}

	int ix = static_cast<int>(QRandomGenerator::global()->bounded(static_cast<quint32>(candidates.size())));
	const auto &cand = candidates[ix];
	int run_id = cand[0].toInt();
	int si_id = cand[1].toInt();
	int start_time_ms = cand[2].toInt(); // ms relative to stage start

	auto *runs_plugin = getPlugin<Runs::RunsPlugin>();
	quickevent::core::CourseDef course = runs_plugin->courseCodesForRunId(run_id);
	QVariantList codes = course.codes();

	int stage_start_ms = event_plugin->stageStartMsec(stage_id);

	if (start_time_ms <= 0) {
		// Runner has no drawn start — place randomly within the past 60 minutes
		start_time_ms = static_cast<int>(QRandomGenerator::global()->bounded(60U * 60U * 1000U));
	}

	int abs_start_ms = stage_start_ms + start_time_ms;
	// Finish 20..30 minutes after start
	int running_ms = (20 * 60 + static_cast<int>(QRandomGenerator::global()->bounded(10U * 60U))) * 1000;
	int abs_finish_ms = abs_start_ms + running_ms;

	// SI card times are in seconds within 12-hour AM window (0..43199)
	static constexpr int SI_HALF_DAY_SEC = 12 * 3600;
	auto toSiSec = [](int abs_ms) {
		// Use positive modulo to handle pre-midnight edge cases
		return ((abs_ms / 1000) % SI_HALF_DAY_SEC + SI_HALF_DAY_SEC) % SI_HALF_DAY_SEC;
	};

	int si_start_sec = toSiSec(abs_start_ms);
	int si_finish_sec = toSiSec(abs_finish_ms);

	auto &rng = *QRandomGenerator::global();
	const PunchingTestServiceSettings ss = settings();

	if (rng.bounded(static_cast<quint32>(ss.unknownCardRate())) == 0)
		si_id = 1000000 + static_cast<int>(rng.bounded(8000000U));

	if (rng.bounded(static_cast<quint32>(ss.missingStartRate())) == 0)
		si_start_sec = siut::SICard::INVALID_SI_TIME;

	if (rng.bounded(static_cast<quint32>(ss.missingFinishRate())) == 0)
		si_finish_sec = siut::SICard::INVALID_SI_TIME;

	// Check time: respect the event's "Card check" max-advance setting.
	// When enabled: 1/badCheckRate chance the runner checked too early (triggers bad-check).
	// When disabled: natural 1–2 minute window, no bad-check possible.
	int check_offset_sec;
	if (auto cfg = event_plugin->eventConfig()->maximumCardCheckAdvanceSec(); cfg.has_value()) {
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

	QVariantList punches;
	double cumulative_w = 0.0;
	for (int k = 0; k < n_controls; ++k) {
		cumulative_w += weights[k];
		if (rng.bounded(static_cast<quint32>(ss.mispunchRate())) == 0)
			continue;
		quickevent::core::CodeDef cd(codes[k].toMap());
		double t = cumulative_w / total_weight;
		int ctrl_ms = abs_start_ms + static_cast<int>(t * (abs_finish_ms - abs_start_ms));
		int ctrl_sec = (ctrl_ms / 1000) % SI_HALF_DAY_SEC;
		siut::SIPunch punch(cd.code(), ctrl_sec);
		punches << QVariant(static_cast<QVariantMap>(punch));
	}

	// Extra punch: a wrong control inserted at a chronologically correct position
	if (rng.bounded(static_cast<quint32>(ss.extraPunchRate())) == 0) {
		QSet<int> course_codes;
		for (const auto &v : punches)
			course_codes.insert(siut::SIPunch(v.toMap()).code());
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
				if (extra_sec < siut::SIPunch(punches[i].toMap()).time()) {
					insert_at = i;
					break;
				}
			}
			punches.insert(insert_at, QVariant(static_cast<QVariantMap>(extra_punch)));
		}
	}

	siut::SICard card;
	card.setCardNumber(si_id);
	card.setCheckTime(si_check_sec);
	card.setStartTime(si_start_sec);
	card.setFinishTime(si_finish_sec);
	card.setPunches(punches);

	setStatusMessage(tr("Card SI %1, %2 controls").arg(si_id).arg(punches.size()));

	getPlugin<CardReader::CardReaderPlugin>()->emitSiTaskFinished(
		static_cast<int>(siut::SiTask::Type::CardRead),
		QVariant(static_cast<QVariantMap>(card)));
}

qf::gui::framework::DialogWidget *PunchingTestService::createDetailWidget()
{
	return new PunchingTestServiceWidget();
}

} // namespace Event::services
