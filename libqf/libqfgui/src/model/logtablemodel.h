#pragma once

#include "../guiglobal.h"

#include <qf/core/utils.h>
#include <qf/core/logentrymap.h>

#include <necrolog/necrologlevel.h>

#include <QAbstractTableModel>

namespace qf::core { class LogEntryMap; }

namespace qf {
namespace gui {
namespace model {

class QFGUI_DECL_EXPORT  LogTableModel : public QAbstractTableModel
{
	Q_OBJECT
private:
	using Super = QAbstractTableModel;
public:
	enum Cols {TimeStamp, Severity, Category, Message, File, Line, Function, UserData, Count};
	class QFGUI_DECL_EXPORT Row {
	public:
		explicit Row() {}
		explicit Row(NecroLogLevel severity, const QString& domain, const QString& file, int line, const QString& msg, const QDateTime& time_stamp, const QString& function = QString(), const QVariant &user_data = QVariant());

		QVariant value(int col) const;
		void setValue(int col, const QVariant &v);
	private:
		QVector<QVariant> m_data;
	};
public:
	LogTableModel(QObject *parent = nullptr);

	enum class Direction {AppendToTop, AppendToBottom};

	QF_PROPERTY_IMPL2(Direction, d, D, irection, Direction::AppendToBottom)

	int maximumRowCount() const { return m_maximumRowCount; }
	Q_SLOT bool setMaximumRowCount(int maximum_row_count);
	Q_SIGNAL void maximumRowCountChanged(const int &maximum_row_count);
	void removeRows(int remove_count);

	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const Q_DECL_OVERRIDE;
	int rowCount(const QModelIndex &parent = QModelIndex()) const Q_DECL_OVERRIDE;
	int columnCount(const QModelIndex &parent = QModelIndex()) const Q_DECL_OVERRIDE;
	QVariant data(const QModelIndex &index, int role) const Q_DECL_OVERRIDE;

	virtual void clear();
	Row rowAt(int row) const;
	Q_SLOT void addLogEntry(const core::LogEntryMap &le);
	void addLog(NecroLogLevel severity, const QString& category, const QString &file, int line, const QString& msg, const QDateTime& time_stamp, const QString &function = QString(), const QVariant &user_data = QVariant());
	void addRow(const Row &row);
	Q_SIGNAL void logEntryInserted(int row_no);
protected:
	virtual QString prettyFileName(const QString &file_name);
private:
	int m_maximumRowCount = 10000;
protected:
	QList<Row> m_rows;
};

}}}
