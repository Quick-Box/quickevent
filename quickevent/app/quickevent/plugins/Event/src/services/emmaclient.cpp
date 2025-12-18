#include "emmaclient.h"
#include "emmaclientwidget.h"

#include "../eventplugin.h"

#include <quickevent/core/si/checkedcard.h>
#include <quickevent/core/runstatus.h>

#include <qf/gui/framework/mainwindow.h>
#include <qf/gui/dialogs/dialog.h>

#include <qf/core/log.h>
#include <qf/core/assert.h>
#include <qf/core/sql/querybuilder.h>
#include <qf/core/sql/query.h>
#include <qf/core/sql/connection.h>
#include <plugins/Runs/src/runsplugin.h>
#include <plugins/Relays/src/relaysplugin.h>

#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>

namespace qfs = qf::core::sql;
using qf::gui::framework::getPlugin;
using Event::EventPlugin;
using Relays::RelaysPlugin;
using Runs::RunsPlugin;


namespace Event::services {

constexpr int HR_12_MSEC = 12 * 60 * 60 * 1000;
constexpr int INVALID_SI_TIME = 61166; // 0xEEEE

EmmaClient::EmmaClient(QObject *parent)
	: Super(EmmaClient::serviceName(), parent)
{
	connect(getPlugin<EventPlugin>(), &Event::EventPlugin::dbEventNotify, this, &EmmaClient::onDbEventNotify, Qt::QueuedConnection);

	m_exportTimer = new QTimer(this);
	connect(m_exportTimer, &QTimer::timeout, this, &EmmaClient::onExportTimerTimeOut);
	connect(this, &EmmaClient::settingsChanged, this, &EmmaClient::init, Qt::QueuedConnection);

}

QString EmmaClient::serviceName()
{
	return QStringLiteral("EmmaClient");
}

void EmmaClient::run() {
	Super::run();
	// run once immediately without waiting for first timer expiration
	if(settings().exportIntervalSec() > 0) {
		onExportTimerTimeOut();
	}
	m_exportTimer->start();
}

void EmmaClient::stop() {
	Super::stop();
	m_exportTimer->stop();
}

void EmmaClient::exportRadioCodesRacomTxt()
{
	EmmaClientSettings ss = settings();
	QString export_dir = ss.exportDir();
	if(!createExportDir()) {
		return;
	}
	QFile f_splitnames(export_dir + '/' + ss.fileNameBase() + ".splitnames.txt");
	if(!f_splitnames.open(QFile::WriteOnly)) {
		qfError() << "Canot open file:" << f_splitnames.fileName() << "for writing.";
		return;
	}
	qfInfo() << "EmmaClient: exporting code names to" << f_splitnames.fileName();
	QFile f_splitcodes(ss.exportDir() + '/' + ss.fileNameBase() + ".splitcodes.txt");
	if(!f_splitcodes.open(QFile::WriteOnly)) {
		qfError() << "Canot open file:" << f_splitcodes.fileName() << "for writing.";
		return;
	}
	qfInfo() << "EmmaClient: exporting codes to" << f_splitcodes.fileName();

	QTextStream ts_names(&f_splitnames);
	QTextStream ts_codes(&f_splitcodes);

	int current_stage = getPlugin<EventPlugin>()->currentStageId();
	bool is_relays = getPlugin<EventPlugin>()->eventConfig()->isRelays();

	if (is_relays) {
		qfs::QueryBuilder qb_classes;
		qb_classes.select2("classes", "name")
				.select2("classdefs", "relayStartNumber, relayLegCount")
				.from("classes")
				.joinRestricted("classes.id", "classdefs.classId", "classdefs.stageId=" + QString::number(current_stage))
				.orderBy("classes.name");
		qfs::Query q1;
		q1.execThrow(qb_classes.toString());
		while(q1.next()) {
			int relayStartNumber = q1.value("relayStartNumber").toInt();
			int relayLegCount = q1.value("relayLegCount").toInt();
			for (int leg = 1; leg <= relayLegCount; leg++)
			{
				qfs::QueryBuilder qb_codes;
				qb_codes.select2("codes", "*")
						.from("coursecodes, courses, codes")
						.where("coursecodes.codeId=codes.id")
						.where("coursecodes.courseId=courses.id")
						.where("courses.name=" + QString("'%1.%2'").arg(relayStartNumber).arg(leg))
						.where("codes.radio")
						.orderBy("coursecodes.position");
//				qfInfo() << qb_codes.toString();

				QString class_name = q1.value("classes.name").toString();
				class_name.remove(" ");
				class_name += QString("_%1").arg(leg);
				QVector<int> codes;
				qfs::Query q2;
				q2.execThrow(qb_codes.toString());
				while(q2.next()) {
					int code = q2.value("codes.code").toInt();
					codes << code;
				}

				ts_names << class_name;
				ts_codes << class_name;
				if(!codes.isEmpty()) {
					for(auto &code : codes) {
						ts_names << ' ' << QStringLiteral("cn%1").arg(code);
						ts_codes << ' ' << code;
					}
				}
				ts_names << " finish\n";
				ts_codes << ' ' << 2 << '\n';
			}
		}
	}
	else
	{
		qfs::QueryBuilder qb_classes;
		qb_classes.select2("classes", "name")
				.select2("classdefs", "courseId")
				.from("classes")
				.joinRestricted("classes.id", "classdefs.classId", "classdefs.stageId=" + QString::number(current_stage))
				.orderBy("classes.name");
		qfs::Query q1;
		q1.execThrow(qb_classes.toString());
		while(q1.next()) {
			int course_id = q1.value("courseId").toInt();
			qfs::QueryBuilder qb_codes;
			qb_codes.select2("codes", "*")
					//.select2("coursecodes", "position")
					.from("coursecodes")
					.joinRestricted("coursecodes.codeId", "codes.id", "codes.radio", qfs::QueryBuilder::INNER_JOIN)
					.where("coursecodes.courseId=" + QString::number(course_id))
					.orderBy("coursecodes.position");
//			qfInfo() << qb_codes.toString();

			QString class_name = q1.value("classes.name").toString();
			class_name.remove(" ");
			QVector<int> codes;
			qfs::Query q2;
			q2.execThrow(qb_codes.toString());
			while(q2.next()) {
				int code = q2.value("codes.code").toInt();
				codes << code;
			}

			ts_names << class_name;
			ts_codes << class_name;
			if(!codes.isEmpty()) {
				for(auto &code : codes) {
					ts_names << ' ' << QStringLiteral("cn%1").arg(code);
					ts_codes << ' ' << code;
				}
			}
			ts_names << " finish\n";
			ts_codes << ' ' << 2 << '\n';
		}
	}
}

void EmmaClient::exportResultsIofXml3()
{
	if(!createExportDir())
		return;
	EmmaClientSettings ss = settings();
	QString export_dir = ss.exportDir();
	QString file_name = export_dir + '/' + ss.fileNameBase() + ".results.xml";
	int current_stage = getPlugin<EventPlugin>()->currentStageId();
	bool is_relays = getPlugin<EventPlugin>()->eventConfig()->isRelays();

	QString str = is_relays
			? getPlugin<RelaysPlugin>()->resultsIofXml30()
			: getPlugin<RunsPlugin>()->resultsIofXml30Stage(current_stage);

	QFile f(file_name);
	if(f.open(QFile::WriteOnly)) {
		f.write(str.toUtf8());
	}
}

void EmmaClient::exportStartListIofXml3()
{
	if(!createExportDir())
		return;
	EmmaClientSettings ss = settings();
	QString export_dir = ss.exportDir();
	QString file_name = export_dir + '/' + ss.fileNameBase() + ".startlist.xml";
	int current_stage = getPlugin<EventPlugin>()->currentStageId();
	bool is_relays = getPlugin<EventPlugin>()->eventConfig()->isRelays();

	QString str = is_relays
			? getPlugin<RelaysPlugin>()->startListIofXml30()
			: getPlugin<RunsPlugin>()->startListStageIofXml30(current_stage, false);

	QFile f(file_name);
	if(f.open(QFile::WriteOnly)) {
		f.write(str.toUtf8());
	}
}

bool EmmaClient::createExportDir() const
{
	EmmaClientSettings ss = settings();
	QString export_dir = ss.exportDir();
	QDir ed;
	if(!ed.mkpath(export_dir)) {
		qfError() << "Canot create export dir:" << export_dir;
		return false;
	}
	return true;
}

void EmmaClient::onDbEventNotify(const QString &domain, int connection_id, const QVariant &data)
{
	Q_UNUSED(connection_id)
	Q_UNUSED(data)
	//qfInfo() << domain << data;
	if(domain == QLatin1String(Event::EventPlugin::DBEVENT_CARD_PROCESSED_AND_ASSIGNED)) {
		onCardChecked(data.toMap());
	}
}

void EmmaClient::onCardChecked(const QVariantMap &data)
{
	Q_UNUSED(data)
/*  regenerate file every X second
	if(status() != Status::Running)
		return;
	quickevent::core::si::CheckedCard checked_card(data);
	QString s = QString("%1").arg(checked_card.cardNumber(), 8, 10, QChar(' '));
	s += QStringLiteral(": FIN/");
	int64_t msec = checked_card.stageStartTimeMs() + checked_card.finishTimeMs();
	QTime tm = QTime::fromMSecsSinceStartOfDay(msec);
	s += tm.toString(QStringLiteral("HH:mm:ss.zzz"));
	s += '0';
	s += '/';
	if (checked_card.finishTimeMs() > 0) {
		if (checked_card.isMisPunch() || checked_card.isBadCheck()) {
			s += QStringLiteral("MP  ");
		} else {
			//checked_card is OK
			s += QStringLiteral("O.K.");
		}
	} else {
		// DidNotFinish
		s += QStringLiteral("DNF ");
	}
	qfInfo() << "EmmaClient: " << s;
	EmmaClientSettings ss = settings();
	QString fn = ss.exportDir() + '/' + ss.fileName();
	QFile file(fn);
	if(!file.open(QIODevice::WriteOnly | QIODevice::Append)) {
		qfError() << "Cannot open file" << file.fileName() << "for writing, stopping service";
		stop();
		return;
	}
	QTextStream out(&file);
	out << s << "\n";*/
}

qf::gui::framework::DialogWidget *EmmaClient::createDetailWidget()
{
	auto *w = new EmmaClientWidget();
	return w;
}

void EmmaClient::init()
{
	EmmaClientSettings ss = settings();
	if(ss.exportIntervalSec() > 0) {
		m_exportTimer->setInterval(ss.exportIntervalSec() * 1000);
	}
}

bool EmmaClient::preExport() const
{
	EmmaClientSettings ss = settings();
	if(!QDir().mkpath(ss.exportDir())) {
		qfError() << "Cannot create export dir:" << ss.exportDir();
		return false;
	}
	return true;
}

void EmmaClient::onExportTimerTimeOut()
{
	if (!preExport())
		return;

	EmmaClientSettings ss = settings();

	if (ss.exportStartListTypeXml3())
	{
		qfInfo() << "EmmaClient Start List Iof Xml3 creation called";
		exportStartListIofXml3();
	}
	if (ss.exportResultTypeXml3())
	{
		qfInfo() << "EmmaClient Result Iof Xml3 creation called";
		exportResultsIofXml3();
	}
	if (ss.exportStartTypeTxt())
	{
		qfInfo() << "EmmaClient startlist creation called";
		if(settings().startExportType() == EmmaClientSettings::StartExportType::CSV)
			exportStartListRacomCsv();
		else
			exportStartListRacomTxt();
	}
	if (ss.exportFinishTypeTxt())
	{
		qfInfo() << "EmmaClient finish creation called";
		exportFinishRacomTxt();
	}
}

void EmmaClient::exportFinishRacomTxt() const
{
	EmmaClientSettings ss = settings();
	QString export_dir = ss.exportDir();
	QFile f(export_dir + '/' + ss.fileNameBase() + ".finish.txt");
	if(!f.open(QFile::WriteOnly)) {
		qfError() << "Canot open file:" << f.fileName() << "for writing.";
		return;
	}

	QTextStream ts(&f);

	int current_stage = getPlugin<EventPlugin>()->currentStageId();
	int start00 = getPlugin<EventPlugin>()->stageStartMsec(current_stage);

	qfs::QueryBuilder qb;
	qfs::Query q2;

	qb.select2("runs", "siId, isRunning, finishTimeMs, " + quickevent::core::RunStatus::runsTableColumns().join(","))
			.select2("cards", "id")
			.from("runs")
			.join("runs.id","cards.runId")
			.where("runs.stageId=" QF_IARG(current_stage))
			.where("runs.siId IS NOT NULL")
			.orderBy("runs.finishTimeMs ASC ")
			.orderBy("runs.id ASC");
	q2.execThrow(qb.toString());
	int last_si = 0;
	while(q2.next()) {
		int si = q2.value("runs.siId").toInt();
		int fin_time = q2.value("runs.finishTimeMs").toInt();
		auto run_status = quickevent::core::RunStatus::fromQuery(q2);
		int cards_id = q2.value("cards.id").toInt();

		if (si == 0 || last_si == si)
			continue; // without si or duplicate readout are unusable
		if (cards_id == 0 && run_status.isOk())
			continue; // skip if not yet card readout and not DNS,DNF, ...
		last_si = si;
		QString s = QString("%1").arg(si , 8, 10, QChar(' '));
		s += QStringLiteral(": FIN/");
		int msec = start00 + fin_time;
		QTime tm = QTime::fromMSecsSinceStartOfDay(msec);
		s += tm.toString(QStringLiteral("HH:mm:ss.zzz"));
		s += '0';
		s += '/';
		s += run_status.toEmmaExportString();
		ts << s << "\n";
	}
}

void EmmaClient::exportStartListRacomTxt() const
{
	EmmaClientSettings ss = settings();
	QString export_dir = ss.exportDir();
	QFile f(export_dir + '/' + ss.fileNameBase() + ".start.txt");
	if(!f.open(QFile::WriteOnly)) {
		qfError() << "Canot open file:" << f.fileName() << "for writing.";
		return;
	}

	QTextStream ts(&f);
	ts.setGenerateByteOrderMark(true); // BOM

	bool is_relays = getPlugin<EventPlugin>()->eventConfig()->isRelays();
	int current_stage = getPlugin<EventPlugin>()->currentStageId();
	qfs::QueryBuilder qb;
	qb.select2("runs", "startTimeMs, siId, competitorId, isrunning, leg, finishTimeMs")
			.select2("competitors","firstName, lastName, registration")
			.select2("classes","name")
			.select2("cards", "id, startTime")
			.from("runs")
			.join("runs.competitorId","competitors.id")
			.join("runs.id", "cards.runId")
			.where("runs.stageId=" QF_IARG(current_stage));
	if(is_relays) {
		qb.select2("relays","number, isrunning");
		qb.join("runs.relayId", "relays.id");
		qb.join("relays.classId", "classes.id");
		qb.orderBy("runs.leg, relays.number ASC");
	}
	else {
		qb.join("competitors.classId","classes.id");
		qb.orderBy("runs.id ASC");
	}

	int start00 = getPlugin<EventPlugin>()->stageStartMsec(current_stage);
	qfDebug() << qb.toString();
	qfs::Query q2;
	q2.execThrow(qb.toString());
	int last_id = -1;
	QMap <int,int> start_times_relays;
	while(q2.next()) {
		int id = q2.value("runs.competitorId").toInt();
		if (id == last_id)
			continue;
		bool is_running = (q2.value("runs.isrunning").isNull()) ? false : q2.value("runs.isrunning").toBool();
		bool is_rel_running = (q2.value("relays.isrunning").isNull()) ? false : q2.value("relays.isrunning").toBool();
		if (!is_running || !is_rel_running)
			continue;
		last_id = id;
		int si = q2.value("runs.siId").toInt();
		int start_time = q2.value("runs.startTimeMs").toInt();
		bool start_time_null = q2.value("runs.startTimeMs").isNull();
		int start_time_card = q2.value("cards.startTime").toInt();
		bool start_time_card_null = q2.value("cards.startTime").isNull();
		if ((start_time_card == INVALID_SI_TIME || start_time_card_null) && start_time_null)
			start_time_card = 0;
		QString name = q2.value("competitors.lastName").toString() + " " + q2.value("competitors.firstName").toString();
		QString class_name = q2.value("classes.name").toString();
		class_name.remove(" ");
		// don't export runners without class and SI card
		if (class_name.isEmpty() || si == 0)
			continue;
		QString reg = q2.value("competitors.registration").toString();
		name = name.leftJustified(22,QChar(' '),true);
		int id_or_bib = id;
		if (is_relays)
		{
			int leg = q2.value("runs.leg").toInt();
			class_name = QString("%1 %2").arg(class_name).arg(leg);
			class_name = class_name.leftJustified(7,QChar(' '),true);
			reg = reg.left(3);
			reg = reg.leftJustified(7,QChar(' '),true);
			// EmmaClient uses for all relays start time of 1st leg
			int rel_num =  q2.value("relays.number").toInt();
			if (leg == 1 && rel_num > 0) {
				start_times_relays.insert(rel_num,start_time);
			} else if (rel_num > 0) {
				auto it = start_times_relays.find(rel_num);
				if (it != start_times_relays.end())
					start_time = it.value();
			}
			id_or_bib = (rel_num > 0) ? rel_num : 10000+id; // when no number, use id with offset
		}
		else
		{
			class_name = class_name.leftJustified(7,QChar(' '),true);
			reg = reg.leftJustified(7,QChar(' '),true);
		}

		int msec = start_time;
		if (start_time_card != 0 && !start_time_card_null && !is_relays)
		{
			// has start in si card (P, T, HDR) & not used in relays
			start_time_card *= 1000; // msec
			if (start00 > HR_12_MSEC) // if start00 is PM, move start time
				start_time_card += HR_12_MSEC;
			start_time_card -= start00;
			msec = start_time_card;
		}

		// new emma client can handle negative times

		int min = (msec / 60000);
		int sec = (abs(msec) % 60000 / 1000);
		int zzzz = abs(msec) % 1000 * 10;

		QString tm2 = QString("%1.%2,%3").arg(min,3,10,QChar('0')).arg(sec,2,10,QChar('0')).arg(zzzz,4,10,QChar('0'));

		if (id != 0) // filter bad data
		{
			QString s = QString("%1 %2 %3 %4 %5 %6").arg(id_or_bib , 5, 10, QChar(' ')).arg(si, 8, 10, QChar(' ')).arg(class_name).arg(reg).arg(name).arg(tm2);
			ts << s << "\n";
		}
	}
}

void EmmaClient::exportStartListRacomCsv() const
{
	EmmaClientSettings ss = settings();
	QString export_dir = ss.exportDir();
	QFile f(export_dir + '/' + ss.fileNameBase() + ".start.csv");
	if(!f.open(QFile::WriteOnly)) {
		qfError() << "Canot open file:" << f.fileName() << "for writing.";
		return;
	}

	QTextStream ts(&f);
	ts.setGenerateByteOrderMark(true); // BOM
#if QT_VERSION_MAJOR >= 6
	ts.setEncoding(QStringConverter::encodingForName("UTF-8").value());
#else
	ts.setCodec("UTF-8");
#endif
	bool is_relays = getPlugin<EventPlugin>()->eventConfig()->isRelays();
	int current_stage = getPlugin<EventPlugin>()->currentStageId();
	const QString separator(";");

	ts << "ID" << separator << "Class" << separator << "SI" << separator
	   << "Name" << separator << "Starttime" << separator << "Club" << separator <<"Bib";
	if (is_relays)
		ts << separator << "Leg";
	ts << "\n";

	qfs::QueryBuilder qb;
	qb.select2("runs", "startTimeMs, siId, competitorId, isrunning, leg")
		.select2("competitors","firstName, lastName, registration, club, startNumber")
		.select2("classes","name")
		.select2("cards", "id, startTime")
		.from("runs")
		.join("runs.competitorId","competitors.id")
		.join("runs.id", "cards.runId")
		.where("runs.stageId=" QF_IARG(current_stage));
	if(is_relays) {
		qb.select2("relays","number, isrunning");
		qb.join("runs.relayId", "relays.id");
		qb.join("relays.classId", "classes.id");
		qb.orderBy("runs.leg, relays.number ASC");
	}
	else {
		qb.join("competitors.classId","classes.id");
		qb.orderBy("runs.id ASC");
	}

	int start00 = getPlugin<EventPlugin>()->stageStartMsec(current_stage);
	qfDebug() << qb.toString();
	qfs::Query q2;
	q2.execThrow(qb.toString());
	int last_id = -1;
	QMap <int,int> start_times_relays;
	while(q2.next()) {
		int id = q2.value("runs.competitorId").toInt();
		if (id == last_id)
			continue;
		bool is_running = (q2.value("runs.isrunning").isNull()) ? false : q2.value("runs.isrunning").toBool();
		bool is_rel_running = (q2.value("relays.isrunning").isNull()) ? false : q2.value("relays.isrunning").toBool();
		if (!is_running || !is_rel_running)
			continue;
		last_id = id;
		int si = q2.value("runs.siId").toInt();
		int start_time = q2.value("runs.startTimeMs").toInt();
		int bib = q2.value("competitors.startNumber").toInt();
		bool start_time_null = q2.value("runs.startTimeMs").isNull();
		int start_time_card = q2.value("cards.startTime").toInt();
		bool start_time_card_null = q2.value("cards.startTime").isNull();
		if ((start_time_card == INVALID_SI_TIME || start_time_card_null) && start_time_null)
			start_time_card = 0;
		QString name = q2.value("competitors.lastName").toString() + " " + q2.value("competitors.firstName").toString();
		QString class_name = q2.value("classes.name").toString();
		// don't export runners without class and SI card
		if (class_name.isEmpty() || si == 0)
			continue;
		int id_or_bib = id;
		int leg_no = q2.value("runs.leg").toInt();
		if (is_relays) {
			// EmmaClient uses for all relays start time of 1st leg
			int rel_num =  q2.value("relays.number").toInt();
			if (rel_num > 0)
				bib = rel_num;
			if (leg_no == 1 && rel_num > 0) {
				start_times_relays.insert(rel_num,start_time);
			} else if (rel_num > 0) {
				auto it = start_times_relays.find(rel_num);
				if (it != start_times_relays.end())
					start_time = it.value();
			}
			id_or_bib = (rel_num > 0) ? rel_num : 10000+id; // when no number, use id with offset
		}

		int msec = start_time;
		if (start_time_card != 0 && !start_time_card_null && !is_relays)
		{
			// has start in si card (P, T, HDR) & not used in relays
			start_time_card *= 1000; // msec
			if (start00 > HR_12_MSEC) // if start00 is PM, move start time
				start_time_card += HR_12_MSEC;
			start_time_card -= start00;
			msec = start_time_card;
		}

		int min = (msec / 60000);
		int sec = (abs(msec) % 60000 / 1000);
		int zzzz = abs(msec) % 1000 * 10;

		QString tm2 = QString("%1.%2,%3").arg(min,3,10,QChar('0')).arg(sec,2,10,QChar('0')).arg(zzzz,4,10,QChar('0'));

		QString club = q2.value("competitors.club").toString();
		QString reg = q2.value("competitors.registration").toString();
		if (club.isEmpty() && !reg.isEmpty()) {
			qf::core::sql::Query q;
			q.exec(QStringLiteral("SELECT name FROM clubs WHERE abbr='%1'").arg(reg.left(3)), qf::core::Exception::Throw);
			if (q.next()) {
				club = q.value(0).toString();
			}
		}

		if (id != 0){ // filter bad data
			// columns are :
			// ID, Class, SI, Name, Starttime, Club, Bib, [Leg]
			ts << id_or_bib;
			ts << separator;
			ts << class_name;
			ts << separator;
			ts << si;
			ts << separator;
			ts << name;
			ts << separator;
			ts << tm2;
			ts << separator;
			ts << club;
			ts << separator;
			ts << bib;
			if (is_relays) {
				ts << separator;
				ts << leg_no;
			}
			ts << "\n";
		}
	}
}

void EmmaClient::loadSettings()
{
	Super::loadSettings();
	init();
}

}
