#ifndef REPORTSTABLEMODEL_H
#define REPORTSTABLEMODEL_H

#include <QAbstractTableModel>
#include <QList>
#include <QString>

namespace Core {

class ReportsTableModel : public QAbstractTableModel
{
	Q_OBJECT

public:
	enum Column {
		FileNameColumn,
		FileSizeColumn,
		OriginalHashColumn,
		CachedHashColumn,
		DatabaseHashColumn,
		SaveToDbColumn,
		ColumnCount
	};

	struct Report {
		QString filePath;
		QString relativePath;
		qint64 size = 0;
		QString originalHash;
		QString cachedHash;
		QString databaseHash;
	};

	explicit ReportsTableModel(QObject *parent = nullptr);

	int rowCount(const QModelIndex &parent = {}) const override;
	int columnCount(const QModelIndex &parent = {}) const override;
	QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
	Qt::ItemFlags flags(const QModelIndex &index) const override;

	void load();
	bool saveReportToDb(const QModelIndex &report_index, QString *error_text = nullptr);
	void setReports(QList<Report> reports);
	const Report &reportAt(int row) const;

private:
	QList<Report> m_reports;
};

}

#endif // REPORTSTABLEMODEL_H
