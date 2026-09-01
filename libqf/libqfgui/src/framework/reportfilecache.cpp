#include "reportfilecache.h"

#include <qf/core/log.h>

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

using namespace qf::gui::framework;

ReportFileCache::ReportFileCache()
	: QObject(nullptr)
{
}


QString ReportFileCache::effectiveReportsDir() const
{
	if(m_reportsDir.isEmpty())
		return reportCacheDir();
	return m_reportsDir;
}

QString ReportFileCache::defaultReportsDir() const
{
	static const auto dir = QCoreApplication::applicationDirPath() + "/reports";
	return dir;
}

QString ReportFileCache::reportCacheDir() const
{
	static const auto dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/reports";
	return dir;
}

namespace {

bool isSafeReportPath(const QString &path)
{
	if(path.isEmpty() || QDir::isAbsolutePath(path))
		return false;
	const auto parts = QDir::fromNativeSeparators(path).split('/', Qt::SkipEmptyParts);
	return !parts.contains(QStringLiteral(".")) && !parts.contains(QStringLiteral(".."));
}

}

void ReportFileCache::initialize() const
{
	const QString cache_dir_path = reportCacheDir();
	QFileInfo cache_info(cache_dir_path);
	if(cache_info.exists())
		return;

	QDir cache_dir;
	if(!cache_dir.mkpath(cache_dir_path)) {
		qfError() << "Cannot create report cache directory:" << cache_dir_path;
		return;
	}

	const QString source_dir_path = defaultReportsDir();
	QDir source_dir(source_dir_path);
	if(!source_dir.exists()) {
		qfWarning() << "Default reports directory does not exist:" << source_dir_path;
		return;
	}

	QDirIterator it(source_dir_path, QDir::Files, QDirIterator::Subdirectories);
	while(it.hasNext()) {
		const QString source_file_path = it.next();
		const QString relative_path = source_dir.relativeFilePath(source_file_path);
		const QString destination_file_path = cache_dir.filePath(relative_path);
		QDir().mkpath(QFileInfo(destination_file_path).path());
		if(!QFile::copy(source_file_path, destination_file_path))
			qfWarning() << "Cannot copy report file to cache:" << source_file_path << destination_file_path;
	}
}

void ReportFileCache::applyDatabaseOverrides() const
{
	QSqlDatabase db = QSqlDatabase::database();
	if(!db.isValid() || !db.isOpen())
		return;

	QSqlQuery query(db);
	if(!query.exec(QStringLiteral("SELECT path, data FROM reports"))) {
		qfWarning() << "Cannot read report overrides:" << query.lastError().text();
		return;
	}

	QDir cache_dir(reportCacheDir());
	if(!cache_dir.exists() && !cache_dir.mkpath(QStringLiteral("."))) {
		qfError() << "Cannot create report cache directory:" << cache_dir.path();
		return;
	}
	while(query.next()) {
		const QString relative_path = query.value(0).toString();
		if(!isSafeReportPath(relative_path)) {
			qfWarning() << "Ignoring unsafe report path from database:" << relative_path;
			continue;
		}
		const QString file_path = cache_dir.filePath(relative_path);
		if(!QDir().mkpath(QFileInfo(file_path).path())) {
			qfWarning() << "Cannot create report directory:" << QFileInfo(file_path).path();
			continue;
		}
		QFile file(file_path);
		if(!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(query.value(1).toByteArray()) < 0)
			qfWarning() << "Cannot write report override:" << file_path;
	}
}
