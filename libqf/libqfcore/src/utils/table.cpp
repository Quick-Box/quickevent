#include "table.h"
#include "csvreader.h"
#include "treetable.h"

#include "../core/log.h"
#include "../core/assert.h"
#include "../core/utils.h"
#include "../core/string.h"

#include <QStringBuilder>
#include <QDate>
#include <QDomElement>
#include <QTextStream>

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
static const auto SkipEmptyParts = QString::SkipEmptyParts;
#else
static const auto SkipEmptyParts = Qt::SkipEmptyParts;
#endif

using namespace qf::core::utils;

//====================================================
//                      Table::LessThan
//====================================================
Table::LessThan::LessThan(const Table &t)
	: table(t)
	, sortedFields(t.tableProperties().sortDefinition())
	, sortCollator(t.sortCollator())
{
	//qfLogFuncFrame() << "sortedFields.count():" << sortedFields.count();
	for(int i=0; i<sortedFields.count(); i++) {
		if(!table.fields().isValidFieldIndex(sortedFields[i].fieldIndex))
			qfWarning() << "Invalid sort definition. field index:" << sortedFields[i].fieldIndex;
	}
}

bool Table::LessThan::lessThan(int r1, int r2) const
{
	//qfLogFuncFrame() << "r1:" << r1 << "r2:" << r2;
	bool ret = false;
	for(int i=0; i<sortedFields.count(); i++) {
		const Table::SortDef &sd = sortedFields[i];
		QVariant l = table.rows()[r1].value(sd.fieldIndex);
		QVariant r = table.rows()[r2].value(sd.fieldIndex);
		int res = cmp(l, r, sd);
		//qfDebug() << "\t res:" << res;
		if(res == 0) continue;
		if(sd.ascending) {ret = (res < 0); break;}
		ret = (res > 0); break;
	}
	//qfDebug() << "\t return:" << ret;
	return ret;
}

bool Table::LessThan::lessThan_helper(int _row, const QVariant &v, bool switch_params) const
{
	//qfLogFuncFrame() << "row:" << _row << "v:" << v.toString() << "switch_params:" << switch_params;
	bool ret = false;
	QVariantList vlst = v.toList();
	int cnt = 1;
	if(!vlst.isEmpty()) cnt = vlst.count();
	for(int i=0; i<cnt && i<sortedFields.count(); i++) {
		const Table::SortDef &sd = sortedFields[i];
		QF_CHECK(sd.ascending, "Ascending sort is needed for seek.");
		QVariant l = table.rows()[_row].value(sd.fieldIndex);
		QVariant r;
		if(vlst.isEmpty()) r = v;
		else r = vlst[i];
		int res;
		//qfDebug() << "\t l:" << l.toString() << "r:" << r.toString();
		if(switch_params) res = cmp(r, l, sd);
		else res = cmp(l, r, sd);
		if(res < 0) {ret = true; break;}
		if(res > 0) {ret = false; break;}
	}
	//qfDebug() << "\treturn:" << ret;
	return ret;
}

int Table::LessThan::cmp(const QVariant &l, const QVariant &r, const Table::SortDef &sd) const
{
	//qfInfo() << QF_FUNC_NAME << "l:" << l << "r:" << r;
	/// NULL je nejmensi ze vsech
	if(!l.isValid() && r.isValid())
		return -1;
	if(l.isValid() && !r.isValid())
		return 1;
	if(!l.isValid() && !r.isValid())
		return 0;
	int ret = 1;
#if QT_VERSION_MAJOR >= 6
	if(l.typeId() == QMetaType::QString) {
#else
	if(l.type() == QVariant::String) {
#endif
		if(sd.ascii7bit) {
			QByteArray la = qf::core::Collator::toAscii7(QLocale::Czech, l.toString(), !sd.caseSensitive);
			QByteArray ra = qf::core::Collator::toAscii7(QLocale::Czech, r.toString(), !sd.caseSensitive);
			if(la < ra)
				ret = -1;
			else if(la == ra)
				ret = 0;
		}
		else {
			/// case sensitivity is property of table sort collator now
			/// and cannot be specified for particular columns
			ret = sortCollator.compare(l.toString(), r.toString());
			//qfWarning() << l.toString() << "vs" << r.toString() << ":" << ret;
		}
	}
	else {
		if(l == r) ret = 0;
#if QT_VERSION_MAJOR >= 6
		else switch (l.typeId()) {
#else
		else switch ((int)l.type()) {
#endif
			case QMetaType::UnknownType:
				ret = -1;
				break;
			case QMetaType::Bool:
			case QMetaType::Int:
				if(l.toInt() < r.toInt()) ret = -1;
				break;
			case QMetaType::UInt:
				if(l.toUInt() < r.toUInt()) ret = -1;
				break;
			case QMetaType::LongLong:
				if(l.toLongLong() < r.toLongLong()) ret = -1;
				break;
			case QMetaType::ULongLong:
				if(l.toULongLong() < r.toULongLong()) ret = -1;
				break;
			case QMetaType::Double:
				if(l.toDouble() < r.toDouble()) ret = -1;
				//qfInfo() << "l:" << l.toDouble() << "lestThan" << "r:" << r.toDouble() << ret;
				break;
			case QMetaType::Char:
				if(l.toChar() < r.toChar()) ret = -1;
				break;
			case QMetaType::QDate:
				if(l.toDate() < r.toDate()) ret = -1;
				break;
			case QMetaType::QTime:
				if(l.toTime() < r.toTime()) ret = -1;
				break;
			case QMetaType::QDateTime:
				if(l.toDateTime() < r.toDateTime()) ret = -1;
				break;
			case QMetaType::QByteArray:
				if(l.toByteArray() < r.toByteArray()) ret = -1;
				break;
			default:
#if QT_VERSION_MAJOR >= 6
				qfWarning() << "unsupported variant comparison, variant type:" << l.typeId();
#else
				qfWarning() << "unsupported variant comparison, variant type:" << l.type();
#endif
				break;
		}
	}
	//qfInfo() << "\tret:" << ret;
	return ret;
}

//=========================================
//           Table::Field
//=========================================
const Table::Field &Table::Field::sharedNull()
{
	static Field n = Field(SharedDummyHelper());
	return n;
}

Table::Field::Field(Table::Field::SharedDummyHelper)
{
	d = new Data();
}

Table::Field::Field()
{
	*this = sharedNull();
}

Table::Field::Field(const QString &name, int user_type)
{
	d = new Data(name, user_type);
}

QString Table::Field::shortName() const
{
	QString ret = name();
	qf::core::Utils::parseFieldName(ret, &ret);
	return ret;
}

QString Table::Field::tableId() const
{
	QString ret = name();
	QString ds;
	qf::core::Utils::parseFieldName(ret, nullptr, &ret, &ds);
	if(!ds.isEmpty())
		ret = ds + '.' + ret;
	return ret;
}

QString Table::Field::toString() const
{
	QString ret = "name: '%1', type: %2, canUpdate: %3, isPriKey: %4";
#if QT_VERSION_MAJOR >= 6
	return ret.arg(name()).arg(type().name()).arg(canUpdate()).arg(isPriKey());
#else
	return ret.arg(name()).arg(type().name().data()).arg(canUpdate()).arg(isPriKey());
#endif
}

/*
Table::Field::Field(const QString & _name, const QString & _def)
{
	d = new Data(QVariant::Invalid, _name);
	QString fs = _def;
	String::StringMap map = fs.splitMap();
	QMapIterator<QString, QString> i(map);
	while (i.hasNext()) {
		i.next();
		QString s = i.key().toLower();
		if(s == "type") {
			QString s1 = i.value().toLower();
			if(!s1.isEmpty()) {
				if(s1 == "string") type = QVariant::String;
				else if(s1 == "date") type = QVariant::Date;
				else if(s1 == "datetime") type = QVariant::DateTime;
				else if(s1 == "time") type = QVariant::Time;
				else type = QVariant::nameToType(qPrintable(s1));
				//qfInfo() << "s1:" << s1 << qPrintable(s1) << "type:" << type;
				//qfInfo() << "name:" << name << "type:" << QVariant::typeToName(type);
			}
		}
		else if(s.startsWith("len")) length = i.value().toInt();
		else if(s.startsWith("prec")) prec = i.value().toInt();
	}
}
*/
//=========================================
//           Table::FieldList
//=========================================
int Table::FieldList::fieldIndex(const QString &field_name) const
{
	int col = -1, i = 0;
	QString s = field_name;
	foreach(Field fld, *this) {
		if(Utils::fieldNameEndsWith(fld.name(), s)) {
			col = i;
			break;
		}
		i++;
	}
	/*
	if(col < 0) {
		QString s1;
		foreach(Field fld, *this)
			s1 += ", " + fld.name();
		s1 = s1.mid(2);
		if(throw_exc) {
			qfInfo() << Log::stackTrace();
			qfFatal("Field '%s' not found in query\nAvailable fields are: %s", qPrintable(field_name), qPrintable(s1));
		}
		return -1;
	}
	*/
	return col;
}

bool Table::FieldList::isValidFieldIndex(int fld_ix) const
{
	bool ret = (fld_ix >= 0 && fld_ix < count());
	return ret;
}

//=========================================
//       Table::TableProperties
//=========================================
Table::TableProperties::TableProperties(SharedDummyHelper)
{
	d = new Data();
}

Table::TableProperties::TableProperties()
{
	*this = sharedNull();
}

const Table::TableProperties& Table::TableProperties::sharedNull()
{
	static TableProperties n = TableProperties(SharedDummyHelper());
	return n;
}

//=========================================
//         Table::Row
//=========================================
TableRow::Data::Data()
{
	flags.insert = false;
}

TableRow::Data::Data(const Table::TableProperties &props)
	: tableProperties(props)
{
	flags.insert = false;
	//flags.forcedInsert = false;
	Table::FieldList flst = tableProperties.fields();
	values.resize(flst.count());
	for(int i=0; i<flst.count(); i++)
		values[i] = QVariant();/// NULL je QVariant(), to kvuli QDate. (flst[i].type());
}

TableRow::Data::~Data()
= default;

TableRow::TableRow()
{
	*this = sharedNull();
}

TableRow::TableRow(SharedDummyHelper)
{
	d = new Data();
}
/*
void TableRow::initValues()
{
	d->origValues.clear();
	const Table::FieldList &flst = tableProperties.fields();
	d->values.resize(flst.count());
	for(int i=0; i<flst.count(); i++)
		d->values[i] = QVariant();/// NULL je QVariant(), to kvuli QDate. (flst[i].type());
}
*/
TableRow::TableRow(const Table::TableProperties &props)
{
	d = new Data(props);
}

const TableRow& TableRow::sharedNull()
{
	static TableRow n = TableRow(SharedDummyHelper());
	return n;
}

void TableRow::insertInitValue(int ix)
{
	d->origValues.clear();
	d->dirtyFlags.clear();
	d->values.insert(ix, QVariant());
}

bool TableRow::isDirty(int field_no) const
{
	QF_ASSERT(field_no >= 0 && field_no < d->values.size(),
			  QString("field index %1 is out of range (%2)").arg(field_no).arg(d->values.size()),
			  return false);
	bool ret = false;
	if(field_no < d->dirtyFlags.count()) {
		ret = d->dirtyFlags.at(field_no);
	}
	//qfInfo() << field_no << "->" << ret;
	return ret;
}

void TableRow::setDirty(int field_no, bool val)
{
	qfLogFuncFrame() << "field_no:" << field_no << "val:" << val;
	QF_ASSERT(field_no >= 0 && field_no < d->values.size(),
			  QString("field index %1 is out of range (%2)").arg(field_no).arg(d->values.size()),
			  return);
	if(d->dirtyFlags.count() < d->values.count()) {
		d->dirtyFlags.clear();
		d->dirtyFlags.resize(d->values.size());
	}
	//qfInfo() << val << "->" << field_no;
	d->dirtyFlags[field_no] = val;
}

bool TableRow::isDirty() const
{
	for(int i=0; i<d->dirtyFlags.count(); i++) {
		if(d->dirtyFlags[i])
			return true;
	}
	return false;
}

QVariant TableRow::origValue(int col) const
{
	QVariant ret;
	QF_ASSERT(col >= 0 && col < d->origValues.size(),
			  QString("Column %1 is out of origValues range %2").arg(col).arg(d->origValues.size()),
			  return ret);
	ret = d->origValues.value(col);
	return ret;
}

QVariant TableRow::origValue(const QString & field_name) const
{
	int ix = fields().fieldIndex(field_name);
	QVariant ret;
	QF_ASSERT(ix >= 0, QString("Field name '%1' not found.").arg(field_name), return ret);
	ret = origValue(ix);
	return ret;
}

QVariant TableRow::value(int col) const
{
	QVariant ret;
	QF_ASSERT(col >= 0 && col < d->values.size(),
			  QString("Column %1 is out of range %2").arg(col).arg(d->values.size()),
			  return ret);
	ret = d->values.value(col);
	// qfInfo().nospace() << "col: " << col << " value: '" << ret.toString() << "' type: " << ret.typeName() << " is null: " << ret.isNull() << " is valid: " << ret.isValid();
	return ret;
}

QVariant TableRow::value(const QString &field_name) const
{
	QVariant ret;
	int ix = fields().fieldIndex(field_name);
	if (ix < 0) {
		qfError() << QString("Field name '%1' not found.").arg(field_name);
		return ret;
	}
	ret = value(ix);
	return ret;
}

QVariantMap TableRow::valuesMap(bool full_names) const
{
	QVariantMap ret;
	for (int i=0; i<fields().count(); ++i) {
		const Table::Field &fld = fields()[i];
		QString key = fld.name().toLower(); // PSQL returns all fieldnames in lowercase
		if(!full_names)
			qf::core::Utils::parseFieldName(key, &key);
		ret[key] = value(i);
	}
	return ret;
}

QVariantMap TableRow::dirtyValuesMap() const
{
	QVariantMap ret;
	for (int i=0; i<fields().count(); ++i) {
		const Table::Field &fld = fields()[i];
		if (isDirty(i)) {
			QString key = fld.name().toLower(); // PSQL returns all fieldnames in lowercase
			key.replace("__", ".");
			ret[key] = value(i);
		}
	}
	return ret;
}

void TableRow::setBareBoneValue(int col, const QVariant & val)
{
	d->values[col] = val;
}

void TableRow::setValue(int col, const QVariant &v)
{
	qfLogFuncFrame() << "col:" << col << "val:" << v.toString();
	if(d->values.count() > d->origValues.count()) {
		saveValues();
	}
	QF_ASSERT(col >= 0 && col < d->values.size(),
		QString("Column %1 is out of range %2").arg(col).arg(d->values.size()),
		return);

	QVariant new_val;
	if(v.isValid())
		new_val = Utils::retypeVariant(v, fields()[col].type().id());
	QVariant orig_val = origValue(col);
	//qfInfo() << new_val << "is null:" << new_val.isNull() << "==" << orig_val << "is null:" << orig_val.isNull() << "->" << (new_val == orig_val);
	bool same_nullity = (new_val.isNull() && orig_val.isNull()) || (!new_val.isNull() && !orig_val.isNull());
	bool same_values = same_nullity && (new_val == orig_val);
	if(same_values) {
		/// pokud hodnota byla nastavena (napr. po opakovanem prenastaveni) nakonec na originalni, neni nutne ji ukladat
		d->values[col] = orig_val;
		setDirty(col, false);
	}
	else {
		d->values[col] = new_val;
		setDirty(col, true);
	}
}

void TableRow::setValue(const QString &field_name, const QVariant &v)
{
	qfLogFuncFrame() << "field_name" << field_name << "val:" << v.toString();
	int col = fields().fieldIndex(field_name);
	setValue(col, v);
}

void TableRow::restoreOrigValue(int col)
{
	qfLogFuncFrame() << "col:" << col;
	QVariant orig_val = d->origValues.value(col);
	/// pokud hodnota byla nastavena (napr. po opakovanem prenastaveni) nakonec na originalni, neni nutne ji ukladat
	d->values[col] = orig_val;
	if(col < d->dirtyFlags.count()) {
		d->dirtyFlags[col] = false;
	}
}

void TableRow::saveValues()
{
	//qfInfo() << "save values:" << d->values.size();
	//if(isDirty()) return;
	//qfLogFuncFrame() << "fieldcnt:" << fields().size();
	//origValues.clear();
	d->origValues.resize(d->values.size());
	d->dirtyFlags.resize(d->values.size());
	for(int i=0; i<d->values.size(); i++) {
		//qfInfo() << i << d->values[i] << "->" << d->origValues[i];
		d->origValues[i] = d->values[i];
		d->dirtyFlags[i] = false;
	}
}

void TableRow::restoreOrigValues()
{
	for(int i=0; i<d->values.count(); i++) {
		d->values[i] = origValue(i);
	}
	clearOrigValues();
}

void TableRow::clearOrigValues()
{
	d->origValues.clear();
	d->dirtyFlags.clear();
}
void TableRow::clearEditFlags()
{
	clearOrigValues();
	setInsert(false);
	//setForcedInsert(false);
}

void TableRow::prepareForCopy()
{
	/// setup copied row to be inserted on post call
	d->origValues.clear();
	d->origValues.resize(d->values.size());
	setInsert(true);
	//qfInfo() << "is insert;" << isInsert();
	for(int i=0; i<fields().count(); i++) {
		Table::Field fld = fields().value(i);
		if(fld.isPriKey() && fld.isSerial()) {
			setValue(i, QVariant());
			setDirty(i, false);
		}
		else {
			setDirty(i, true);
		}
	}
}

QString TableRow::toString(const QString &sep) const
{
	QStringList sl;
	for(int i=0; i<fields().count(); i++) {
		QString s = "[%1:%2]: %3(%4)";
		s = s.arg(fields()[i].name());
		QVariant v = value(i);
		QVariant ov = origValue(i);
		sl << s.arg(v.typeName()).arg(v.toString()).arg(ov.toString());
	}
	return sl.join(sep);
}

//=========================================
// Table
//=========================================
Table::Data::Data()
	: sortCollator(QLocale::Czech)
{
	sortCollator.setIgnorePunctuation(true);
}

Table::Table(Table::SharedDummyHelper )
{
	d = new Data();
}

const Table & Table::sharedNull()
{
	static Table n = Table(SharedDummyHelper());
	return n;
}

Table::Table()
{
	*this = sharedNull();
}

Table::Table(const QStringList &col_names)
{
	d = new Data();
	for(int i=0; i<col_names.count(); i++) {
		Field fld(col_names[i], QMetaType::QString);
		fld.setCanUpdate(true);
		fieldsRef().append(fld);
	}
}

Table::Table(const Table::FieldList &col_defs)
{
	d = new Data();
	for(int i=0; i<col_defs.count(); i++) {
		Field fld(col_defs[i]);
		//fld.setLength(cd.length);
		//fld.setPrecision(cd.prec);
		fieldsRef().append(fld);
	}
}

Table::~Table()
= default;

void Table::cleanupData(CleanupDataOption fields_options)
{
	qfLogFuncFrame();
	if(fields_options == ClearFieldsRows) {
		qfDebug() << "\tcleaning fields";
		fieldsRef().clear();
	}
	qfDebug() << "\tcleaning rows";
	d->rows.clear();
	createRowIndex();
}

int Table::rowCount() const
{
	return rowIndex().size();
}

int Table::columnCount() const
{
	return fields().count();
}

Table::Field &Table::insertColumn(int ix, const QString &name, QMetaType::Type t)
{
	FieldList &fields = fieldsRef();
	fields.insert(ix, Field(name, t));
	for (int i = 0; i < rowCount(); ++i) {
		TableRow &r = rowRef(i);
		r.setTableProperties(tableProperties());
		r.insertInitValue(ix);
	}
	return fields[ix];
}

bool Table::isValidRowIndex(int row) const //throw(QFException)
{
	bool ret = (row >= 0 && row < rowCount());
	return ret;
}

Table::Field& Table::fieldRef(int fld_ix)
{
	if(!isValidFieldIndex(fld_ix)) {
		qfError() << QString("col %1 is not valid column number (count: %2)").arg(fld_ix).arg(fields().count());
		qfFatal("Giving up.");
	}
	return fieldsRef()[fld_ix];
}

Table::Field &Table::fieldRef(const QString &field_name)
{
	int ix = fields().fieldIndex(field_name);
	QF_ASSERT_EX(ix >= 0, QString("Invalid field name '%1'").arg(field_name));
	return fieldRef(ix);
}

Table::Field Table::field(int fld_ix) const
{
	Table::Field ret;
	QF_ASSERT(isValidFieldIndex(fld_ix),
			  QString("Invalid field index: %1").arg(fld_ix),
			  return ret);
	ret = fields()[fld_ix];
	return ret;
}

TableRow& Table::insertRow(int before_row)
{
	//qfLogFuncFrame();// << Log::stackTrace();
	TableRow &row = insertRow(before_row, isolatedRow());
	return row;
}

TableRow& Table::insertRow(int before_row, const TableRow &_row)
{
	//qfLogFuncFrame() << "before_row:" << before_row << "row cnt:" << rowCount();
	if(columnCount() <= 0) {
		qfFatal("Table has no columns, row can not be inserted.");
	}

	if(!(tableProperties() == _row.tableProperties()))
		qfFatal("Inserted row comes from different table.");

	int r = (isValidRowIndex(before_row))? before_row: rowCount();
	TableRow empty_row(_row);
	empty_row.setInsert(true);
	rowsRef().append(empty_row);
	rowIndexRef().insert(r, rowsRef().count()-1);
	TableRow &row = rowRef(r);
	//if(fill_default_and_auto_values) row.fillDefaultAndAutogeneratedValues();
	return row;
}


TableRow Table::isolatedRow() const
{
	//qfLogFuncFrame();// << Log::stackTrace();
	TableRow empty_row(tableProperties());
	return empty_row;
}

bool Table::removeRow(int ri)
{
	//qfLogFuncFrame();
	if(!isValidRowIndex(ri)) {
		qfWarning() << "Attempt to remove invalid row:" << ri;
		return false;
	}
	int ix = rowNumberToRowIndex(ri);
	rowsRef().removeAt(ix);
	rowIndexRef().remove(ri);
    /// posun zbyvajici indexy
	RowIndexList &rlst = rowIndexRef();
	for(int i=0; i<rlst.count(); i++)
		if(rlst[i]>ix)
			rlst[i]--;
	return true;
}

void Table::revertRow(int ri)
{
	if(isValidRowIndex(ri)) {
		TableRow &r = rowRef(ri);
		revertRow(r);
	}
}

void Table::revertRow(TableRow &r)
{
	r.restoreOrigValues();
}

void Table::createRowIndex()
{
	d->rowIndex.clear();
	d->rowIndex.resize(d->rows.count());
	for(int i=0; i<d->rows.count(); i++)
		d->rowIndex[i] = i;
}

int Table::rowNumberToRowIndex(int rowno) const
{
	int ix = rowIndex().value(rowno, -1);
	QF_ASSERT(ix >= 0 && ix < rows().count(),
			  QString("Row index corrupted, wanted row: %1, rows count: %2").arg(ix).arg(d->rows.count()),
			  return -1);
	return ix;
}

TableRow& Table::rowRef(int ri)
{
	//qfLogFuncFrame();
	QF_ASSERT_EX(isValidRowIndex(ri),
			  QString("row: %1 is out of range of rows (%2)").arg(ri).arg(rowCount()));
	ri = rowNumberToRowIndex(ri);
	return d->rows[ri];
}

TableRow Table::row(int ri) const
{
	TableRow ret;
	if(!isValidRowIndex(ri)) {
			auto msg = QString("row: %1 is out of range of row count (%2)").arg(ri).arg(d->rows.size());
			qfError() << msg;
			return ret;
	}
	ret = rows().value(rowNumberToRowIndex(ri));
	return ret;
}

TableRow Table::lastRow() const
{
	TableRow ret;
	QF_ASSERT(rowCount() > 0,
			  "Table is empty",
			  return ret);
	ret = row(rowCount() - 1);
	return ret;
}

int TableRow::fieldCount() const
{
	return fields().count();
}

int TableRow::count() const
{
	return d->values.count();
}

bool TableRow::isInsert() const
{
	return d->flags.insert;
}

void TableRow::setInsert(bool b)
{
	d->flags.insert = b;
}

/*
static QString quote_XML(const QString &s, const Table::TextExportOptions &opts)
{
	Q_UNUSED(opts);
	QString ret = s;
	ret.replace('<', "&lt;");
	ret.replace('>', "&gt;");
	return ret;
}
*/
QString Table::quoteCSV(const QVariant &val, const Table::TextExportOptions &opts)
{
	QString ret;
#if QT_VERSION_MAJOR >= 6
	if(val.typeId() == QMetaType::Bool)
#else
	if(val.type() == QVariant::Bool)
#endif
		ret = val.toBool()? QStringLiteral("true"): QStringLiteral("false");
	else
		ret = val.toString();
	bool quote = false;
	if(opts.fieldQuotingPolicy() == Table::TextExportOptions::IfNecessary) {
		if(ret.indexOf('"') >= 0) {
			ret = ret.replace('"', "\"\"");
			quote = true;
		}
		else if(!quote && ret.indexOf('\n') >= 0) {
			quote = true;
		}
		else if(!quote && ret.indexOf(opts.fieldSeparator()) >= 0) {
			quote = true;
		}
		else if(!quote && ret.indexOf('#') >= 0) {
			quote = true; /// extended CVS, # je komentar
		}
	}
	else if(opts.fieldQuotingPolicy() == Table::TextExportOptions::Always) {
		quote = true;
	}
	if(quote)
		ret = "\"" + ret + "\"";
	return ret;
}

void Table::exportCSV(QTextStream &ts, const QString col_names, Table::TextExportOptions opts) const
{
	QList<int> ixs;
	if(col_names == "*") {
		for(int i=0; i<fields().count(); i++) ixs.append(i);
	}
	else {
		QStringList sl = col_names.split(',', SkipEmptyParts);
		foreach(QString s, sl) {
			s = s.trimmed();
			int ix = fields().fieldIndex(s);
			if(ix >= 0)
				ixs.append(ix);
			else {
				qfError() << "Field not found:" << s;
				return;
			}
		}
	}
	if(opts.isExportColumnNames()) {
		/// export field names
		QVariantMap column_captions;
		if(opts.isUseColumnCaptions())
			column_captions = opts.columnCaptions();
		QStringList caption_field_names = column_captions.keys();
		for(int i=0; i<ixs.count(); i++) {
			if(i > 0) ts << opts.fieldSeparator();
			Field fld = field(ixs[i]);
			QString fld_name = fld.name();
			QString cap = fld_name;
			if(!opts.isFullColumnNames())
				Utils::parseFieldName(fld_name, &cap);
			else {
				foreach(QString key, caption_field_names) {
					if(Utils::fieldNameCmp(key, fld_name)) {
						cap = column_captions.value(key).toString();
						break;
					}
				}
			}
			ts << cap;
		}
		ts << '\n';
	}
	// export data
	int n1 = opts.fromLine();
	int n2 = opts.toLine();
	if(n2 < 0) n2 = INT_MAX;
	for(int i=n1; i<rowCount() && i<=n2; i++) {
		TableRow r = row(i);
		for(int i=0; i<ixs.count(); i++) {
			if(i > 0)
				ts << opts.fieldSeparator();
			ts << quoteCSV(r.value(ixs[i]), opts);
		}
		ts << '\n';
	}
}

const QString Table::CVSTableEndTag = "#CVS_TABLE_END";

void Table::importCSV(QTextStream &ts, TextImportOptions opts)
{
	qfLogFuncFrame();
	QString s;
	QStringList sl;
	//emit progressValue(0, tr("Importuji CSV"));
	qfDebug() << "\t opts fieldSeparator:" << opts.fieldSeparator();
	qfDebug() << "\t opts ignoreFirstLinesCount:" << opts.ignoreFirstLinesCount();
	qfDebug() << "\t opts isImportColumnNames:" << opts.isImportColumnNames();
	CSVReader reader(&ts, String(opts.fieldSeparator()).value(0).toLatin1(), String(opts.fieldQuotes()).value(0).toLatin1());
	if(!opts.isImportAppend()) clear();
	for(int i=0; i<opts.ignoreFirstLinesCount(); i++)
		reader.readCSVLine();
	if(opts.isImportColumnNames()) {
		sl = reader.readCSVLineSplitted();
		foreach(s, sl) {
			//qfDebug() << "\tfield:" << s;
			Field fld(s, QMetaType::QString);
			fieldsRef().append(fld);
		}
	}
	//int n = 0, cnt = 100, steps = 100, progress_step = cnt / steps;
	bool trim_values = opts.isTrimValues();
	while(!ts.atEnd()) {
		//if(((n++)%steps) % progress_step) emit progressValue(1.*n / cnt, tr("Probiha import"));
		sl = reader.readCSVLineSplitted();
		if(!sl.isEmpty()) {
			if(sl[0] == CVSTableEndTag) break;
			s = sl[0];
			if(s[0] == '#') continue; /// CVS comment (my CVS extension)
			if(fields().isEmpty()) {
				/// pokud nejsou vytvoreny fieldy, treba protoze se neimpoortovaly nazvy sloupcu, udelej je z tohoto radku
				int colno = 0;
				foreach(s, sl) {
					Field fld(QString("col%1").arg(++colno), QMetaType::QString);
					fieldsRef().append(fld);
				}
			}
			TableRow r(tableProperties());
			for(int i=0; i<fields().count() && i<sl.count(); i++) {
				QString s = sl[i];
				if(trim_values) s = s.trimmed();
				r.setValue(i, s);
			}
			r.clearOrigValues();
			d->rows.append(r);
		}
	}
	//reload();
}
#ifdef TXT_EXPORT_IMPORT
static QList<int> parse_field_structure(Table &table, const QString & file_structure_definition, QVariantMap & parsed_props)
{
	qfLogFuncFrame();
	QList<int> col_mapping;
	if(!file_structure_definition.isEmpty()) {
		table.fieldsRef().clear();
		qfc::String fs = file_structure_definition;
		QStringList lines = fs.splitAndTrim('\n');
		QString content;
		foreach(fs, lines) {
			int ix = fs.indexOf('#');
			if(ix >= 0)
				fs = fs.slice(0, ix).trimmed();
			if(fs.isEmpty()) continue;
			content += fs;
		};
		QStringList sl = content.splitVector();
		//Table::ColumnDefList cd_list;
		foreach(fs, sl) {
			QString key;
			String val;
			int ix = fs.indexOf(':');
			if(ix >= 0) {
				key = fs.slice(0, ix).trim();
				val = fs.slice(ix+1).trim();
				if(val[0] == '\'') val = val.slice(1, -1);
			}
			else key = fs.trim();
			//qfInfo() << "key:" << key << "val:" << val;
			if(val[0] == '{') parsed_props[key] = val.splitMapRecursively();
			//else if(val[0] == '[') values[key] = split_array(val);
			else parsed_props[key] = val;

			if(key == "columns") {
				QStringList sl2 = val.splitVector();
				int mapped_col_no = 0;
				foreach(fs, sl2) {
					ix = fs.indexOf(':');
					if(ix >= 0) {key = fs.slice(0, ix).trim(); val = fs.slice(ix+1).trim();}
					else {key = fs.trim(); val = QString();}
					if(key == "skip-columns") {
						mapped_col_no += val.toInt();
						continue;
					}
					{
						Table::ColumnDef cd(key, val);
						Field fld(cd.name, cd.type);
						fld.setLength(cd.length);
						fld.setPrecision(cd.prec);
						table.fieldsRef().append(fld);
						qfDebug() << "\t adding field:" << fld.toString();
						qfDebug() << "\t mapped to column:" << mapped_col_no;
					}
					col_mapping << mapped_col_no++;
				}
			}
		}
	}
	return col_mapping;
}

static QChar field_delimiter_helper(const QString &_delimiter)
{
	QChar field_delimiter;
	if(_delimiter == "\\n") field_delimiter = '\n';
	else if(_delimiter == "\\t") field_delimiter = '\t';
	else if(_delimiter.count() > 0) field_delimiter = _delimiter[0];
	return field_delimiter;
}


void Table::importTXT(QTextStream &ts, const QString &file_structure_definition)
{
	qfLogFuncFrame();
	QVariantMap parsed_props;
	QList<int> col_mapping = parse_field_structure(*this, file_structure_definition, parsed_props);
	//QList<int> delimiter_positions;
	//qfDebug() << "\t parsed_props:" << QFJson::variantToString(parsed_props);

	QString codec_name = parsed_props.value("text-codec", "utf8").toString();
	QTextCodec *codec = QTextCodec::codecForName(codec_name.toLatin1());
	if(!codec) QF_EXCEPTION(QString("Cannot find text codec for name '%1'.").arg(codec_name));
	ts.setCodec(codec);

	QChar field_delimiter = field_delimiter_helper(parsed_props.value("field-delimiter", ";").toString());
	QChar field_quote = String(parsed_props.value("field-quote", QString()).toString()).value(0);
	qfDebug() << "\t field_delimiter:" << field_delimiter;
	qfDebug() << "\t field_quote:" << field_quote;
	bool preserve_empty_lines = parsed_props.value("preserve-empty-lines", 0).toBool();
	QStringList lines;
	if(field_delimiter == '\n') {
		QString s = ts.readAll();
		lines << s.trimmed();
	}
	else {
		while(!ts.atEnd()) {
			String line;
			int qcnt = 0;
			do {
				QString s = ts.readLine();
				qfDebug().noSpace() << "\t read line: ###" << s << "###";
				qcnt += s.count(field_quote);
				line += s;
				if(qcnt % 2) line += "\n";
			} while((qcnt % 2) && !ts.atEnd());
			qfDebug().noSpace() << "\t whole line: ###" << line << "###";
			if(line.isEmpty() && !preserve_empty_lines) continue;
			//qfInfo().noSpace() << "\t encoded line: ###" << s << "###";
			lines << line;
		}
		//qfInfo().noSpace() << "\t <EOF>";
	}
	int skip_lines = parsed_props.value("skip-lines", 0).toInt();
	QVariantMap original_column_definition = parsed_props.value("columns").toMap();
	for(int line_no=skip_lines; line_no<lines.count(); line_no++) {
		String line = lines[line_no];
		qfDebug().noSpace() << "\t parsing line: ###" << line << "###" << line_no << "/" << lines.count();
		QStringList field_list = line.splitAndTrim(field_delimiter, field_quote, String::TrimParts, QString::KeepEmptyParts);
		Row r = singleRow();
		for(int col_no=0; col_no<fields().count(); col_no++) {
			int imported_col_no = col_mapping[col_no];
			if(imported_col_no >= field_list.count()) {
				qfWarning() << QString("invalid field mapping %1->%2 for field '%3'").arg(col_no).arg(imported_col_no).arg(fields()[col_no].fieldName());
				qfInfo() << "line:" << line_no << "fields count:" << field_list.count();
				for(int i=0; i<field_list.count(); i++) qfInfo().noSpace() << i << ": '" << field_list[i] << "'";
				continue;
			}
			String fs = field_list[imported_col_no];
			{
				QString fld_name = field(col_no).fieldName();
				QVariantMap def_map = original_column_definition.value(fld_name).toMap();
				int start_index = def_map.value("start-index", 0).toInt();
				int end_index = def_map.value("end-index", INT_MAX).toInt();
				//qfInfo().noSpace() << "\tsource string: '" << fs << "'";
				//qfInfo() << "\t start index: " << start_index;
				//qfInfo() << "\t end index: " << end_index;
				fs = fs.slice(start_index, end_index).trim();
				//qfInfo().noSpace() << "\t [" << fld_name << "] = '" << fs << "'";
			}
			QVariant v;
			//if(!fs.isEmpty()) v = fs; /// prazdne hodnoty jsou NULL pro klaestables (je to moznast, lae zatim to asi nevyuzivam)
			/// pokud by se ukazala potreba NULL hodnoty v klaes tabulce, pridam nullable flag do definice sloupce
			v = fs;
			r.setValue(col_no, v);
		}
		//qfInfo() << "\t appending row:" << r.toString(", ");
		//r.post();
		r.clearOrigValues();
		appendRow(r);
		//rowsRef().append(r);
		//qfDebug() << "\t rowsRef().count():" << rowsRef().count();
	}
	//Table::reload();
	//qfInfo() << "\n" << dump();
	qfDebug() << "\t row count:" << rowCount();
}
#endif
QString Table::toString() const
{
	char sep = '\t';
	QString s;
	QTextStream ts(&s);
	//ts.setPadChar('%');
	ts << "       ";
	for(int i=0; i<fields().count(); i++) {
		if(i > 0) ts << sep;
		ts << field(i).name();
	}
	ts << '\n';
	// export data
	int no = 0;
	QString s1 = "%1.";
	for(int i=0; i<rowCount(); i++) {
		TableRow r = row(i);
		ts << s1.arg(++no, 5) << sep;
		for(int i=0; i<fields().count(); i++) {
			if(i > 0)
				ts << sep;
			ts << r.value(i).toString();
		}
		ts << '\n';
	}
	return s;
}

qf::core::Collator Table::sortCollator() const
{
    return d->sortCollator;
}

void Table::setSortCollator(const Collator &coll)
{
    d->sortCollator = coll;
}

void Table::sort(const QString &_colnames)
{
	SortDefList sd;
	String colnames = _colnames;
	QStringList sl = colnames.splitAndTrim(',');
	foreach(String s, sl) {
		int ix = s.pos(' ');
		bool asc = true;
		bool cs = true;
		bool ascii7bit = true;
		QString colname = s;
		if(ix >= 0) {
			colname = s.slice(0, ix).trimmed();
			s = s.slice(ix+1).trimmed().toUpper();
			asc = s.indexOf("DESC") >= 0;
			cs = !(s.indexOf("ICS") >= 0);
		}
		ix = fields().fieldIndex(s);
		sd.append(SortDef(ix, asc, cs, ascii7bit));
	}
	sort(sd);
}

void Table::sort(const SortDef &sorted_field)
{
	SortDefList sdl;
	if(sorted_field.isValid()) sdl << sorted_field;
	sort(sdl);
}

void Table::sort(const Table::SortDefList &sorted_fields)
{
	//qfLogFuncFrame() << QLocale().name();
	sort(sorted_fields, 0, rowCount());
}

void Table::sort(const SortDefList & sorted_fields, int start_row_index, int row_count)
{
	tablePropertiesRef().setSortDefinition(sorted_fields);
	qfLogFuncFrame() << "locale:" << QLocale().name();
	if(sorted_fields.isEmpty()) {
		//filter();
		createRowIndex();
	}
	else {
		for(int i=0; i<tableProperties().sortDefinition().count(); i++) {
			//qfDebug() << "\t field index:" << tableProperties().sortDefinition()[i].fieldIndex;
			//qfDebug() << "\t ascending:" << tableProperties().sortDefinition()[i].ascending;
		}
		RowIndexList::iterator begin = rowIndexRef().begin() + start_row_index;
		RowIndexList::iterator  end = begin + row_count;
		sort(begin, end);
		//stable_sort(rowIndexRef().begin(), rowIndexRef().end(), TableLessThan(*this));
	}
}
/*
void Table::resort()
{
	sort(tableProperties().sortDefinition());
}
*/
int Table::seek(const QVariant &v) const
{
	const Table::SortDefList &sdl = tableProperties().sortDefinition();
	QF_ASSERT(sdl.count() > 0,
			  "Empty sort definition.",
			  return -1);
	const Table::SortDef &sd = sdl[0];
	int seek_ix = sd.fieldIndex;
	QF_ASSERT(fields().isValidFieldIndex(seek_ix),
			  QString("Invalid sort field index: %1").arg(seek_ix),
			  return -1);
	QF_ASSERT(sd.ascending,
			  "Ascending sort is needed for seek.",
			  return -1);
	RowIndexList::const_iterator res = binaryFind(rowIndex().begin(), rowIndex().end(), v);
	if(res == rowIndex().end())
		return -1;
	return res - rowIndex().begin();
}

int Table::find(int field_ix, const QVariant &val) const
{
	int ret = -1;
	RowList row_lst = rows();
	for(int ix=0; ix < row_lst.count(); ix++) {
		const TableRow &r = row_lst.value(ix);
		if(r.value(field_ix) == val) {
			ret = ix;
		}
	}
	return ret;
}

int Table::find(const QString &field_name, const QVariant &val) const
{
	int field_ix = fields().fieldIndex(field_name);
	QF_ASSERT(field_ix >= 0,
			  QString("Invalid field name: '%1'").arg(field_name),
			  return -1);
	return find(field_ix, val);
}

QVariant Table::sumValue(int field_ix) const
{
	auto type = field(field_ix).type().id();
	QVariant ret;
	switch(type) {
		case QMetaType::Int:
		case QMetaType::UInt:
		{
			int d = 0;
			Q_FOREACH(auto r, rows())
				d += r.value(field_ix).toInt();
			ret = d;
			break;
		}
		case QMetaType::Double:
		default:
		{
			double d = 0;
			Q_FOREACH(auto r, rows())
				d += r.value(field_ix).toDouble();
			ret = d;
			break;
		}
	}
	return ret;
}

namespace {
void setDomElementText(QDomDocument &owner_doc, QDomElement &el, const QString &str)
{
	QDomNode nd = el.firstChild();
	QDomText el_txt = nd.toText();
	if(el_txt.isNull()) {
		el_txt = owner_doc.createTextNode(str);
		el.insertBefore(el_txt, nd);
	}
	else {
		el_txt.setData(str);
	}
}
}

QDomElement Table::toHtmlElement(QDomDocument &owner_doc, const QString & col_names, TextExportOptions opts) const
{
	QList<int> ixs;
	//emit progressValue(0, tr("Exportuji XML"));
	if(col_names.isEmpty() || col_names == "*") {
		for(int i=0; i<fields().count(); i++) ixs.append(i);
	}
	else {
		QStringList sl = col_names.split(',', SkipEmptyParts);
		foreach(QString s, sl) {
			int ix = fields().fieldIndex(s);
			if(ix >= 0)
				ixs.append(ix);
			else
				qfError() << "Field not found:" << s;
		}
	}
	QDomElement el_table = owner_doc.createElement("table");
	el_table.setAttribute("border", 1);
	if(opts.isExportColumnNames()) {
		/// export columns
		QVariantMap column_captions;
		if(opts.isUseColumnCaptions()) column_captions = opts.columnCaptions();
		QStringList caption_field_names = column_captions.keys();
		QDomElement el_row = owner_doc.createElement("tr");
		for(int i=0; i<ixs.count(); i++) {
			QDomElement el_th = owner_doc.createElement("th");
			Field fld = field(ixs[i]);
			QString fld_name = fld.name();
			QString cap;
			Utils::parseFieldName(fld_name, &cap);
			foreach(QString key, caption_field_names) {
				if(Utils::fieldNameCmp(key, fld_name)) {
					cap = column_captions.value(key).toString();
					break;
				}
			}
			setDomElementText(owner_doc, el_th, cap);
			el_row.appendChild(el_th);
		}
		el_table.appendChild(el_row);
	}
	/// export data
	//int n = 0, cnt = rowCount(), steps = 100, progress_step = cnt / steps + 1;
	for(int i=0; i<rowCount(); i++) {
		TableRow r = row(i);
		QDomElement el_row = owner_doc.createElement("tr");
		for(int i=0; i<ixs.count(); i++) {
			QDomElement el_td = owner_doc.createElement("td");
			setDomElementText(owner_doc, el_td, r.value(ixs[i]).toString());
			el_row.appendChild(el_td);
		}
		el_table.appendChild(el_row);
	}
	return el_table;
}

QVariantMap Table::toJson(const QString &col_names) const
{
	QList<int> ixs;
	if(col_names.isEmpty() || col_names == "*") {
		for(int i=0; i<fields().count(); i++) ixs.append(i);
	}
	else {
		QStringList sl = col_names.split(',', SkipEmptyParts);
		foreach(QString s, sl) {
			int ix = fields().fieldIndex(s);
			if(ix >= 0)
				ixs.append(ix);
			else
				qfError() << "Field not found:" << s;
		}
	}
	QVariantMap obj_m;
	QVariantList flds_lst;
	for(int i=0; i<ixs.count(); i++) {
		QVariantMap fld_m;
		Field f = field(ixs[i]);
		fld_m["name"] = f.name();
		fld_m["type"] = f.type().name();
		flds_lst << fld_m;
	}
	obj_m["fields"] = flds_lst;
	/// export data
	QVariantList rows_lst;
	for(int i=0; i<rowCount(); i++) {
		TableRow r = row(i);
		QVariantList row_lst;
		for(int i=0; i<ixs.count(); i++) {
			row_lst << r.value(ixs[i]);
		}
		rows_lst << row_lst;
	}
	obj_m["rows"] = rows_lst;
	return obj_m;
}

TreeTable Table::toTreeTable(const QString &col_names, const QString &table_name) const
{
	TreeTable tt(table_name);
	QList<int> ixs;
	if(col_names.isEmpty() || col_names == "*") {
		for(int i=0; i<fields().count(); i++)
			ixs.append(i);
	}
	else {
		QStringList sl = col_names.split(',', SkipEmptyParts);
		foreach(QString s, sl) {
			int ix = fields().fieldIndex(s);
			if(ix >= 0)
				ixs.append(ix);
			else
				qfError() << "Field not found:" << s;
		}
	}
	for(int i=0; i<ixs.count(); i++) {
		Field f = field(ixs[i]);
		tt.appendColumn(f.name(), f.type());
	}

	/// export data
	{
		for(int j=0; j<rowCount(); j++) {
			int ix = tt.appendRow();
			TreeTableRow tt_row = tt.row(ix);
			TableRow r = row(j);
			for(int i=0; i<ixs.count(); i++) {
				tt_row.setValue(i, r.value(ixs[i]));
			}
			tt.setRow(ix, tt_row);
		}
	}
	return tt;
}

bool Table::fromTreeTable(const TreeTable &tree_table)
{
	qfLogFuncFrame();
	bool ret = true;
	{
		for(int i=0; i<columnCount(); i++) {
			//qfDebug() << "\tfield:" << s;
			TreeTableColumn cc = tree_table.column(i);
			Field fld(cc.name(), cc.type());
			//fld.setCanUpdate(true);
			fieldsRef().append(fld);
		}
	}
	for(int i=0; i<tree_table.rowCount(); i++) {
		TableRow row(tableProperties());
		TreeTableRow rr = tree_table.row(i);
		for(int j=0; j<columnCount(); j++) {
			row.setValue(j, rr.value(j));
		}
		row.clearOrigValues();
		d->rows.append(row);
	}
	createRowIndex();
	return ret;
}

QVariantList Table::dataToVariantList() const
{
	QVariantList ret;
	Q_FOREACH(auto r, rows()) {
		QVariantList lst;
		Q_FOREACH(auto v, r.values())
			lst << v;
		QVariant v = lst; /// pozor. 4.5 maji appent pro QList
		///ret.append(lst); tohle nefunguje
		ret.append(v);
	}
	return ret;
}

void Table::dataFromVariantList(const QVariantList &_lst)
{
	clearRows();
	foreach(QVariant _v, _lst) {
		QVariantList lst = _v.toList();
		TableRow r(tableProperties());
		int i = 0;
		foreach(QVariant v, lst) {
			if(i < columnCount()) r.setValue(i++, v);
		}
		r.clearOrigValues();
		d->rows.append(r);
	}
	createRowIndex();
}
void Table::sort(RowIndexList::iterator begin, RowIndexList::iterator end)
{
	std::sort(begin, end, LessThan(*this));
}

Table::RowIndexList::const_iterator Table::binaryFind(Table::RowIndexList::const_iterator begin, Table::RowIndexList::const_iterator end, const QVariant & val) const
{
	auto it = std::lower_bound(begin, end, val, LessThan(*this));
	return it;
}





