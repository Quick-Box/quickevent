#include "reportfilecache.h"

#include <qf/core/log.h>

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <qsqlquery.h>

namespace qf::gui::framework {

namespace {

bool isSafeReportPath(const QString &path)
{
	if(path.isEmpty() || QDir::isAbsolutePath(path))
		return false;
	const auto parts = QDir::fromNativeSeparators(path).split('/', Qt::SkipEmptyParts);
	return !parts.contains(QStringLiteral(".")) && !parts.contains(QStringLiteral(".."));
}

bool isSafeCacheId(const QString &id)
{
	return isSafeReportPath(id) && !id.contains('/') && !id.contains('\\');
}

}

ReportFileCache::ReportFileCache()
	: QObject(nullptr)
{
}

QString ReportFileCache::localReportFile(const QString &relative_path) const
{
	if(!isSafeReportPath(relative_path)) {
		qfWarning() << "Unsafe report path:" << relative_path;
		return {};
	}
	const QString dir = localReportsDir();
	if(dir.isEmpty())
		return {};
	return dir + "/" + relative_path;
}

QString ReportFileCache::localReportsDir() const
{
	return reportCacheDir();
}

QString ReportFileCache::reportCacheRootDir() const
{
	static const auto dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/cache/report";
	return dir;
}

QString ReportFileCache::reportCacheDir() const
{
	if(m_eventCacheId.isEmpty()) {
		qfError() << "Report cache is not open";
		return {};
	}
	return reportCacheRootDir() + "/" + m_eventCacheId;
}

void ReportFileCache::openEventCache(const QString &event_cache_id)
{
	if(!isSafeCacheId(event_cache_id)) {
		qfError() << "Invalid report cache id:" << event_cache_id;
		closeEventCache();
		return;
	}
	m_eventCacheId = event_cache_id;
	qfInfo() << "Reports local cache dir set to:" << reportCacheDir();
	syncWithResources();
	applyDatabaseOverrides();
}

void ReportFileCache::closeEventCache()
{
	m_eventCacheId.clear();
}

QString ReportFileCache::resourceHashesFile() const
{
	// kept outside of the cache dir, so it is not listed as a report file
	return reportCacheDir() + ".hashes.json";
}

void ReportFileCache::syncWithResources() const
{
	const QString cache_dir_path = reportCacheDir();
	if(cache_dir_path.isEmpty())
		return;
	QDir cache_dir(cache_dir_path);
	if(!cache_dir.mkpath(cache_dir_path)) {
		qfError() << "Cannot create report cache directory:" << cache_dir_path;
		return;
	}

	static const auto source_dir_path = QStringLiteral(":/reports");
	QDir source_dir(source_dir_path);
	if(!source_dir.exists()) {
		qfWarning() << "Default reports directory does not exist:" << source_dir_path;
		return;
	}

	// hashes of resource files copied to cache last time,
	// local file with the same hash was not changed by user and can be updated safely
	QJsonObject old_hashes;
	{
		QFile file(resourceHashesFile());
		if(file.open(QIODevice::ReadOnly))
			old_hashes = QJsonDocument::fromJson(file.readAll()).object();
	}
	QJsonObject new_hashes;

	QDirIterator it(source_dir_path, QDir::Files, QDirIterator::Subdirectories);
	while(it.hasNext()) {
		const QString source_file_path = it.next();
		const QString relative_path = source_dir.relativeFilePath(source_file_path);
		const QString destination_file_path = cache_dir.filePath(relative_path);
		const QString resources_hash = fileHash(source_file_path);
		const QString local_hash = fileHash(destination_file_path);
		if(local_hash == resources_hash) {
			new_hashes[relative_path] = resources_hash;
			continue;
		}
		if(!local_hash.isEmpty() && local_hash != old_hashes.value(relative_path).toString()) {
			qfInfo() << "Keeping locally changed report file:" << relative_path;
			if(old_hashes.contains(relative_path))
				new_hashes[relative_path] = old_hashes.value(relative_path);
			continue;
		}
		qfInfo() << (local_hash.isEmpty()? "Adding": "Updating") << "report file in cache:" << relative_path;
		QDir().mkpath(QFileInfo(destination_file_path).path());
		QFile source_file(source_file_path);
		QFile destination_file(destination_file_path);
		if(!source_file.open(QIODevice::ReadOnly)
				|| !destination_file.open(QIODevice::WriteOnly | QIODevice::Truncate)
				|| destination_file.write(source_file.readAll()) < 0) {
			qfWarning() << "Cannot copy report file to cache:" << source_file_path << destination_file_path;
			continue;
		}
		new_hashes[relative_path] = resources_hash;
	}

	QFile file(resourceHashesFile());
	if(!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(QJsonDocument(new_hashes).toJson()) < 0) {
		qfWarning() << "Cannot write report resource hashes:" << file.fileName();
	}
}

void ReportFileCache::applyDatabaseOverrides() const
{
	if(reportCacheDir().isEmpty())
		return;
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
		const QString file_path = localReportFile(relative_path);
		const QString local_hash = fileHash(file_path);
		const QString resources_hash = fileHash(QStringLiteral(":/reports/") + relative_path);
		if(resources_hash.isEmpty() || local_hash == resources_hash) {
			// file does not exist in resource or it is the same as the local copy
			if(!QDir().mkpath(QFileInfo(file_path).path())) {
				qfWarning() << "Cannot create report directory:" << QFileInfo(file_path).path();
				continue;
			}
			QFile file(file_path);
			if(!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(query.value(1).toByteArray()) < 0) {
				qfWarning() << "Cannot write report override:" << file_path;
			}
		} else {
			qfInfo() << "Skipping report DB override because the local report was changed:" << relative_path;
		}
	}
}

void ReportFileCache::clearLocalChanges()
{
	if(reportCacheDir().isEmpty()) {
		return;
	}
	QDir cache_dir(reportCacheDir());
	if(cache_dir.exists()) {
		cache_dir.removeRecursively();
	}
	QFile::remove(resourceHashesFile());
	syncWithResources();
	applyDatabaseOverrides();
}

QString ReportFileCache::dataHash(const QByteArray &data)
{
	return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex());
}

QString ReportFileCache::fileHash(const QString &file_path)
{
	QFile file(file_path);
	if (!file.open(QIODevice::ReadOnly))
		return {};
	return dataHash(file.readAll());
}

QByteArray ReportFileCache::loadReportFile(const QString &relative_path) const
{
	const QString file_path = localReportFile(relative_path);
	if(file_path.isEmpty())
		return {};
	QFile file(file_path);
	if (!file.open(QIODevice::ReadOnly))
		return {};
	return file.readAll();
}

bool ReportFileCache::saveRemoteFileContent(const QString &relative_path, const QByteArray &data, bool update_local_copy) const
{
	if (!isSafeReportPath(relative_path)) {
		qfError() << "Cannot save report file, unsafe path:" << relative_path;
		return false;
	}
	// local copy is staged in a temporary file and committed only after the DB is updated,
	// so a DB failure leaves the original local report untouched
	QSaveFile file;
	if (update_local_copy) {
		const QString file_path = localReportFile(relative_path);
		if (file_path.isEmpty()) {
			qfError() << "Cannot save report file, no event report cache is open:" << relative_path;
			return false;
		}
		if (!QDir().mkpath(QFileInfo(file_path).path())) {
			qfError() << "Cannot create report directory:" << QFileInfo(file_path).path();
			return false;
		}
		file.setFileName(file_path);
		if (!file.open(QIODevice::WriteOnly)) {
			qfError() << "Failed to open local report file:" << file.fileName() << "for writing" << file.errorString();
			return false;
		}
		if (file.write(data) != data.size()) {
			qfError() << "Failed to write local report file:" << file.fileName() << file.errorString();
			return false;
		}
	}

	const QString hash = dataHash(data);

	QSqlQuery update_query;
	update_query.prepare(QStringLiteral("UPDATE reports SET data=:data, hash=:hash WHERE path=:path"));
	update_query.bindValue(QStringLiteral(":path"), relative_path);
	update_query.bindValue(QStringLiteral(":data"), data);
	update_query.bindValue(QStringLiteral(":hash"), hash);
	if (!update_query.exec()) {
		qfError() << "Failed to update report in database:" << update_query.lastError().text();
		return false;
	}
	if (update_query.numRowsAffected() < 1) {
		QSqlQuery insert_query;
		insert_query.prepare(QStringLiteral("INSERT INTO reports(path, data, hash) VALUES(:path, :data, :hash)"));
		insert_query.bindValue(QStringLiteral(":path"), relative_path);
		insert_query.bindValue(QStringLiteral(":data"), data);
		insert_query.bindValue(QStringLiteral(":hash"), hash);
		if (!insert_query.exec()) {
			qfError() << "Failed to insert report into database:" << insert_query.lastError().text();
			return false;
		}
	}
	if (update_local_copy && !file.commit()) {
		qfError() << "Failed to commit local report file:" << file.fileName() << file.errorString();
		return false;
	}
	return true;
}

}
