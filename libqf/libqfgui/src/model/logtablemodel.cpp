#include "logtablemodel.h"

#include <qf/gui/style.h>
#include <qf/core/logentrymap.h>
#include <qf/core/log.h>

#include <QColor>
#include <QDateTime>

Q_DECLARE_METATYPE(NecroLog::Level)

namespace qf::gui::model {

LogTableModel::Row::Row(NecroLog::Level severity, const QString &domain, const QString &file, int line, const QString &msg, const QDateTime &time_stamp, const QString &function, const QVariant &user_data)
{
	m_data.resize(Cols::Count);
	m_data[Cols::Severity] = QVariant::fromValue(severity);
	m_data[Cols::Category] = domain;
	m_data[Cols::File] = file;
	m_data[Cols::Line] = line;
	m_data[Cols::Function] = function;
	m_data[Cols::Message] = msg;
	m_data[Cols::TimeStamp] = time_stamp;
	m_data[Cols::UserData] = user_data;
}

QVariant LogTableModel::Row::value(int col) const
{
	QVariant val = m_data.value(col);
	return val;
}

void LogTableModel::Row::setValue(int col, const QVariant &v)
{
	while (m_data.count() <= col)
		m_data.append(QVariant());
	m_data[col] = v;
}

LogTableModel::LogTableModel(QObject *parent)
	: Super(parent)
{
}

QVariant LogTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
		switch (section) {
		case Cols::Category:
			return tr("Category");
		case Cols::File:
			return tr("File");
		case Cols::Line:
			return tr("Line");
		case Cols::Severity:
			return tr("Severity");
		case Cols::TimeStamp:
			return tr("Time stamp");
		case Cols::Message:
			return tr("Message");
		case Cols::Function:
			return tr("Function");
		case Cols::UserData:
			return tr("Data");
		};
		return Super::headerData(section, orientation, role);
	}
	return Super::headerData(section, orientation, role);
}

int LogTableModel::rowCount(const QModelIndex &parent) const
{
	Q_UNUSED(parent)
	return m_rows.count();
}

int LogTableModel::columnCount(const QModelIndex &parent) const
{
	Q_UNUSED(parent)
	return Cols::Count;
}

QVariant LogTableModel::data(const QModelIndex &index, int role) const
{
	if (index.row() < 0 || index.row() >= rowCount()) {
		return QVariant();
	}
	switch (role) {
	case Qt::DisplayRole: {
		QVariant ret = data(index, Qt::EditRole);
		if(ret.userType() == qMetaTypeId<NecroLog::Level>())
			ret = NecroLog::levelToString(ret.value<NecroLog::Level>());
		else if(ret.userType() == qMetaTypeId<QDateTime>())
			ret = ret.value<QDateTime>().toString(Qt::ISODateWithMs);
		return ret;
	}
	case Qt::EditRole:
		return m_rows[index.row()].value(index.column());
	case Qt::ToolTipRole:
		return data(index, Qt::DisplayRole);
	case Qt::ForegroundRole: {
		static const auto contrast_color = isDarkTheme()? QColor(Qt::lightGray): QColor(Qt::black);
		static const auto blue_color = isDarkTheme()? QColor(0x99ccff): QColor(Qt::blue);

		auto severity = m_rows[index.row()].value(Cols::Severity).value<NecroLog::Level>();
		switch (severity) {
		case NecroLog::Level::Error:
		case NecroLog::Level::Warning:
		case NecroLog::Level::Message:
			return contrast_color;
		case NecroLog::Level::Info:
			return blue_color;
		default:
			return QVariant();
		}
	}
	case Qt::BackgroundRole: {
		static const auto error_color = isDarkTheme()? QColor(Qt::red).darker(170): QColor(Qt::red).lighter(170);
		static const auto warning_color = isDarkTheme()? QColor("orange").darker(170):  QColor("orange").lighter(170);
		auto severity = m_rows[index.row()].value(Cols::Severity).value<NecroLog::Level>();
		switch (severity) {
		case NecroLog::Level::Invalid:
		case NecroLog::Level::Fatal:
		case NecroLog::Level::Error:
			return error_color;
		case NecroLog::Level::Warning:
			return warning_color;
		default:
			return QVariant();
		}
	}
	};
	return QVariant();
}

void LogTableModel::clear()
{
	beginResetModel();
	m_rows.clear();
	endResetModel();
}

LogTableModel::Row LogTableModel::rowAt(int row) const
{
	return m_rows.value(row);
}

void LogTableModel::addLogEntry(const qf::core::LogEntryMap &le)
{
	addLog(le.level(), le.category(), le.file(), le.line(), le.message(), le.timeStamp(), le.function());
}

void LogTableModel::addLog(NecroLog::Level severity, const QString &category, const QString &file, int line, const QString &msg, const QDateTime &time_stamp, const QString &function, const QVariant &user_data)
{
	QString module = prettyFileName(file);
	Row row(severity, category, module, line, msg, time_stamp, function, user_data);
	addRow(row);
}

void LogTableModel::addRow(const LogTableModel::Row &row)
{
	//qfInfo() << "add row:" << row.value(Cols::Message);
	static constexpr int ROWS_OVERLAP = 100;
	if(rowCount() >= maximumRowCount() + ROWS_OVERLAP) {
		if(direction() == Direction::AppendToBottom) {
			beginRemoveRows(QModelIndex(), 0, ROWS_OVERLAP - 1);
			m_rows = m_rows.mid(ROWS_OVERLAP);
			endRemoveRows();
		}
		else {
			beginRemoveRows(QModelIndex(), rowCount() - ROWS_OVERLAP, rowCount() - 1);
			m_rows = m_rows.mid(0, rowCount() - ROWS_OVERLAP);
			endRemoveRows();
		}
	}
	if(direction() == Direction::AppendToBottom) {
		beginInsertRows(QModelIndex(), rowCount(), rowCount());
		m_rows.append(row);
		endInsertRows();
		emit logEntryInserted(rowCount() - 1);
	}
	else {
		beginInsertRows(QModelIndex(), 0, 0);
		m_rows.insert(0, row);
		endInsertRows();
		emit logEntryInserted(0);
	}
}

QString LogTableModel::prettyFileName(const QString &file_name)
{
	return QString::fromStdString(NecroLog::moduleFromFileName(file_name.toStdString().c_str()));
}

}
