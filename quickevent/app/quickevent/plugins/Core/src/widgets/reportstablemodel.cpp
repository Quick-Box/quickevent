#include "reportstablemodel.h"

#include <qf/core/log.h>
#include <qf/core/sql/query.h>
#include <qf/gui/framework/plugin.h>
#include <qf/gui/framework/reportfilecache.h>

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QColor>

#include <utility>

namespace Core {

ReportsTableModel::ReportsTableModel(QObject *parent) :
	QAbstractTableModel(parent)
{
}

int ReportsTableModel::rowCount(const QModelIndex &parent) const
{
	return parent.isValid() ? 0 : m_reports.size();
}

int ReportsTableModel::columnCount(const QModelIndex &parent) const
{
	return parent.isValid() ? 0 : ColumnCount;
}

QVariant ReportsTableModel::data(const QModelIndex &index, int role) const
{
	if (!index.isValid() || index.row() < 0 || index.row() >= m_reports.size()) {
		return {};
	}

	const auto &report = m_reports.at(index.row());
	if (role == Qt::TextAlignmentRole && index.column() == FileSizeColumn)
		return QVariant::fromValue(Qt::AlignRight | Qt::AlignVCenter);
	if (role == Qt::EditRole) {
		switch (index.column()) {
		case FileNameColumn: return report.relativePath;
		case FileSizeColumn: return report.size;
		case ResourcesHashColumn: return report.resourcesHash;
		case LocalHashColumn: return report.localHash;
		case DatabaseHashColumn: return report.databaseHash;
		default: break;
		}
		return {};
	}
	if (role == Qt::DisplayRole) {
		switch (index.column()) {
		case ResourcesHashColumn: return report.resourcesHash.mid(0, 8);
		case LocalHashColumn: return report.localHash.mid(0, 8);
		case DatabaseHashColumn: return report.databaseHash.mid(0, 8);
		default: return data(index, Qt::EditRole);
		}
	}
	if (role == Qt::BackgroundRole) {
		static QColor edited_background("salmon");
		switch (index.column()) {
		case LocalHashColumn: {
			if (report.localHash != report.resourcesHash) {
				return edited_background;
			}
			return {};
		}
		case DatabaseHashColumn: {
			if (!report.databaseHash.isEmpty() && report.localHash != report.databaseHash) {
				return edited_background;
			}
			return {};
		}
		default: return {};
		}
	}
	if (role == Qt::ToolTipRole) {
		switch (index.column()) {
		case ResourcesHashColumn:
		case LocalHashColumn:
		case DatabaseHashColumn: return data(index, Qt::EditRole);
		default: return data(index, Qt::DisplayRole);
		}
	}
	return {};
}

QVariant ReportsTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
		switch (section) {
		case FileNameColumn: return tr("Name");
		case FileSizeColumn: return tr("Size");
		case ResourcesHashColumn: return tr("Resources hash");
		case LocalHashColumn: return tr("Local hash");
		case DatabaseHashColumn: return tr("Database hash");
		default: return {};
		}
	}
	return QAbstractTableModel::headerData(section, orientation, role);
}

Qt::ItemFlags ReportsTableModel::flags(const QModelIndex &index) const
{
	if (!index.isValid())
		return Qt::NoItemFlags;
	return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}


void ReportsTableModel::load()
{
	m_reportsDir = qf::gui::framework::Plugin::reportFileCache()->localReportsDir();
	QList<Report> reports;
	if (m_reportsDir.isEmpty()) {
		setReports(std::move(reports));
		return;
	}
	QHash<QString, QString> database_hashes;
	qf::core::sql::Query database_query;
	database_query.prepare(QStringLiteral("SELECT path, hash FROM reports"));
	if (!database_query.exec()) {
		qfWarning() << "Cannot read report hashes from database:" << database_query.lastErrorText();
	}
	else {
		while (database_query.next())
			database_hashes.insert(database_query.value(0).toString(), database_query.value(1).toString());
	}

	const QDir reports_dir(m_reportsDir);
	QDirIterator iterator(m_reportsDir, QDir::Files | QDir::Hidden | QDir::System, QDirIterator::Subdirectories);
	while (iterator.hasNext()) {
		const QFileInfo file_info(iterator.next());
		const QString relative_path = reports_dir.relativeFilePath(file_info.filePath());
		reports.append(
			Report{
				.relativePath = relative_path,
				.size = file_info.size(),
				.resourcesHash = qf::gui::framework::ReportFileCache::fileHash(QStringLiteral(":/reports/") + relative_path),
				.localHash = qf::gui::framework::ReportFileCache::fileHash(file_info.filePath()),
				.databaseHash = database_hashes.value(relative_path)});
	}
	setReports(std::move(reports));
}

bool ReportsTableModel::saveReportToDb(const QModelIndex &report_index, QString *error_text)
{
	if (!report_index.isValid() || report_index.model() != this || report_index.row() < 0 || report_index.row() >= m_reports.size())
		return false;
	auto &report = m_reports[report_index.row()];

	QFile file(QDir(m_reportsDir).filePath(report.relativePath));
	if (!file.open(QIODevice::ReadOnly)) {
		if (error_text)
			*error_text = file.errorString();
		return false;
	}
	const QByteArray data = file.readAll();
	const QString hash = qf::gui::framework::ReportFileCache::dataHash(data);

	qf::core::sql::Query update_query;
	update_query.prepare(QStringLiteral("UPDATE reports SET data=:data, hash=:hash WHERE path=:path"));
	update_query.bindValue(QStringLiteral(":path"), report.relativePath);
	update_query.bindValue(QStringLiteral(":data"), data);
	update_query.bindValue(QStringLiteral(":hash"), hash);
	if (!update_query.exec()) {
		if (error_text) {
			*error_text = update_query.lastErrorText();
		}
		return false;
	}
	if (update_query.numRowsAffected() < 1) {
		qf::core::sql::Query insert_query;
		insert_query.prepare(QStringLiteral("INSERT INTO reports(path, data, hash) VALUES(:path, :data, :hash)"));
		insert_query.bindValue(QStringLiteral(":path"), report.relativePath);
		insert_query.bindValue(QStringLiteral(":data"), data);
		insert_query.bindValue(QStringLiteral(":hash"), hash);
		if (!insert_query.exec()) {
			if (error_text) {
				*error_text = insert_query.lastErrorText();
			}
			return false;
		}
	}
	report.databaseHash = hash;
	emitReportHashChanged(report_index);
	return true;
}

bool ReportsTableModel::clearReportFromDb(const QModelIndex &report_index, QString *error_text)
{
	if (!report_index.isValid() || report_index.model() != this || report_index.row() < 0 || report_index.row() >= m_reports.size())
		return false;
	const auto &report = m_reports[report_index.row()];

	qf::core::sql::Query query;
	query.prepare(QStringLiteral("DELETE FROM reports WHERE path=:path"));
	query.bindValue(QStringLiteral(":path"), report.relativePath);
	if (!query.exec()) {
		if (error_text)
			*error_text = query.lastErrorText();
		return false;
	}
	if (query.numRowsAffected() < 1) {
		if (error_text)
			*error_text = tr("The report is not stored in the database.");
		return false;
	}
	m_reports[report_index.row()].databaseHash.clear();
	emitReportHashChanged(report_index);
	return true;
}

bool ReportsTableModel::restoreReportFromDb(const QModelIndex &report_index, QString *error_text)
{
	if (!report_index.isValid() || report_index.model() != this || report_index.row() < 0 || report_index.row() >= m_reports.size())
		return false;
	auto &report = m_reports[report_index.row()];

	qf::core::sql::Query query;
	query.prepare(QStringLiteral("SELECT data, hash FROM reports WHERE path=:path"));
	query.bindValue(QStringLiteral(":path"), report.relativePath);
	if (!query.exec()) {
		if (error_text)
			*error_text = query.lastErrorText();
		return false;
	}
	if (!query.next()) {
		if (error_text)
			*error_text = tr("The report is not stored in the database.");
		return false;
	}

	const QString file_path = QDir(m_reportsDir).filePath(report.relativePath);
	QFile file(file_path);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		if (error_text)
			*error_text = file.errorString();
		return false;
	}
	if (file.write(query.value("data").toByteArray()) < 0) {
		if (error_text)
			*error_text = file.errorString();
		return false;
	}
	report.databaseHash = query.value("hash").toString();
	emitReportHashChanged(report_index);
	return true;
}

bool ReportsTableModel::restoreReportFromResources(const QModelIndex &report_index, QString *error_text)
{
	if (!report_index.isValid() || report_index.model() != this || report_index.row() < 0 || report_index.row() >= m_reports.size())
		return false;
	auto &report = m_reports[report_index.row()];

	QFile resource_file(QStringLiteral(":/reports/") + report.relativePath);
	if (!resource_file.open(QIODevice::ReadOnly)) {
		if (error_text)
			*error_text = resource_file.errorString();
		return false;
	}
	QFile file(QDir(m_reportsDir).filePath(report.relativePath));
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		if (error_text)
			*error_text = file.errorString();
		return false;
	}
	auto data = resource_file.readAll();
	if (file.write(data) < 0) {
		if (error_text)
			*error_text = file.errorString();
		return false;
	}
	report.localHash = qf::gui::framework::ReportFileCache::dataHash(data);
	emitReportHashChanged(report_index);
	return true;
}

void ReportsTableModel::emitReportHashChanged(const QModelIndex &report_index)
{
	auto index1 = report_index.sibling(report_index.row(), ResourcesHashColumn);
	auto index3 = report_index.sibling(report_index.row(), DatabaseHashColumn);
	emit dataChanged(index1, index3);
}

void ReportsTableModel::setReports(QList<Report> reports)
{
	beginResetModel();
	m_reports = std::move(reports);
	endResetModel();
}

const ReportsTableModel::Report &ReportsTableModel::reportAt(int row) const
{
	static const Report default_report;
	if (row < 0 || row >= m_reports.size()) {
		return default_report;
	}
	return m_reports.at(row);
}

}
