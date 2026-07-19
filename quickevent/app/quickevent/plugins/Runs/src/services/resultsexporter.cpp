#include "resultsexporter.h"
#include "resultsexporterwidget.h"
#include "../runsplugin.h"

#include <quickevent/core/si/checkedcard.h>
#include <quickevent/core/exporters/stageresultshtmlexporter.h>
#include <quickevent/core/exporters/stageresultscsvexporter.h>

#include <qf/gui/framework/mainwindow.h>
#include <qf/gui/dialogs/dialog.h>
#include <plugins/Event/src/eventplugin.h>
#include <plugins/Relays/src/relaysplugin.h>

#include <qf/core/log.h>
#include <qf/core/assert.h>

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>

using qf::gui::framework::getPlugin;
using Event::EventPlugin;
using Runs::RunsPlugin;
using Relays::RelaysPlugin;

namespace Runs::services {

ResultsExporter::ResultsExporter(QObject *parent)
	: Super(ResultsExporter::serviceName(), parent)
{
	m_exportTimer = new QTimer(this);
	connect(m_exportTimer, &QTimer::timeout, this, &ResultsExporter::onExportTimerTimeOut);
	connect(this, &ResultsExporter::settingsChanged, this, &ResultsExporter::init, Qt::QueuedConnection);

}

QString ResultsExporter::serviceName()
{
	return QStringLiteral("ResultsExporter");
}

void ResultsExporter::run() {
	Super::run();
	// run once immediately without waiting for first timer expiration
	if(settings().exportIntervalSec() > 0) {
		onExportTimerTimeOut();
	}
	m_exportTimer->start();
}

void ResultsExporter::stop() {
	Super::stop();
	m_exportTimer->stop();
}

bool ResultsExporter::exportResults() const
{
	ResultsExporterSettings ss = settings();
	if(!QDir().mkpath(ss.exportDir())) {
		qfError() << "Cannot create export dir:" << ss.exportDir();
		return false;
	}
	qfInfo() << "ResultsExporter export dir:" << ss.exportDir();
	if(ss.outputFormat() == static_cast<int>(ResultsExporterSettings::OutputFormat::CSOS)) {
		int current_stage = getPlugin<EventPlugin>()->currentStageId();
		QString fn = ss.exportDir() + "/results-csos.txt";
		getPlugin<RunsPlugin>()->exportResultsCsosStage(current_stage, fn);
		return true;
	}
	if(ss.outputFormat() == static_cast<int>(ResultsExporterSettings::OutputFormat::IofXml3)) {
		int current_stage = getPlugin<EventPlugin>()->currentStageId();
		QString file_name = ss.exportDir() + "/" + getPlugin<EventPlugin>()->eventDbName() + "." + Event::RESULTS_IOFXML3_FILE;
		bool is_relays = getPlugin<EventPlugin>()->eventConfig().isRelays();

		QString str = is_relays
				? getPlugin<RelaysPlugin>()->resultsIofXml30()
				: getPlugin<RunsPlugin>()->resultsIofXml30Stage(current_stage);

		QFile f(file_name);
		if(f.open(QFile::WriteOnly)) {
			f.write(str.toUtf8());
		}
		return true;
	}
	if(ss.outputFormat() == static_cast<int>(ResultsExporterSettings::OutputFormat::HtmlMulti)) {
		quickevent::core::exporters::StageResultsHtmlExporter exp;
		exp.setOutDir(ss.exportDir());
		exp.generateHtml();

		whenFinishedRunCmd();
		return true;
	}
	if(ss.outputFormat() == static_cast<int>(ResultsExporterSettings::OutputFormat::CSVMulti)) {
		bool is_iof_race = getPlugin<EventPlugin>()->eventConfig().iofRace;
		quickevent::core::exporters::StageResultsCsvExporter exp(is_iof_race);
		exp.setOutDir(ss.exportDir());
		if (!ss.csvSeparator().isNull())
			exp.setSeparator(ss.csvSeparator());
		exp.setWithDidNotStart(ss.exportCsvWithDNS());
		exp.generateCsvMulti();

		whenFinishedRunCmd();
		return true;
	}
	if(ss.outputFormat() == static_cast<int>(ResultsExporterSettings::OutputFormat::CSV)) {
		bool is_iof_race = getPlugin<EventPlugin>()->eventConfig().iofRace;
		quickevent::core::exporters::StageResultsCsvExporter exp(is_iof_race);
		exp.setOutDir(ss.exportDir());
		if (!ss.csvSeparator().isNull())
			exp.setSeparator(ss.csvSeparator());
		exp.setWithDidNotStart(ss.exportCsvWithDNS());
		exp.generateCsvSingle();

		whenFinishedRunCmd();
		return true;
	}
	qfError() << "Unsupported output format:" << ss.outputFormat();
	return false;
}

void ResultsExporter::whenFinishedRunCmd() const
{
	ResultsExporterSettings ss = settings();
	QString cmd = ss.whenFinishedRunCmd();
	if(!cmd.isEmpty()) {
		qfInfo() << "Starting process:" << cmd;
		auto *proc = new QProcess();
		connect(proc, &QProcess::readyReadStandardOutput, [proc]() {
			QByteArray ba = proc->readAllStandardOutput();
			qfInfo() << "PROC stdout:" << ba;
		});
		connect(proc, &QProcess::readyReadStandardError, [proc]() {
			QByteArray ba = proc->readAllStandardError();
			qfWarning() << "PROC stderr:" << ba;
		});
		connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), [proc](int exit_code, QProcess::ExitStatus exit_status) {
			if(exit_status == QProcess::ExitStatus::CrashExit)
				qfError() << "PROC crashed";
			else
				qfInfo() << "PROC finished with exit code:" << exit_code;
			proc->deleteLater();
		});

		QStringList cmd_args = qf::core::Utils::parseProgramAndArgumentsList(cmd);
		proc->start(cmd_args.value(0), cmd_args.mid(1));
	}
}


void ResultsExporter::onExportTimerTimeOut()
{
	if(!exportResults())
		stop();
}

qf::gui::framework::DialogWidget *ResultsExporter::createDetailWidget()
{
	auto *w = new ResultsExporterWidget();
	return w;
}

void ResultsExporter::loadSettings()
{
	Super::loadSettings();
	ResultsExporterSettings ss = settings();

	if(ss.exportDir().isEmpty()) {
		ss.setExportDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/quickevent/results");
		setSettings(ss);
	}

	init();
}

void ResultsExporter::init()
{
	ResultsExporterSettings ss = settings();
	if(ss.exportIntervalSec() > 0) {
		m_exportTimer->setInterval(ss.exportIntervalSec() * 1000);
	}
	else {
		m_exportTimer->stop();
	}
}

}
