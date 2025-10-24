#pragma once

#include "../guiglobal.h"

#include <qf/core/utils/table.h>
#include <qf/core/utils.h>

#include <QAbstractTableModel>

namespace qf {
namespace gui {

namespace utils {
class TreeTable;
}

namespace model {

class QFGUI_DECL_EXPORT TableModel : public QAbstractTableModel
{
	Q_OBJECT
	Q_PROPERTY(bool nullReportedAsString READ isNullReportedAsString WRITE setNullReportedAsString NOTIFY nullReportedAsStringChanged)

	QF_PROPERTY_IMPL2(QString, i, I, dColumnName, QStringLiteral("id"))
public:
	explicit TableModel(QObject *parent = nullptr);
private:
	typedef QAbstractTableModel Super;
public:
	enum ItemDataRole {FieldNameRole = Qt::UserRole+1,
					   FieldTypeRole, ColumnDefinitionRole,
					   RawValueRole, ValueIsNullRole, ValueIsDirtyRole,
					   SortRole,
					   FirstUnusedRole };
	//enum RecordEditMode {ModeView, ModeEdit, ModeInsert, ModeCopy, ModeDelete};
public:
	class QFGUI_DECL_EXPORT ColumnDefinition
	{
	private:
		class SharedDummyHelper {};
		class Data : public QSharedData
		{
		public:
			QString fieldName; //!< ID to pair ColumnDefinitions with fields
			int fieldIndex;
			QString caption;
			QString toolTip;
			//int initialSize; //!< initial width of column
			bool isReadOnly = false;
			bool isVirtual = false;
			Qt::Alignment alignment;
			QString format; //!< format for date, time, ... types
			int castType;
			QVariantMap castProperties;

			Data(const QString &fldname = QString()) : fieldName(fldname), fieldIndex(-1), castType(QMetaType::UnknownType) {}
		};
	private:
		QSharedDataPointer<Data> d;
		ColumnDefinition(SharedDummyHelper) {
			d = new Data();
		}
	public:
		static const ColumnDefinition& sharedNull();
		bool isNull() const {return d == sharedNull().d;}
	public:
		ColumnDefinition() {
			*this = sharedNull();
		}
		ColumnDefinition(const QString &fldname) {
			d = new Data(fldname);
		}
		ColumnDefinition(const QString &fldname, const QString &caption) {
			d = new Data(fldname);
			setCaption(caption);
		}

		QString fieldName() const {return d->fieldName;}
		ColumnDefinition& setFieldName(const QString &s) {d->fieldName = s; return *this;}
		int fieldIndex() const {return d->fieldIndex;}
		ColumnDefinition& setFieldIndex(int i) {d->fieldIndex = i; return *this;}
		QString caption() const {return d->caption.isEmpty()? fieldName(): d->caption;}
		ColumnDefinition& setCaption(const QString &s) {d->caption = s; return *this;}
		QString toolTip() const {return d->toolTip;}
		ColumnDefinition& setToolTip(const QString &s) {d->toolTip = s; return *this;}
		//int initialSize() const {return d->initialSize;}
		//ColumnDefinition& setInitialSize(int i) {d->initialSize = i; return *this;}
		bool isReadOnly() const {return d->isReadOnly;}
		ColumnDefinition& setReadOnly(bool b = true) {d->isReadOnly = b; return *this;}
		bool isVirtual() const {return d->isVirtual;}
		ColumnDefinition& setVirtual(bool b, int cast_type, const QVariantMap &cast_properties = QVariantMap())
		{
			d->isVirtual = b;
			setCastType(cast_type, cast_properties);
			return *this;
		}
		Qt::Alignment alignment() const {return d->alignment;}
		ColumnDefinition& setAlignment(const Qt::Alignment &al) {d->alignment = al; return *this;}
		QString format() const {return d->format;}
		/// for double see QString::number(...)
		/// for QTime see QTime::toString(...)
		/// for QDate see QDate::toString(...)
		ColumnDefinition& setFormat(const QString &s) {d->format = s; return *this;}

		int castType() const {return d->castType;}
		ColumnDefinition& setCastType(int t, const QVariantMap &cast_properties = QVariantMap())
		{
			d->castType = t;
			d->castProperties = cast_properties;
			return *this;
		}
		const QVariantMap& castProperties() const {return d->castProperties;}

		bool matchesSqlId(const QString column_name) const;
	};
	typedef QVector<ColumnDefinition> ColumnList;

public:
	void clearRows();
	void clearColumns(int new_column_count = 0);
	ColumnDefinition& addColumn(const QString &field_name, const QString &caption = QString());
	ColumnDefinition& insertColumn(int before_ix, const QString &field_name, const QString &_caption = QString());
	ColumnDefinition& insertColumn(int before_ix, const ColumnDefinition &cd);
	void setColumn(int ix, const ColumnDefinition &cd);
	ColumnDefinition removeColumn(int ix);
public:
	const qf::core::utils::Table& table() {return m_table;}
	qf::core::utils::Table& tableRef() {return m_table;}
	void setTable(const qf::core::utils::Table &t);

	Q_INVOKABLE int rowCount(const QModelIndex &parent = QModelIndex()) const Q_DECL_OVERRIDE;
	Q_INVOKABLE int columnCount(const QModelIndex &parent = QModelIndex()) const Q_DECL_OVERRIDE;
	Q_INVOKABLE Qt::ItemFlags flags(const QModelIndex &index) const Q_DECL_OVERRIDE;
	Q_INVOKABLE QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const Q_DECL_OVERRIDE;
	Q_INVOKABLE QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const Q_DECL_OVERRIDE;
	Q_INVOKABLE bool setData(const QModelIndex &index, const QVariant & value, int role = Qt::EditRole) Q_DECL_OVERRIDE;
	Q_INVOKABLE void restoreOrigData(const QModelIndex &index);

	void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) Q_DECL_OVERRIDE;

	Q_SLOT virtual bool reload();
	Q_SLOT virtual bool postRow(int row_no, bool throw_exc);
	Q_SLOT virtual bool postAll(bool throw_exc);
	Q_SLOT virtual void revertRow(int row_no);
	Q_SLOT virtual void revertAll();
	Q_SLOT virtual int reloadRow(int row_no);
	Q_SLOT virtual int reloadInserts(const QString &id_column_name) { Q_UNUSED(id_column_name) return 0;}
	int appendRow() {return insertRows(rowCount(), 1);}
	bool dropRow(int row_ix, bool throw_exc) {return dropRows(row_ix, 1, throw_exc);}
	bool prepareForCopyRow(int row_no);

	bool insertRows(int row_ix, int count, const QModelIndex &parent = QModelIndex()) Q_DECL_OVERRIDE;
	bool removeRows(int row_ix, int count, const QModelIndex &parent = QModelIndex()) Q_DECL_OVERRIDE
	{
		Q_UNUSED(parent)
		return dropRows(row_ix, count, qf::core::Exception::Throw);
	}
	virtual void cloneRow(int row_ix);
	virtual bool dropRows(int row_ix, int count, bool throw_exc);
	// used by TableView to delete row, when it was deleted externaly
	bool removeRowNoOverload(int row_ix, bool throw_exc);

	bool isNullReportedAsString() const { return m_nullReportedAsString; }
	void setNullReportedAsString(bool arg);
	Q_SIGNAL void nullReportedAsStringChanged(bool arg);

	//Q_SIGNAL void columnsAutoGenerated();
	Q_SIGNAL void reloaded();

	Q_INVOKABLE bool isEmpty() const {return rowCount() == 0;}
	Q_INVOKABLE virtual QVariant value(int row_ix, int column_ix) const;
	Q_INVOKABLE QVariant value(int row_ix, const QString& col_name) const;
	Q_INVOKABLE virtual QVariant origValue(int row_ix, int column_ix) const;
	Q_INVOKABLE QVariant origValue(int row_ix, const QString& col_name) const;
	Q_INVOKABLE virtual QVariantMap values(int row_ix) const;
	Q_INVOKABLE virtual bool isDirty(int row_ix, int column_ix) const;
	Q_INVOKABLE bool isDirty(int row_ix, const QString& col_name) const;

	Q_INVOKABLE virtual bool setDirty(int row, int column, bool d);
	Q_INVOKABLE bool setDirty(int row_ix, const QString& col_name, bool d);

	Q_INVOKABLE virtual bool setValue(int row, int column, const QVariant &val);
	Q_INVOKABLE bool setValue(int row_ix, const QString& col_name, const QVariant &val);

	Q_INVOKABLE virtual void restoreOrigValue(int row, int column);
	//Q_INVOKABLE void restoreOrigValue(int row_ix, const QString& col_name, const QVariant &val);

	qf::core::utils::TableRow& tableRowRef(int row_no);
	qf::core::utils::TableRow tableRow(int row_no) const;

	class QFGUI_DECL_EXPORT TreeTableExportOptions : public QVariantMap
	{
		QF_VARIANTMAP_FIELD2(bool, is, set, ExportRawValues, true)
		public:
			TreeTableExportOptions(const QVariantMap &m = QVariantMap()) : QVariantMap(m) {}
	};
	qf::core::utils::TreeTable toTreeTable(const QString& table_name = QString(), const QVariantList& exported_columns = QVariantList(), const TreeTableExportOptions &opts = TreeTableExportOptions()) const;
	Q_INVOKABLE QVariant toTreeTableData(const QString& table_name = QString(), const QVariantList& exported_columns = QVariantList()) const;

	ColumnDefinition columnDefinition(int ix) const;
	int columnIndex(const QString &column_name) const;
	int columnType(int column_index) const;

	int tableFieldIndex(int column_index) const;
	qf::core::utils::Table::Field tableField(int column_index) const;

	static QColor contrastTextColor(const QColor &background_color);
protected:
	virtual void checkColumns();
	void createColumnsFromTableFields();
	void fillColumnIndexes();

	virtual QVariant rawValueToEdit(int column_index, const QVariant &val) const;
	virtual QVariant editValueToRaw(int column_index, const QVariant &val) const;

	/// @returns: index of inserted line or -1
	virtual int insertTableRow(int before_row);
	virtual bool removeTableRow(int row_ix, bool throw_exc = false);
protected:
	qf::core::utils::Table m_table;
	ColumnList m_columns;
	bool m_columnsAutoGenerated = false;
	bool m_nullReportedAsString;
};

}}}

Q_DECLARE_METATYPE(qf::gui::model::TableModel::ColumnDefinition)

