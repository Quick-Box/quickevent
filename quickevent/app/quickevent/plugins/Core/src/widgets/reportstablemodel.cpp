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

#include <utility>

namespace Core {

namespace {

QString fileHash(const QString &file_path)
{
	QFile file(file_path);
	if (!file.open(QIODevice::ReadOnly))
		return {};
	return QString::fromLatin1(QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha1).toHex());
}

}

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
		case OriginalHashColumn: return report.originalHash;
		case CachedHashColumn: return report.cachedHash;
		case DatabaseHashColumn: return report.databaseHash;
		case SaveToDbColumn: return tr("Save");
		default: break;
		}
		return {};
	}
	if (role == Qt::DisplayRole) {
		switch (index.column()) {
		case OriginalHashColumn: return report.originalHash.mid(0, 8);
		case CachedHashColumn: return report.cachedHash.mid(0, 8);
		case DatabaseHashColumn: return report.databaseHash.mid(0, 8);
		default: return data(index, Qt::EditRole);
		}
	}
	if (role == Qt::ToolTipRole) {
		switch (index.column()) {
		case OriginalHashColumn:
		case CachedHashColumn:
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
		case OriginalHashColumn: return tr("Original hash");
		case CachedHashColumn: return tr("Cached hash");
		case DatabaseHashColumn: return tr("Database hash");
		case SaveToDbColumn: return tr("Save to DB");
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
	const auto dir = qf::gui::framework::Plugin::reportFileCache()->effectiveReportsDir();
	QList<Report> reports;
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

	const QDir reports_dir(dir);
	QDirIterator iterator(dir, QDir::Files | QDir::Hidden | QDir::System, QDirIterator::Subdirectories);
	while (iterator.hasNext()) {
		const QFileInfo file_info(iterator.next());
		const QString relative_path = reports_dir.relativeFilePath(file_info.filePath());
		reports.append(
			Report{
				.filePath = file_info.filePath(),
				.relativePath = relative_path,
				.size = file_info.size(),
				.originalHash = fileHash(QStringLiteral(":/reports/") + relative_path),
				.cachedHash = fileHash(file_info.filePath()),
				.databaseHash = database_hashes.value(relative_path)});
	}
	setReports(std::move(reports));
}

void ReportsTableModel::setReports(QList<Report> reports)
{
	beginResetModel();
	m_reports = std::move(reports);
	endResetModel();
}

const ReportsTableModel::Report &ReportsTableModel::reportAt(int row) const
{
	return m_reports.at(row);
}

}
