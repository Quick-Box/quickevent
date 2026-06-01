#include "tablemodel.h"
#include "../style.h"

#include <qf/core/log.h>
#include <qf/core/assert.h>
#include <qf/core/utils.h>
#include <qf/core/utils/treetable.h>
#include <qf/core/sql/qxrecchng.h>

#include <QTime>
#include <QTimeZone>
#include <QColor>
#include <QPixmap>
#include <QIcon>
#include <algorithm>

namespace qfc = qf::core;
namespace qfu = qf::core::utils;
namespace qf::gui::model {

//=========================================
//        TableModel::ColumnDefinition
//=========================================
const TableModel::ColumnDefinition & TableModel::ColumnDefinition::sharedNull()
{
	static ColumnDefinition n = ColumnDefinition(SharedDummyHelper());
	return n;
}

bool TableModel::ColumnDefinition::matchesSqlId(const QString column_name) const
{
	return qf::core::Utils::fieldNameEndsWith(fieldName(), column_name);
}

//=========================================
// TableModel
//=========================================
namespace {
const auto DEFAULT_TIME_FORMAT = QStringLiteral("hh:mm:ss");
const auto DEFAULT_DATE_FORMAT = QStringLiteral("yyyy-MM-dd");
const auto DEFAULT_DATETIME_FORMAT = QStringLiteral("yyyy-MM-ddThh:mm:ss");
}

TableModel::TableModel(QObject *parent)
	: Super(parent)
	//, m_autoColumns(false)
	, m_nullReportedAsString(true)
{
}

void TableModel::clearRows()
{
	if(rowCount() > 0) {
		beginRemoveRows(QModelIndex(), 0, rowCount() - 1);
		m_table.clearRows();
		endRemoveRows();
	}
}

int TableModel::rowCount(const QModelIndex &parent) const
{
	Q_UNUSED(parent);
	return m_table.rowCount();
}

int TableModel::columnCount(const QModelIndex &parent) const
{
	Q_UNUSED(parent);
	return m_columns.count();
}

Qt::ItemFlags TableModel::flags(const QModelIndex &index) const
{
	Qt::ItemFlags flags = QAbstractTableModel::flags(index);
	if(index.isValid()) {
		bool can_edit = false;
		ColumnDefinition cd = m_columns.value(index.column());
		if(!cd.isNull()) {
			can_edit = !cd.isReadOnly();
			if(!cd.isVirtual()) {
				qfu::Table::Field field = tableField(index.column());
				if(!field.isNull()) {
					can_edit = can_edit && field.canUpdate();
					int type = columnType(index.column());
					// BLOB fields cannot be edited in grid.
					//can_edit = can_edit && (type != QVariant::ByteArray);
					if(type == QMetaType::Bool) {
						flags |= Qt::ItemIsUserCheckable;// << Qt::ItemIsEnabled;
					}
				}
				else {
					can_edit = false;
				}
			}
		}
		if(can_edit)
			flags |= Qt::ItemIsEditable;
		else
			flags &= ~Qt::ItemIsEditable;
		//qfInfo() << cd.fieldName() << flags << "checkable:" << (flags & Qt::ItemIsUserCheckable);
	}
	return flags;
}

QVariant TableModel::data(const QModelIndex &index, int role) const
{
	QVariant ret;
	if(!index.isValid())
		return ret;
	if(role == RawValueRole) {
		ret = value(index.row(), index.column());
	}
	else if(role == ValueIsDirtyRole) {
		ret = isDirty(index.row(), index.column());
	}
	else if(role == SortRole) {
		int type = columnType(index.column());
		if(type == QMetaType::Bool)
			ret = value(index.row(), index.column()).toBool();
		else
			ret = data(index, Qt::DisplayRole);
	}
	else if(role == Qt::DisplayRole) {
		ColumnDefinition cd = m_columns.value(index.column());
		if(cd.isNull()) {
			return QString("!%1").arg(index.column());
		}
		if(data(index, ValueIsNullRole).toBool()) {
			if(isNullReportedAsString())
				return QStringLiteral("null");
			return QVariant();
		}
		ret = data(index, Qt::EditRole);
		int type = columnType(index.column());
		if(type == QMetaType::UnknownType)
#if QT_VERSION_MAJOR >= 6
			type = ret.typeId(); /// pokud jsou sloupce virtualni (sloupce se pocitaji, nemusi byt pro ne definovan typ)
#else
			type = (int)ret.type(); /// pokud jsou sloupce virtualni (sloupce se pocitaji, nemusi byt pro ne definovan typ)
#endif
		if(type == QMetaType::QByteArray) {
			const static QString blob_string = "{blob %1%2}";
			QByteArray ba = ret.toByteArray();
			int size = ba.size();
			if(size < 64)
				ret = ba;
			else if(size < 1024)
				ret = blob_string.arg(size).arg(" B");
			else
				ret = blob_string.arg(size/1024).arg("kB");
		}
		else if(type == QMetaType::Bool) {
			/// display check
			ret = QString();
		}
		//else if(type == QVariant::Double) {
		//	qfInfo() << "dbl:" << ret << QString("%1").arg(ret.toDouble(), 0, 'f', 16);
		//}
		QString format = cd.format();
		if(format.isEmpty()) {
			if(type == QMetaType::QDate) {
				format = DEFAULT_DATE_FORMAT;
			}
			else if(type == QMetaType::QTime) {
				format = DEFAULT_TIME_FORMAT;
				//qfInfo() << "format" << format;
			}
			else if(type == QMetaType::QDateTime) {
				format = DEFAULT_DATETIME_FORMAT;
				//qfInfo() << "format" << format;
			}
		}
		if(!format.isEmpty()) {
			if(type == QMetaType::QTime) {
				QTime t = ret.toTime();
				ret = t.toString(format);
			}
			else if(type == QMetaType::QDate) {
				QDate d = ret.toDate();
				ret = d.toString(format);
			}
			else if(type == QMetaType::QDateTime) {
				QDateTime dt = ret.toDateTime();
				ret = dt.toLocalTime().toString(format);
			}
		}
	}
	else if(role == Qt::EditRole) {
		ret = data(index, RawValueRole);
		ret = rawValueToEdit(index.column(), ret);
	}
	else if (role == ValueIsNullRole) {
		ret = data(index, RawValueRole);
		return ret.isNull() || !ret.isValid();
	}
	else if (role == Qt::TextAlignmentRole) {
		const ColumnDefinition cd = m_columns.value(index.column());
		Qt::Alignment al = cd.alignment();
		if(!!al)
			ret = (int)al;
		else {
			if(!data(index, ValueIsNullRole).toBool()) {
				ret = data(index, RawValueRole);
#if QT_VERSION_MAJOR >= 6
				if(ret.typeId() > QMetaType::Bool && ret.typeId() <= QMetaType::Double)
#else
				if(ret.type() > QVariant::Bool && ret.type() <= QVariant::Double)
#endif
					ret = Qt::AlignRight;
				else
					ret = Qt::AlignLeft;
			}
		}
	}
	else if(role == Qt::ForegroundRole) {
		static const auto blue_color = isDarkTheme()? QColor(0x99ccff):  QColor(Qt::blue);

		int type = columnType(index.column());
		if(type == QMetaType::QByteArray) {
			return blue_color;
		}
		if(data(index, ValueIsNullRole).toBool()) {
			return blue_color;
		}
		ret = QVariant();
	}
	else if (role == Qt::BackgroundRole) {
		/// delegate does it
	}
	else if (role == Qt::CheckStateRole) {
		int type = columnType(index.column());
		if(type == QMetaType::Bool) {
			//qfInfo() << "BOOL";
			return (data(index, Qt::EditRole).toBool()? Qt::Checked: Qt::Unchecked);
		}
		return QVariant();
	}
	else if (role == Qt::ToolTipRole) {
		QString s = data(index, Qt::DisplayRole).toString();
		//s = "<pre>" + s + "</pre>"; /// kvuli chybe v Qt 4.6.3, kdy aplikace pada pokud je v textu za sebou 0x09 0x0A
		s.replace("\t\n", "\n");
		ret = s;
		//ret = data(index, Qt::DisplayRole);
		//qfInfo() << ret.toString();
	}
	else if(role == ColumnDefinitionRole) {
		ColumnDefinition cd = m_columns.value(index.column());
		ret = QVariant::fromValue(cd);
	}
	return ret;
}

QVariant TableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	//static QIcon ico_dirty(QPixmap(":/qf/core/images/pencil.png"));
	//static QIcon ico_filter(QPixmap(":/libqfgui/images/filter.png"));
	QVariant ret;
	if(orientation == Qt::Horizontal) {
		if(role == Qt::DisplayRole || role == Qt::EditRole) {
			ColumnDefinition cd = m_columns.value(section);
			QF_ASSERT(!cd.isNull(),
					  QString("Invalid horizontal section number: %1").arg(section),
					  return ret);
			ret = cd.caption();
		}
		else if(role == Qt::DecorationRole) {
		}
		else if(role == Qt::ToolTipRole) {
			ColumnDefinition cd = m_columns.value(section);
			ret = cd.toolTip();
		}
		else if (role == FieldTypeRole) {
			int type = columnType(section);
			return type;
		}
		else if(role == ColumnDefinitionRole) {
			ColumnDefinition cd = m_columns.value(section);
			ret = QVariant::fromValue(cd);
		}
		else if(role == FieldNameRole) {
			ret = m_columns.value(section).fieldName();
		}
	}
	else if(orientation == Qt::Vertical) {
		if(role == Qt::DisplayRole || role == Qt::EditRole) {
			qfu::TableRow r = m_table.row(section);
			if(!r.isNull() && r.isDirty()) {
				ret = QString();
			}
			else {
				ret = QVariant(section + 1);
			}
		}
		//else if(role == Qt::DecorationRole) {
		//	qfu::TableRow r = m_table.row(section);
		//	if(!r.isNull() && r.isDirty()) {
		//		return QVariant::fromValue(ico_dirty);
		//	}
		//}
	}
	else {
		ret = QAbstractTableModel::headerData(section, orientation, role);
	}
	return ret;
}

bool TableModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
	qfLogFuncFrame() << value << "role:" << role;
	bool ret = false;
	if(!index.isValid())
		return ret;
	if(role == Qt::EditRole) {
		QVariant val = editValueToRaw(index.column(), value);
		ret = setValue(index.row(), index.column(), val);
		if(ret)
			emit dataChanged(index, index);
	}
	else if(role == Qt::CheckStateRole) {
		//qfInfo() << __LINE__;
		bool chk_val = (value.toInt() != Qt::Unchecked);
		ret = setValue(index.row(), index.column(), chk_val);
		if(ret)
			emit dataChanged(index, index);
	}
	else if(role == ValueIsDirtyRole) {
		bool d = value.toBool();
		ret = setDirty(index.row(), index.column(), d);
	}
	else {
		ret = Super::setData(index, value, role);
	}
	return ret;
}

void TableModel::restoreOrigData(const QModelIndex &index)
{
	if(index.isValid()) {
		restoreOrigValue(index.column(), index.row());
		emit dataChanged(index, index);
	}
}

void TableModel::sort(int column, Qt::SortOrder order)
{
	int table_field_index = tableFieldIndex(column);
	QF_ASSERT(table_field_index >= 0,
			  tr("Cannot find table field index for column index: %1").arg(column),
			  return);
	qfu::Table::SortDef sd(table_field_index, order == Qt::AscendingOrder, !qfu::Table::SortDef::CaseSensitive);
	m_table.sort(sd);
	emit layoutChanged();
}

void TableModel::checkColumns()
{
	if(m_columns.isEmpty()) {
		createColumnsFromTableFields();
	}
	fillColumnIndexes();
}

bool TableModel::reload()
{
	beginResetModel();
	checkColumns();
	endResetModel();
	emit reloaded();
	return true;
}

bool TableModel::postRow(int row_no, bool throw_exc)
{
	qfLogFuncFrame() << row_no << throw_exc;
	qfu::TableRow &row = m_table.rowRef(row_no);
	row.clearEditFlags();
	return true;
}

bool TableModel::postAll(bool throw_exc)
{
	bool ok = true;
	for (int row_no = 0; row_no < rowCount(); ++row_no) {
		qfu::TableRow row = m_table.row(row_no);
		if(row.isDirty()) {
			ok = postRow(row_no, throw_exc);
			if(!ok)
				break;
		}
	}
	return ok;
}

void TableModel::revertRow(int row_no)
{
	qfu::TableRow &row = m_table.rowRef(row_no);
	row.restoreOrigValues();
	QModelIndex ix1 = index(row_no, 0);
	QModelIndex ix2 = index(row_no, columnCount() - 1);
	emit dataChanged(ix1, ix2);
}

void TableModel::revertAll()
{
	for (int row_no = 0; row_no < rowCount(); ++row_no) {
		qfu::TableRow row = m_table.row(row_no);
		if(row.isDirty()) {
			revertRow(row_no);
		}
	}
}

int TableModel::reloadRow(int row_no)
{
	qfu::TableRow &row = m_table.rowRef(row_no);
	row.clearEditFlags();
	QModelIndex ix1 = index(row_no, 0);
	QModelIndex ix2 = index(row_no, columnCount() - 1);
	emit dataChanged(ix1, ix2);
	return 1;
}

bool TableModel::prepareForCopyRow(int row_no)
{
	qfu::TableRow &row = m_table.rowRef(row_no);
	row.prepareForCopy();
	return true;
}

void TableModel::setNullReportedAsString(bool arg)
{
	if (m_nullReportedAsString != arg) {
		m_nullReportedAsString = arg;
		emit nullReportedAsStringChanged(arg);
	}
}

bool TableModel::setValue(int row, int column, const QVariant &val)
{
	bool ret = false;
	QF_ASSERT(m_table.isValidRowIndex(row),
			  tr("Invalid table row: %1").arg(row),
			  return ret);
	int table_field_index = tableFieldIndex(column);
	QF_ASSERT(table_field_index >= 0,
			  tr("Cannot find table field index for column index: %1").arg(column),
			  return ret);
	QF_ASSERT(m_table.isValidFieldIndex(table_field_index),
			  tr("Invalid table field index: %1").arg(table_field_index),
			  return ret);
	//QVariant v = val;
	//if(isNullReportedAsString() && v.toString() == qfc::Utils::nullValueString())
	//	v = QVariant();
	auto &r = m_table.rowRef(row);
	r.setValue(table_field_index, val);
	ret = true;

	return ret;
}

bool TableModel::setValue(int row_ix, const QString &col_name, const QVariant &val)
{
	int col_ix = columnIndex(col_name);
	QF_ASSERT(col_ix >= 0,
			  tr("Cannot find column index for name: '%1'").arg(col_name),
			  return false);
	return setValue(row_ix, col_ix, val);
}

void TableModel::restoreOrigValue(int row, int column)
{
	QF_ASSERT(m_table.isValidRowIndex(row),
			  tr("Invalid table row: %1").arg(row),
			  return);
	int table_field_index = tableFieldIndex(column);
	QF_ASSERT(table_field_index >= 0,
			  tr("Cannot find table field index for column index: %1").arg(column),
			  return);
	QF_ASSERT(m_table.isValidFieldIndex(table_field_index),
			  tr("Invalid table field index: %1").arg(table_field_index),
			  return);
	//QVariant v = val;
	//if(isNullReportedAsString() && v.toString() == qfc::Utils::nullValueString())
	//	v = QVariant();
	auto &r = m_table.rowRef(row);
	r.restoreOrigValue(table_field_index);
}

QVariant TableModel::value(int row_ix, int column_ix) const
{
	QVariant ret;
	int table_field_index = tableFieldIndex(column_ix);
	//if(table_field_index < 0)
	//	qfDebug() << "debug";
	QF_ASSERT(table_field_index >= 0,
			  tr("%2 Cannot find table field for column index: %1 (row index: %3)").arg(column_ix).arg(objectName()).arg(row_ix),
			  return ret);
	ret = m_table.row(row_ix).value(table_field_index);
	/// DO NOT foreget that SQL NULL values are represented by null QVariants of appropriate column type
	/// NULL String is represented by QVariant(QVariant::String) NOT by QVariant()
	//qfInfo() << row_ix << column_ix << "->" << ret.toString() << ret.typeName();
	return ret;
}

QVariant TableModel::value(int row_ix, const QString &col_name) const
{
	QVariant ret;
	int col_ix = columnIndex(col_name);
	if(col_ix < 0) {
		qfError() << tr("Cannot find column index for name: '%1'").arg(col_name);
		return ret;
	}
	return value(row_ix, col_ix);
}

QVariant TableModel::origValue(int row_ix, int column_ix) const
{
	QVariant ret;
	int table_field_index = tableFieldIndex(column_ix);
	QF_ASSERT(table_field_index >= 0,
			  tr("Cannot find table field for column index: %1").arg(column_ix),
			  return ret);
	ret = m_table.row(row_ix).origValue(table_field_index);
	/// DO NOT foreget that SQL NULL values are represented by null QVariants of appropriate columnt type
	/// NULL String is represented by QVariant(QVariant::String) NOT by QVariant()
	return ret;
}

QVariant TableModel::origValue(int row_ix, const QString &col_name) const
{
	QVariant ret;
	int col_ix = columnIndex(col_name);
	QF_ASSERT(col_ix >= 0,
			  tr("Cannot find column index for name: '%1'").arg(col_name),
			  return ret);
	return origValue(row_ix, col_ix);
}

QVariantMap TableModel::values(int row_ix) const
{
	QVariantMap ret;
	for (int i=0; i<columnCount(); ++i) {
		auto cd = columnDefinition(i);
		QString key;
		qf::core::Utils::parseFieldName(cd.fieldName(), &key);
		ret[key] = value(row_ix, i);
	}
	return ret;
}

bool TableModel::isDirty(int row_ix, int column_ix) const
{
	bool ret = false;
	int table_field_index = tableFieldIndex(column_ix);
	QF_ASSERT(table_field_index >= 0,
			  tr("Cannot find table field for column index: %1").arg(column_ix),
			  return ret);
	ret = m_table.row(row_ix).isDirty(table_field_index);
	/// DO NOT foreget that SQL NULL values are represented by null QVariants of appropriate columnt type
	/// NULL String is represented by QVariant(QVariant::String) NOT by QVariant()
	return ret;
}

bool TableModel::isDirty(int row_ix, const QString &col_name) const
{
	bool ret = false;
	int col_ix = columnIndex(col_name);
	QF_ASSERT(col_ix >= 0,
			  tr("Cannot find column index for name: '%1'").arg(col_name),
			  return ret);
	return isDirty(row_ix, col_ix);
}

bool TableModel::setDirty(int row, int column, bool d)
{
	QF_ASSERT(m_table.isValidRowIndex(row),
			  tr("Invalid table row: %1").arg(row),
			  return false);
	int table_field_index = tableFieldIndex(column);
	QF_ASSERT(table_field_index >= 0,
			  tr("Cannot find table field index for column index: %1").arg(column),
			  return false);
	QF_ASSERT(m_table.isValidFieldIndex(table_field_index),
			  tr("Invalid table field index: %1").arg(table_field_index),
			  return false);
	qfu::TableRow &r = m_table.rowRef(row);
	r.setDirty(table_field_index, d);
	return true;
}

bool TableModel::setDirty(int row_ix, const QString &col_name, bool d)
{
	int col_ix = columnIndex(col_name);
	QF_ASSERT(col_ix >= 0,
			  tr("Cannot find column index for name: '%1'").arg(col_name),
			  return false);
	return setDirty(row_ix, col_ix, d);
}

qf::core::utils::TableRow &TableModel::tableRowRef(int row_no)
{
	return m_table.rowRef(row_no);
}

qf::core::utils::TableRow TableModel::tableRow(int row_no) const
{
	return m_table.row(row_no);
}

void TableModel::createColumnsFromTableFields()
{
	clearColumns();
	m_columnsAutoGenerated = true;
	Q_FOREACH(auto fld, m_table.fields()) {
		QString caption;
		qfc::Utils::parseFieldName(fld.name(), &caption);
		addColumn(fld.name(), caption);
	}
}

void TableModel::fillColumnIndexes()
{
	for(int i=0; i<m_columns.count(); i++) {
		ColumnDefinition &c = m_columns[i];
		int ix = -1;
		if(!c.isVirtual()) {
			ix = m_table.fields().fieldIndex(c.fieldName());
			if(ix < 0)
				qfWarning() << "\tcolumn" << c.fieldName() << "not found in fields.";
		}
		c.setFieldIndex(ix);
	}
}

int TableModel::columnType(int column_index) const
{
	int ret = QMetaType::UnknownType;
	ColumnDefinition cd = m_columns.value(column_index);
	QF_ASSERT(!cd.isNull(),
			  tr("Invalid column index: %1").arg(column_index),
			  return ret);
	ret = cd.castType();
	if(ret == QMetaType::UnknownType) {
		if(cd.isVirtual()) {
			qfWarning() << "Virtual column shoul have cast type defined!";
		}
		else {
			qfu::Table::Field fld = tableField(column_index);
			QF_ASSERT(!fld.isNull(), tr("Invalid field for column index: %1").arg(column_index), return ret);
			ret = fld.type().id();
		}
	}
	//qfInfo() << cd.fieldName() << ret;
	return ret;
}

std::optional<int> TableModel::columnIndexOfTableField(const QString &field_name) const
{
	if (auto fld_ix = m_table.fields().fieldIndex(field_name); fld_ix >= 0) {
		for (int i = 0; i < m_columns.size(); ++i) {
			const auto &cd = m_columns[i];
			if (cd.fieldIndex() == fld_ix) {
				return i;
			}
		}
	}
	return {};
}

QColor TableModel::contrastTextColor(const QColor &background_color)
{
	return (qGray(background_color.rgb()) < 128)? QColor(Qt::white) : QColor(Qt::black);
}

void TableModel::handleQxRecChng(const core::sql::QxRecChng &recchng, QObject *source)
{
	// qfInfo() << __FUNCTION__ << source;
	if (source == this) {
		return;
	}
	auto find_row_by_id = [this](const QString &table_name, int id) {
		if (auto fld_ix = table().fields().fieldIndex(table_name + ".id"); fld_ix >= 0) {
			for (int i = 0; i < rowCount(); ++i) {
				if (table().row(i).value(fld_ix).toInt() == id) {
					return i;
				}
			}
		} else {
			qfWarning() << "cannot find table column:" << (table_name + ".id");
		}
		return -1;
	};
	if (recchng.op == qf::core::sql::RecOp::Update) {
		if (int row = find_row_by_id(recchng.table, recchng.id); row >= 0) {
			for (const auto &[k, v] : recchng.record.asKeyValueRange()) {
				if (auto col_ix = columnIndexOfTableField(recchng.table + '.' + k); col_ix.has_value()) {
					// Q_ASSERT(0 <= row && row < m_table.rowCount());
					const auto &cd = columnDefinition(col_ix.value());
					Q_ASSERT(cd.fieldIndex() >= 0);
					auto &r = m_table.rowRef(row);
					if (!r.isDirty(cd.fieldIndex())) {
						// do not update current edits
						qfMessage() << "updating orig value, row:" << row << (recchng.table + '.' + k) << "-->" << v;
						r.setBareBoneValue(cd.fieldIndex(), v);
						auto ix = index(row, col_ix.value());
						emit dataChanged(ix, ix);
					}
				} else {
					// calculated column
					qfInfo() << "reloading calculated field, row:" << row << (recchng.table + '.' + k) << "-->" << v;
					reloadRow(row);
				}
			}
		}
	}
}

QVariant TableModel::rawValueToEdit(int column_index, const QVariant &val) const
{
	Q_UNUSED(column_index);
	return val;
}

QVariant TableModel::editValueToRaw(int column_index, const QVariant &val) const
{
	Q_UNUSED(column_index);
	return val;
}

int TableModel::columnIndex(const QString &column_name) const
{
	int ret = -1, i = 0;
	for (const auto &cd : m_columns) {
		//qfInfo() << cd.fieldName() << "vs." << column_name;
		if(qfc::Utils::fieldNameEndsWith(cd.fieldName(), column_name)) {
			ret = i;
			break;
		}
		i++;
	}
	if(ret < 0) {
		QStringList sl;
		for (const auto &cd : m_columns) {
			sl << cd.fieldName();
		}
		QString s = sl.join(", ");
		QString msg = tr("Column named '%1' not found in column list. Existing columns: [%2]").arg(column_name).arg(s);
		qfError() << msg;
	}
	return ret;
}

int TableModel::tableFieldIndex(int column_index) const
{
	ColumnDefinition cd = m_columns.value(column_index);
	if (cd.isNull()) {
			  qfWarning() << "Invalid column index:" << column_index;
			  return -1;
	}
	return cd.fieldIndex();
}

qf::core::utils::Table::Field TableModel::tableField(int column_index) const
{
	qfu::Table::Field ret;
	int table_field_index = tableFieldIndex(column_index);
	//if(table_field_index < 0) for (int i = 0; i < columnCount(); ++i) {
	//	qfInfo() << i << columnDefinition(i).fieldName() << columnDefinition(i).fieldIndex();
	//}
	if(table_field_index < 0) {
		qfError() << QString("Cannot find field index for column index: %1 of %2 columns").arg(column_index).arg(columnCount());
		return ret;
	}
	ret = m_table.field(table_field_index);
	QF_CHECK(!ret.isNull(),
			 tr("Cannot find table field for column index: %1, table index: %2").arg(column_index).arg(table_field_index));
	return ret;
}

void TableModel::clearColumns(int new_column_count)
{
	if(m_columns.count()) {
		beginRemoveColumns(QModelIndex(), 0, m_columns.count() - 1);
		m_columns.clear();
		endRemoveColumns();
	}
	if(new_column_count > 0) {
		beginInsertColumns(QModelIndex(), 0, new_column_count - 1);
		m_columns = ColumnList(new_column_count);
		endInsertColumns();
	}
	m_columnsAutoGenerated = false;
}

qf::gui::model::TableModel::ColumnDefinition& TableModel::addColumn(const QString &field_name, const QString &caption)
{
	return insertColumn(m_columns.count(), field_name, caption);
}

TableModel::ColumnDefinition &TableModel::insertColumn(int before_ix, const QString &field_name, const QString &caption)
{
	ColumnDefinition cd = ColumnDefinition(field_name);
	cd.setCaption(caption);
	return insertColumn(before_ix, cd);
}

TableModel::ColumnDefinition &TableModel::insertColumn(int before_ix, const TableModel::ColumnDefinition &cd)
{
	if(before_ix < 0 || before_ix > m_columns.count())
		before_ix = m_columns.count();
	m_columns.insert(before_ix, cd);
	ColumnDefinition &c = m_columns[before_ix];
	return c;
}

void TableModel::setColumn(int ix, const TableModel::ColumnDefinition &cd)
{
	QF_ASSERT(ix >= 0 && ix < columnCount(), "invalid column index", return);
	m_columns[ix] = cd;
}

TableModel::ColumnDefinition TableModel::removeColumn(int ix)
{
	if(ix >= 0 && ix < m_columns.count()) {
		beginRemoveColumns(QModelIndex(), 0, ix);
		ColumnDefinition cd = m_columns.takeAt(ix);
		endRemoveColumns();
		return cd;
	}
	return ColumnDefinition();
}

void TableModel::setTable(const qf::core::utils::Table &t)
{
	beginResetModel();
	m_table = t;
	endResetModel();
}

int TableModel::insertTableRow(int before_row)
{
	qfLogFuncFrame() << "before_row:" << before_row << "row cnt:" << rowCount();
	if(before_row < 0 || before_row > rowCount())
		before_row = rowCount();
	m_table.insertRow(before_row);
	qfDebug() << "\t row cnt:" << rowCount();
	return before_row;
}

bool TableModel::insertRows(int row_ix, int count, const QModelIndex &parent)
{
	qfLogFuncFrame() << "row:" << row_ix << "count:" << count;
	if(count < 0)
		return false;
	row_ix = std::max(row_ix, 0);
	beginInsertRows(parent, row_ix, row_ix + count - 1);
	bool ok = true;
	for(int i=0; i<count; i++) {
		ok = insertTableRow(row_ix + i);
		if(!ok)
			break;
	}
	endInsertRows();
	return ok;
}

void TableModel::cloneRow(int row_ix)
{
	beginInsertRows(QModelIndex(), row_ix, row_ix + 1);
	insertTableRow(row_ix + 1);
	const qfu::TableRow &r1 = tableRow(row_ix);
	qfu::TableRow &r2 = tableRowRef(row_ix + 1);
	for(int i=0; i<r1.fieldCount() && i<r2.fieldCount(); i++) {
		QVariant v1 = r1.value(i);
		r2.setValue(i, v1);
		//qfInfo() << i << v1 << r2.value(i);
	}
	r2.prepareForCopy();
	endInsertRows();
}

bool TableModel::removeTableRow(int row_ix, bool throw_exc)
{
	Q_UNUSED(throw_exc);
	bool ok = m_table.removeRow(row_ix);
	return ok;
}

bool TableModel::dropRows(int row_ix, int count, bool throw_exc)
{
	qfLogFuncFrame() << "row:" << row_ix << "count:" << count;
	if(count < 0)
		return false;
	bool ok = true;
	beginRemoveRows(QModelIndex(), row_ix, row_ix + count - 1);
	for(int i=0; i<count; i++) {
		ok = removeTableRow(row_ix, throw_exc);
		if(!ok)
			break;
	}
	endRemoveRows();
	return ok;
}

bool TableModel::removeRowNoOverload(int row_ix, bool throw_exc)
{
	qfLogFuncFrame() << "row:" << row_ix;
	beginRemoveRows(QModelIndex(), row_ix, row_ix);
	bool ok = TableModel::removeTableRow(row_ix, throw_exc);
	endRemoveRows();
	return ok;
}

qfu::TreeTable TableModel::toTreeTable(const QString& table_name, const QVariantList& _exported_columns, const TreeTableExportOptions &opts) const
{
	qfu::TreeTable ret(table_name);
	QVariantList exported_columns = _exported_columns;
	if(exported_columns.isEmpty()) {
		for(int ix=0; ix<m_table.columnCount(); ix++) {
			QVariantMap col;
			col[QStringLiteral("index")] = ix;
			exported_columns << col;
		}
	}
	bool raw_values = opts.isExportRawValues();
	for(int i=0; i<exported_columns.count(); i++) {
		QVariantMap col = exported_columns[i].toMap();
		QString cap = col.value("caption").toString();
		int ix = col.value("index").toInt();
		qfu::TreeTableColumn cd;
		if(col.value("origin") == QLatin1String("table")) {
			auto t = m_table.field(ix).type().id();
			cd.setName(m_table.field(ix).name());
			cd.setType(t);
			cd.setHeader(cap);
			//ret.appendColumn(m_table.field(ix).name(), t, cap);
		}
		else {
			int t;
			if(raw_values) {
				qfu::Table::Field fld = tableField(ix);
				t = fld.type().id();
				//qfWarning() << fld.toString();
			}
			else {
				t = (QMetaType::Type)headerData(ix, Qt::Horizontal, FieldTypeRole).toInt();
			}
			cd.setName(headerData(ix, Qt::Horizontal, FieldNameRole).toString());
			if(t > 0)
				cd.setType(t);
			cd.setHeader(cap);
			//ret.appendColumn(headerData(ix, Qt::Horizontal, FieldNameRole).toString(), t, cap);
		}
		cd.setWidth(col.value("width"));
		ret.appendColumn(cd);
	}

	/// export data
	{
		for(int i=0; i<rowCount(); i++) {
			QVariantList row;
			qfu::Table::Row tbl_row = m_table.row(i);
			for(int j=0; j<exported_columns.count(); j++) {
				QVariantMap col = exported_columns[j].toMap();
				QVariant val;
				int ix = col.value("index").toInt();
				if(col.value("origin") == QLatin1String("table")) {
					val = tbl_row.value(ix);
					//qfWarning() << col << val.typeName() << "val:" << val.toString();
				}
				else {
					if(raw_values) {
						val = value(i, ix);
						//qfWarning() << col << val.typeName() << "val:" << val.toString();
					}
					else {
						QModelIndex mix = index(i, ix);
						val = data(mix);
						//qfWarning() << col << val.typeName() << "val:" << val.toString();
					}
				}
				row << val;
			}
			ret.appendRow(row);
		}
	}
	return ret;
}

QVariant TableModel::toTreeTableData(const QString &table_name, const QVariantList &exported_columns) const
{
	qfu::TreeTable tt = toTreeTable(table_name, exported_columns);
	//qfInfo() << tt.toString();
	QVariant ret = tt.toVariant();
	return ret;
}

TableModel::ColumnDefinition TableModel::columnDefinition(int ix) const
{
	return m_columns.value(ix);
}

}
