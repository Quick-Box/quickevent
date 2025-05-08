#include "plugin.h"

#include <qf/core/utils.h>
#include <qf/core/log.h>
#include <qf/core/assert.h>

#include <QQmlEngine>
#include <QTranslator>
#include <QFile>
#include <QDirIterator>
#include <QCoreApplication>

using namespace qf::qmlwidgets::framework;

QString Plugin::m_reportsDir;

Plugin::Plugin(const QString &feature_id, QObject *parent)
	: QObject(parent)
{
	qfLogFuncFrame();
	m_featureId = feature_id;
}

Plugin::Plugin(QObject *parent)
	: QObject(parent)
{
	qfLogFuncFrame();
}

Plugin::~Plugin()
{
	qfLogFuncFrame() << this;
}

QString Plugin::pluginDataDir()
{
	static QString dir = ":/quickevent";
	return dir;
}

QString Plugin::effectiveReportsDir()
{
	if(m_reportsDir.isEmpty())
		m_reportsDir = defaultReportsDir();
	return m_reportsDir;
}

QString Plugin::defaultReportsDir()
{
	static auto dir = QCoreApplication::applicationDirPath() + "/reports";
	return dir;
}

QString Plugin::findReportFile(const QString &report_file_path) const
{
	QStringList search_paths;
	search_paths << effectiveReportsDir() + '/' + m_featureId + "/qml/reports";
	//search_paths << qmlReportsDir();
	for(const QString &dir : search_paths) {
		qfMessage() << "search_path:" << dir;
		auto fn = dir + '/' + report_file_path;
		qfMessage() << "trying:" << fn;
		QFileInfo fi(fn);
		if(fi.isFile()) {
			qfMessage() << "HIT:" << fn;
			return fn;
		}
	}
	qfError() << "Cannot find report file for feature id:" << m_featureId << "and file path:" << report_file_path;
	return {};
}

QList<Plugin::ReportFileInfo> Plugin::listReportFiles(const QString &report_dir) const
{
	QList<ReportFileInfo> report_files;
	QStringList search_paths;
	search_paths << effectiveReportsDir() + '/' + m_featureId + "/qml/reports";
	for(const QString &dir : search_paths) {
		QDirIterator it(dir + '/' + report_dir, QDirIterator::NoIteratorFlags);
		while (it.hasNext()) {
			it.next();
			QFileInfo fi = it.fileInfo();
			if(fi.isFile() && fi.suffix() == "qml") {
				ReportFileInfo i {
					.reportName = fi.baseName(),
					.reportFilePath = report_dir + '/' + fi.fileName()
				};
				if(!report_files.contains(i))
					report_files << i;
			}
		}
	}
	std::sort(report_files.begin(), report_files.end(), [](const ReportFileInfo &a, const ReportFileInfo &b) { return a.reportName < b.reportName; });
	return report_files;
}
