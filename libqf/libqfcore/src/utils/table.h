#ifndef QF_CORE_UTILS_TABLE_H
#define QF_CORE_UTILS_TABLE_H

#include "../core/coreglobal.h"
#include "../core/utils.h"
#include "../core/collator.h"

#include <QString>
#include <QVariantMap>
#include <QList>
#include <QBitArray>

class QDomElement;
class QDomDocument;
class QTextStream;

namespace qf {
namespace core {
namespace utils {

class TreeTable;

class TableRow;
typedef QList<TableRow> RowList;

class QFCORE_DECL_EXPORT Table
{
	//Q_DECLARE_TR_FUNCTIONS(qf::core::utils::Table);
public:
	class FieldList;
	Table();
	Table(const QStringList &col_names);
	Table(const FieldList &col_defs);
	virtual ~Table();
public:
	typedef TableRow Row;
	enum CleanupDataOption {ClearFieldsRows = 1, ClearRows};
	class QFCORE_DECL_EXPORT TextImportOptions : public QVariantMap
	{
	public:
		QF_VARIANTMAP_FIELD2(QString, f, setF, ieldSeparator, ",")
		QF_VARIANTMAP_FIELD2(QString, f, setF, ieldQuotes, "\"")
		QF_VARIANTMAP_FIELD2(bool, isI, setI, mportColumnNames, true)
		QF_VARIANTMAP_FIELD2(bool, isI, setI, mportAppend, false)
		QF_VARIANTMAP_FIELD2(int, i, setI, gnoreFirstLinesCount, 0)
		QF_VARIANTMAP_FIELD2(bool, isT, setT, rimValues, true)
	};
	class QFCORE_DECL_EXPORT TextExportOptions : public QVariantMap
	{
	public:
		enum FieldQuoting {Never=1, IfNecessary, Always};
	public:
		QF_VARIANTMAP_FIELD2(QString, f, setF, ieldSeparator, ",")
		QF_VARIANTMAP_FIELD2(QString, f, setF, ieldQuotes, "\"")
		QF_VARIANTMAP_FIELD2(bool, isE, setE, xportColumnNames, true)
		QF_VARIANTMAP_FIELD2(bool, isF, setF, ullColumnNames, true)
		QF_VARIANTMAP_FIELD2(bool, isU, setU, seColumnCaptions, true)

		QVariantMap columnCaptions() const {return value("columnCaptions").toMap();}
		TextExportOptions& setColumnCaptions(const QVariantMap &cc) {this->operator[]("columnCaptions") = cc; return *this;}

		FieldQuoting fieldQuotingPolicy() const {return static_cast<FieldQuoting>(value("fieldQuotingPolicy", IfNecessary).toInt());}
		TextExportOptions& setFieldQuotingPolicy(FieldQuoting fq) {this->operator[]("fieldQuotingPolicy") = fq; return *this;}

		//QF_VARIANTMAP_FIELD(FieldQuoting, f, setF, ieldQuotingPolicy, IfNecessary)
		QF_VARIANTMAP_FIELD(QString, c, setC, odecName)
		QF_VARIANTMAP_FIELD2(int, f, setF, romLine, 0)
		QF_VARIANTMAP_FIELD2(int, t, setT, oLine, -1)
		QF_VARIANTMAP_FIELD(QString, n, setN, ote)
		/// pri exportu do XLS se vlozi tabulka slozena z radku XLS prologu
		QF_VARIANTMAP_FIELD(QVariantList, x, setX, lsProlog)
		public:
			TextExportOptions(const QVariantMap &m = QVariantMap()) : QVariantMap(m) {}
	};
public:
	//! Sorting information fo one field.
	struct QFCORE_DECL_EXPORT SortDef
	{
		static const bool Ascending = true;
		static const bool CaseSensitive = true;
		static const bool Ascii7Bit = true;
		int fieldIndex; //!< index of field to sort.
		int ascending:1, caseSensitive:1, ascii7bit:1; //!< convert values to ASCII 7 bit for before sorting.

		bool isValid() const {return fieldIndex >= -1;}
		SortDef(int ix = -2, bool asc = Ascending, bool cs = CaseSensitive, bool _ascii7bit = !Ascii7Bit)
			: fieldIndex(ix), ascending(asc), caseSensitive(cs), ascii7bit(_ascii7bit) {}
	};
	typedef QList<SortDef> SortDefList;
public:
	class QFCORE_DECL_EXPORT LessThan
	{
	public:
		LessThan(const Table &t);
		virtual ~LessThan() {}
	public:
		inline bool operator()(int r1, int r2) const { return lessThan(r1, r2); }
		inline bool operator()(int r, const QVariant &v) const { return lessThan_helper(r, v, false); }
		inline bool operator()(const QVariant &v, int r) const { return lessThan_helper(r, v, true); }
	protected:
		bool lessThan(int r1, int r2) const;
		bool lessThan_helper(int _row, const QVariant &v, bool switch_params) const;
		virtual int cmp(const QVariant &l, const QVariant &r, const Table::SortDef &sd) const;
	protected:
		const Table &table;
		Table::SortDefList sortedFields;
		qf::core::Collator sortCollator;
	};
protected:
	typedef QList<int> RowIndexList;
public:
	class QFCORE_DECL_EXPORT Field
	{
	private:
		class SharedDummyHelper {};
		class Data : public QSharedData
		{
		public:
			int userType;
			QString name;
			unsigned canUpdate:1, isPriKey:1, isSerial:1, isNullable:1;
			Data() : userType(QMetaType::UnknownType) {}
			Data(const QString &name, int t) : userType(t), name(name), canUpdate(false), isPriKey(false), isNullable(false) {}
		};
		QSharedDataPointer<Data> d;

		Field(SharedDummyHelper); /// null row constructor
		static const Field& sharedNull();
	public:
		Field();
		Field(const QString &name, int user_type);

		bool isNull() const {return d == sharedNull().d;}

		QString shortName() const;
		/// [schema.]table
		QString tableId() const;
		QString toString() const;

		QMetaType type() const {return QMetaType(d->userType);}
		void setType(QMetaType::Type t) {d->userType = t;}
		void setUserType(int t) {d->userType = t;}
		QF_SHARED_CLASS_FIELD_RW(QString, n, setN, ame)
		QF_SHARED_CLASS_BIT_FIELD_RW(bool, c, setC, anUpdate)
		QF_SHARED_CLASS_BIT_FIELD_RW(bool, is, set, PriKey)
		QF_SHARED_CLASS_BIT_FIELD_RW(bool, is, set, Serial)
		//QF_SHARED_CLASS_FIELD_RW(bool, is, set, Nullable)
	};
	class QFCORE_DECL_EXPORT FieldList : public QList<Field>
	{
	public:
		/**
			finds index in field list, containing column \a field_name
			@param field_name fieldname in form [tablename.]fieldname
			@return field index or value lower than zero
			 */
		int fieldIndex(const QString &field_name) const;
		bool isValidFieldIndex(int fld_ix) const;
	};
public:
	class QFCORE_DECL_EXPORT TableProperties
	{
	private:
		class Data : public QSharedData
		{
		public:
			FieldList fields;
			SortDefList sortedFields; //!< Each sorted field has one entry in this list.
		public:
			Data() {}
			~Data() {}
		};
		QSharedDataPointer<Data> d;
		class SharedDummyHelper {};
		TableProperties(SharedDummyHelper);
	public:
		static const TableProperties& sharedNull();
		bool isNull() const {return d == sharedNull().d;}

		FieldList& fieldsRef() {return d->fields;}
		const FieldList& fields() const {return d->fields;}

		const SortDefList& sortDefinition() const {return d->sortedFields;}
		SortDefList& sortDefinitionRef() {return d->sortedFields;}
		void setSortDefinition(const SortDefList& sdl) {d->sortedFields = sdl;}

		bool operator==(const TableProperties &other) const {return d == other.d;}
	public:
		TableProperties();
	};
private:
	class Data : public QSharedData
	{
	public:
		//! Properties of table usefull for TableRow (fields, sorting info etc.). Each row has implicitly shared copy of this struct.
		TableProperties tableProperties;
		RowList rows; ///< copy of ResultSet
		//QStringList tableids; ///< list of tablenames in active query
		RowIndexList rowIndex; ///< rows are accessed through this index (sorting, filtering, ...)
		//int currentRow; ///< index of current row in \a index
		SortDefList sortedFields;
		Collator sortCollator;
	public:
		Data();
		~Data() { }
	};
	class SharedDummyHelper {};
	Table(SharedDummyHelper); /// null row constructor
	static const Table& sharedNull();
	QSharedDataPointer<Data> d;
public:
	bool isNull() const {return d == sharedNull().d;}
	const TableProperties& tableProperties() const {return d->tableProperties;}
	const FieldList& fields() const {return tableProperties().fields();}
	//const RowList& rows() const {return d->rows;}

	QVariant sumValue(int field_ix) const;
	QVariant sumValue(const QString &field_name) const
	{
		return sumValue(fields().fieldIndex(field_name));
	}
private:
	/// unsorted, unfiltered table rows
	const RowList& rows() const {return d->rows;}
	RowList& rowsRef() {return d->rows;}
	//! clears all rows, if \a fields_options tells what else will be cleared.
	void cleanupData(CleanupDataOption fields_options);
	int rowNumberToRowIndex(int rowno) const;
	void createRowIndex();
	const RowIndexList& rowIndex() const {return d->rowIndex;}
	RowIndexList& rowIndexRef() {return d->rowIndex;}
public:
	TableProperties& tablePropertiesRef() {return d->tableProperties;}
	FieldList& fieldsRef() {return tablePropertiesRef().fieldsRef();}
public:
	Field& fieldRef(int fld_ix);
	Field& fieldRef(const QString& field_name);
	Field field(int fld_ix) const;
	Field field(const QString& field_name) const
	{
		return field(fields().fieldIndex(field_name));
	}

	TableRow& rowRef(int rowno);
	TableRow row(int i) const;
	TableRow lastRow() const;
	//static TableRow nullRow() {return TableRow::sharedNull();}
public:
	//! Clear all except column definitions.
	void clearRows() {cleanupData(ClearRows);}
	//! frees all allocated resources (fields, columns, rows, indexes, etc.)
	void clear() {cleanupData(ClearFieldsRows);}

	bool isEmpty() const {return rowCount() <= 0;}
	bool isValidRowIndex(int row) const;
	bool isValidFieldIndex(int fld_ix) const {
		return fields().isValidFieldIndex(fld_ix);
	}
public:
	virtual int columnCount() const;
	Field& insertColumn(int ix, const QString &name, QMetaType::Type t);
	Field& appendColumn(const QString &name, QMetaType::Type t) {return insertColumn(columnCount(), name, t);}

	//! return empty row which is not inserted in the table rows
	TableRow isolatedRow() const;
	virtual int rowCount() const;
	TableRow& insertRow(int before_row);
	virtual TableRow& insertRow(int before_row, const TableRow &_row);
	TableRow& appendRow() {return insertRow(rowCount());}
	TableRow& appendRow(const TableRow &_row) {return insertRow(rowCount(), _row);}
	virtual bool removeRow(int ri);
	// other related
	void revertRow(int ri);
	virtual void revertRow(TableRow &r);
protected:
	virtual void sort(RowIndexList::iterator begin, RowIndexList::iterator end);
	virtual RowIndexList::const_iterator binaryFind(RowIndexList::const_iterator begin, RowIndexList::const_iterator end, const QVariant &val) const;
public:
	void setSortCollator(const qf::core::Collator &coll);
	qf::core::Collator sortCollator() const;
	/// sort / search / seek
	//! @param colnames Coma separaded list of fields with optional ASC or DES  and CS or ICS flag.
	//!		CS is CaseSensitive, default is CS.
	//!						For example "id, name DESC CS, salary"
	void sort(const QString &colnames);
	//! For convinience.
	void sort(const SortDef &sorted_field);
	void sort(const SortDefList &sorted_fields);
	//! sort certain rows range only
	virtual void sort(const SortDefList &sorted_fields, int start_row_index, int row_count);
	void resort();
	void setSortDefinition(const SortDefList & sorted_fields) {tablePropertiesRef().setSortDefinition(sorted_fields);}
	//! returns index of \a val or -1. Table must be sorted ascending.
	//! Function seeks in the first sorted field.
	//! If val is QVariantList, function seek in all val values up to sortedFields.count()
	int seek(const QVariant &val) const;
	/// Prochazi tabulku radek po radku, pokus nic nenajde vraci -1
	int find(int field_ix, const QVariant &val) const;
	int find(const QString &field_name, const QVariant &val) const;
public:
	// export / import
	QString toString() const;
	static const QString CVSTableEndTag;
	static QString quoteCSV(const QVariant &val, const TextExportOptions &opts);
	void exportCSV(QTextStream &ts, const QString col_names = "*", TextExportOptions opts = TextExportOptions()) const;
	void importCSV(QTextStream &ts, TextImportOptions opts = TextImportOptions());
#ifdef TXT_EXPORT_IMPORT
	void importTXT(QTextStream &ts, const QString &file_structure_definition);
#endif
	//void exportExcelXML(QTextStream &ts, const QString col_names = QString(), TextExportOptions opts = TextExportOptions()) const;

	QDomElement toHtmlElement(QDomDocument &owner_doc, const QString & col_names = QString(), TextExportOptions opts = TextExportOptions()) const;
	//QDomElement exportXML(const QString col_names = "*") const;
	//QFXmlTable toXmlTable(QDomDocument &owner_doc, const QString &table_name = QString(), const QString &col_names = QString()) const;
	QVariantMap toJson(const QString &col_names = QString()) const;
	qf::core::utils::TreeTable toTreeTable(const QString &col_names = QString(), const QString &table_name = QString()) const;
	/// v pripade neuspechu vraci false
	bool fromTreeTable(const TreeTable &tree_table);
	/// ulozi data v tabulce jako QVariantList QVariantListu (kazdy radek je jeden QVariantList)
	QVariantList dataToVariantList() const;
	void dataFromVariantList(const QVariantList &_lst);
};

//! One row in table, implicitly shared.
class QFCORE_DECL_EXPORT TableRow
{
public:
	TableRow();
	TableRow(const Table::TableProperties &props);
private:
	class SharedDummyHelper {};
	class QFCORE_DECL_EXPORT Data : public QSharedData
	{
		friend class TableRow;
	public:
		QList<QVariant> values;
		QList<QVariant> origValues;
		QBitArray dirtyFlags; ///< jsou situace, kdy je treba oznacit field jako dirty a pritom origValue a value jsou stejne
		Table::TableProperties tableProperties;
		struct Flags {
			bool insert:1;
			//bool forcedInsert:1;
		} flags;
	public:
		Data();
		Data(const Table::TableProperties &props);
		~Data();
	};
private:
	QSharedDataPointer<Data> d;
	static const TableRow& sharedNull();
	TableRow(SharedDummyHelper);
public:
	//void initValues();
	void saveValues();
	void restoreOrigValues();
	void clearEditFlags();
	void clearOrigValues();
	//! Uvede radek do ModeInsert a nastavi vsem fieldum dirty flag
	void prepareForCopy();
public:
	QVariant origValue(int col) const;
	QVariant origValue(const QString &field_name) const;
	//bool hasNullFlag(int col) const;
	QVariant value(int col) const;
	QVariant value(const QString &field_name) const;

	//QList<QVariant>& valuesRef() {return d->values;}
	const QList<QVariant>& values() const {return d->values;}
	QVariantMap valuesMap(bool full_names = false) const;
	QVariantMap dirtyValuesMap() const;

	//! Dirty flag nastavi, jen kdyz je value jina, nez ta, co uz tam byla.
	void setValue(int col, const QVariant &v);
	void setValue(const QString &field_name, const QVariant &v);
	void restoreOrigValue(int col);
	//! Set value without retyping and checks, useful only when table data are loaded from QSL query or something like that.
	/// Very fast and very dangerous function
	void setBareBoneValue(int col, const QVariant &val);
	/// when table column is inserted, all table rows values should be inserted too
	void insertInitValue(int ix);

	bool isDirty() const;
	bool isDirty(int field_no) const;
	void setDirty(int field_no, bool val = true);


	//! returns number of fields in the row.
	int fieldCount() const;
	//! returns number of values in the row, Shoul be the same as \a fieldCount() .
	int count() const;
	bool isInsert() const;
	void setInsert(bool b);
	//bool isForcedInsert() const {return d->flags.forcedInsert;}
	/// forcedInsert se pouziva v pripadech, kdy se data zaznamu nachazi ve vice linkovanych tabulkach a behem loadData() se zjisti, ze zaznam v nekterych tabulkach chybi
	/// viz. napr. PPKlientKartaDataFormDocument::loadData()
	//void setForcedInsert(bool b = true) {d->flags.forcedInsert = b;}
	bool isNull() const {return d == sharedNull().d;}
	const Table::TableProperties& tableProperties() const {return d->tableProperties;}
	void setTableProperties(const Table::TableProperties &tp) {d->tableProperties = tp;}
	const Table::FieldList& fields() const {return d->tableProperties.fields();}

	QString toString(const QString &sep = "\t") const;
};

}}}

#endif
