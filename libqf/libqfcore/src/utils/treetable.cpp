#include "treetable.h"

#include "../core/log.h"
#include "../core/utils.h"
#include "../core/string.h"
#include "../utils/timescope.h"

#include <QMetaType>
#include <algorithm>

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
static const auto SkipEmptyParts = QString::SkipEmptyParts;
#else
static const auto SkipEmptyParts = Qt::SkipEmptyParts;
#endif

using namespace qf::core::utils;

//=================================================
// TreeTableColumn
//=================================================
QString TreeTableColumn::name() const
{
	return m_values.value(TreeTable::KEY_NAME).toString();
}

void TreeTableColumn::setName(const QString& n)
{
	setKeyValue(TreeTable::KEY_NAME, n);
}

QString TreeTableColumn::typeName() const
{
	int t = m_values.value(TreeTable::KEY_TYPE).toInt();
#if QT_VERSION_MAJOR >= 6
	const char *tn = QMetaType(t).name();
#else
	const char *tn = QMetaType::typeName(t);
#endif
	return QString::fromUtf8(tn);
}

void TreeTableColumn::setTypeName(const QString& tn)
{
#if QT_VERSION_MAJOR >= 6
	int t = QMetaType::fromName(tn.toUtf8().constData()).id();
#else
	int t = QMetaType::type(tn.toUtf8().constData());
#endif
	if(t == 0) {
		qfError() << "Invalid type name" << tn;
		return;
	}
	setType(t);
}

int TreeTableColumn::type() const
{
	return m_values.value(TreeTable::KEY_TYPE).toInt();
}

void TreeTableColumn::setType(int t)
{
#if QT_VERSION_MAJOR >= 6
	const char *tn = QMetaType(t).name();
#else
	const char *tn = QMetaType::typeName(t);
#endif
	if(!tn) {
		qfWarning() << "Invalid meta type id:" << t;
		return;
	}
	setKeyValue(TreeTable::KEY_TYPE, t);
}

QVariant TreeTableColumn::width() const
{
	return m_values.value(TreeTable::KEY_WIDTH);
}

void TreeTableColumn::setWidth(const QVariant& w)
{
	setKeyValue(TreeTable::KEY_WIDTH, w);
}

QString TreeTableColumn::header() const
{
	QString ret = m_values.value(TreeTable::KEY_HEADER).toString();
	if(ret.isEmpty())
		Utils::parseFieldName(name(), &ret);
	return ret;
}

void TreeTableColumn::setHeader(const QString &s)
{
	setKeyValue(TreeTable::KEY_HEADER, s.isEmpty()? QVariant(): QVariant(s));
}

QString TreeTableColumn::footer() const
{
	QString ret = m_values.value(TreeTable::KEY_FOOTER).toString();
	return ret;
}

void TreeTableColumn::setKeyValue(const QString &key, const QVariant &val)
{
	if(val.isValid())
		m_values[key] = val;
	else
		m_values.remove(key);
}

QString TreeTableColumn::halign() const
{
	QString ret = m_values.value(TreeTable::KEY_HALIGN).toString();
	if(ret.isEmpty()) {
		/// pokud neni hodnota predepsana, vem ji z dat
		switch(type()) {
			case QMetaType::Int:
			case QMetaType::UInt:
			case QMetaType::LongLong:
			case QMetaType::ULongLong:
			case QMetaType::Double:
				ret = "right";
				break;
			default:
				break;
		}
	}
	return ret;
}

int TreeTableRow::columnIndex(const QString &col_name) const
{
	return TreeTable::columnIndex(m_columns, col_name);
}

QVariant TreeTableRow::value(int col_ix) const
{
	if(0 <= col_ix && col_ix < m_columns.count()) {
		QVariant v;
#if QT_VERSION_MAJOR >= 6
		if(m_row.typeId() == QMetaType::QVariantMap) {
#else
		if(m_row.type() == QVariant::Map) {
#endif
			v = m_row.toMap().value(TreeTable::KEY_ROW).toList().value(col_ix);
		}
		else {
			v = m_row.toList().value(col_ix);
		}
		/// pretypuj na typ sloupce
		return retypeVariant(col_ix, v);
	}
	qfWarning() << "Column index:" << col_ix << "of:" << m_columns.count() << "does not exist.";
	return QVariant();
}

QVariant TreeTableRow::value(const QString &col_or_key_name) const
{
	QVariant v;
	int ix = columnIndex(col_or_key_name);
	if(ix < 0) {
		v = m_row.toMap().value(TreeTable::KEY_KEYVALS).toMap().value(col_or_key_name);
	}
	else {
		v = value(ix);
	}
	return v;
}

void TreeTableRow::setValue(int col_ix, const QVariant &val)
{
	if(col_ix < 0 || col_ix >= columnCount()) {
		qfWarning() << "Invalid column index:" << col_ix << "of:" << columnCount();
		return;
	}
#if QT_VERSION_MAJOR >= 6
	if(m_row.typeId() == QMetaType::QVariantMap) {
#else
	if(m_row.type() == QVariant::Map) {
#endif
		QVariantMap rm = m_row.toMap();
		QVariantList rvals = rm.value(TreeTable::KEY_ROW).toList();
		while (rvals.count() <= col_ix)
			rvals << QVariant();
		rvals[col_ix] = val;
		rm[TreeTable::KEY_ROW] = rvals;
		m_row = rm;
	}
	else {
		QVariantList rvals = m_row.toList();
		while (rvals.count() <= col_ix)
			rvals << QVariant();
		rvals[col_ix] = val;
		m_row = rvals;
	}
}

void TreeTableRow::setValue(const QString &col_or_key_name, const QVariant &val)
{
	int ix = columnIndex(col_or_key_name);
	if(ix < 0) {
		QVariantMap rm;
#if QT_VERSION_MAJOR >= 6
		if(m_row.typeId() != QMetaType::QVariantMap)
#else
		if(m_row.type() != QVariant::Map)
#endif
			rm = QVariantMap{ {TreeTable::KEY_ROW, m_row} };
		else
			rm = m_row.toMap();
		QVariantMap kvals = rm.value(TreeTable::KEY_KEYVALS).toMap();
		if(val.isValid())
			kvals[col_or_key_name] = val;
		else
			kvals.remove(col_or_key_name);
		rm[TreeTable::KEY_KEYVALS] = kvals;
		m_row = rm;
	}
	else {
		setValue(ix, val);
	}
}

QVariant TreeTableRow::retypeVariant(int col_ix, const QVariant &v) const
{
	if(v.isValid()) {
		TreeTableColumn cd(m_columns.value(col_ix).toMap());
		int t = cd.type();
		if(t > 0) {
			return Utils::retypeVariant(v, t);
		}
	}
	return v;
}

int TreeTableRow::tablesCount() const
{
#if QT_VERSION_MAJOR >= 6
	if(m_row.typeId() == QMetaType::QVariantMap) {
#else
	if(m_row.type() == QVariant::Map) {
#endif
		return m_row.toMap().value(TreeTable::KEY_TABLES).toList().count();
	}
	return 0;
}

TreeTable TreeTableRow::table(int ix) const
{
#if QT_VERSION_MAJOR >= 6
	if(m_row.typeId() == QMetaType::QVariantMap) {
#else
	if(m_row.type() == QVariant::Map) {
#endif
		QVariant t = m_row.toMap().value(TreeTable::KEY_TABLES).toList().value(ix);
		return TreeTable(t);
	}
	return TreeTable();
}

TreeTable TreeTableRow::table(const QString &table_name) const
{
	QVariantList tlst = m_row.toMap().value(TreeTable::KEY_TABLES).toList();
	for (int i = 0; i < tlst.count(); ++i) {
		TreeTable tt{tlst[i]};
		if(tt.name() == table_name)
			return tt;
	}
	return TreeTable();
}

void TreeTableRow::appendTable(const TreeTable &t)
{
	QVariantMap rm;
#if QT_VERSION_MAJOR >= 6
	if(m_row.typeId() == QMetaType::QVariantList)
#else
	if(m_row.type() == QVariant::List)
#endif
		rm[TreeTable::KEY_ROW] = m_row;
	else
		rm = m_row.toMap();
	QVariantList tlst = rm.value(TreeTable::KEY_TABLES).toList();
	tlst << t.toVariant();
	rm[TreeTable::KEY_TABLES] = tlst;
	m_row = rm;
}

//=================================================
// TreeTable
//=================================================
const QString TreeTable::KEY_COLUMNS = "fields";
const QString TreeTable::KEY_HEADER = "header";
const QString TreeTable::KEY_FOOTER = "footer";
const QString TreeTable::KEY_HALIGN = "halign";
const QString TreeTable::KEY_VALIGN = "valign";
const QString TreeTable::KEY_WIDTH = "width";
const QString TreeTable::KEY_ROWS = "rows";
const QString TreeTable::KEY_META = "meta";
const QString TreeTable::KEY_NAME = "name";
const QString TreeTable::KEY_TYPE = "type";
const QString TreeTable::KEY_TABLES = "tables";
const QString TreeTable::KEY_ROW = "row";
const QString TreeTable::KEY_KEYVALS = "keyvals";

void TreeTable::setName(const QString &n)
{
	QVariantMap t = m_values.toMap();
	QVariantMap m = t.value(KEY_META).toMap();
	if(n.isEmpty())
		m.remove(KEY_NAME);
	else
		m[KEY_NAME] = n;
	t[KEY_META] = m;
	m_values = t;
}

int TreeTable::insertRow(int ix, const QVariantList &vals)
{
	ix = std::max(ix, 0);
	ix = std::min(ix, rowCount());
	appendRow();
	QVariantList rr = rows();
	for (int i = rr.count() - 1; i > ix; --i) {
		rr[i] = rr[i-1];
	}
	rr[ix] = vals;
	setRows(rr);
	return ix;
}

int TreeTable::appendRow(const QVariantList &vals)
{
	QVariantList rr = rows();
	rr.insert(rr.count(), vals);
	setRows(rr);
	return rowCount() - 1;
}

void TreeTable::removeRow(int ix)
{
	if(ix >= 0 &&  ix < rowCount()) {
		QVariantList rr = rows();
		rr.removeAt(ix);
		setRows(rr);
	}
}

void TreeTable::appendColumn(const QString &name, QMetaType type, const QString &caption)
{
	TreeTableColumn cc;
	cc.setName(name);
	cc.setType(type.id());
	cc.setHeader(caption);
	appendColumn(cc);
}

void TreeTable::appendColumn(const TreeTableColumn &c)
{
	QVariantList cc = columns();
	cc << c.values();
	setColumns(cc);
}

TreeTableColumn TreeTable::column(int col_ix) const
{
	return TreeTableColumn(columns().value(col_ix).toMap());
}

void TreeTable::setColumn(int col_ix, const TreeTableColumn &ttc)
{
	if(col_ix < 0 || col_ix >= columnCount()) {
		qfWarning() << "Invalid column index:" << col_ix << "of:" << columnCount();
		return;
	}
	QVariantList cc = columns();
	cc[col_ix] = ttc.values();
	setColumns(cc);
}

TreeTableRow TreeTable::row(int row_ix) const
{
	return TreeTableRow(columns(), rows().value(row_ix));
}

void TreeTable::setRow(int row_ix, const TreeTableRow &ttr)
{
	if(row_ix < 0 || row_ix >= rowCount()) {
		qfWarning() << "Onvalid row index:" << row_ix << "of:" << rowCount();
		return;
	}
	QVariantList rr = rows();
	rr[row_ix] = ttr.row();
	setRows(rr);
}

QVariant TreeTable::value(const QString &_key_name, const QVariant &default_val, bool key_ends_with) const
{
	qfLogFuncFrame() << _key_name;
	String key_name = _key_name.trimmed();
	QVariant ret = default_val;
	QString path;
	{
		int ix = key_name.lastIndexOf('/');
		if(ix >= 0) {
			path = key_name.mid(0, ix+1);
			key_name = key_name.mid(ix+1);
		}
	}
	//qfDebug() << "\t table is null:" << t.isNull();
	//qfDebug() << "\t path:" << path;
	//qfDebug() << "\t data_src:" << ds;
	if(!path.isEmpty()) {
		TreeTable t = cd(path);
		ret = t.value(key_name);
	}
	else {
		if(key_name.startsWith("SUM(", Qt::CaseInsensitive)) {
			key_name = key_name.slice(4, -1).trimmed();
			ret = sum(key_name);
		}
		else if(key_name.startsWith("AVG(", Qt::CaseInsensitive)) {
			key_name = key_name.slice(4, -1).trimmed();
			ret = average(key_name);
		}
		else if(key_name.startsWith("CNT(", Qt::CaseInsensitive)) {
			key_name = key_name.slice(4, -1).trimmed();
			ret = rowCount();
		}
		else if(key_name.startsWith("CAPTION(", Qt::CaseInsensitive)) {
			key_name = key_name.slice(8, -1).trimmed();
			int ix = columnIndex(key_name);
			if(ix < 0)
				ret = _key_name;
			else
				ret = column(key_name).header();
		}
		else if(key_name.startsWith("HEADER(", Qt::CaseInsensitive)) {
			key_name = key_name.slice(7, -1).trimmed();
			int ix = columnIndex(key_name);
			if(ix < 0)
				ret = _key_name;
			else
				ret = column(key_name).header();
		}
		else if(key_name.startsWith("FOOTER(", Qt::CaseInsensitive)) {
			key_name = key_name.slice(7, -1).trimmed();
			int ix = columnIndex(key_name);
			if(ix < 0)
				ret = _key_name;
			else
				ret = column(key_name).footer();
		}
		else {
			QVariantMap kv = keyvals();
			QMapIterator<QString, QVariant> it(kv);
			while(it.hasNext()) {
				it.next();
				QString key = it.key();
				//qfDebug() << "\t checking key:" << key;
				if(key_ends_with) {
					if(Utils::fieldNameEndsWith(key, key_name)) {
						ret = it.value();
						//qfDebug() << "\t\t ends with" << sv.toString(2);
						break;
					}
				}
				else {
					if(QString::compare(key, key_name, Qt::CaseInsensitive) == 0) {
						ret = it.value();
						//qfDebug() << "\t\t equal";
						break;
					}
				}
			}
		}
	}
	qfDebug() << "\treturn:" << ret.toString();
	return ret;
}

void TreeTable::setValue(const QString& key_name, const QVariant& val)
{
	QVariantMap kv = keyvals();
	kv[key_name] = val;
	setKeyvals(kv);
}

void TreeTable::appendTable(int row_ix, const TreeTable &t)
{
	auto r = row(row_ix);
	r.appendTable(t);
	setRow(row_ix, r);
}

QVariant TreeTable::sum(const QString &col_name) const
{
	//qfDebug() << QF_FUNC_NAME << col_name;
	int ix = columnIndex(col_name);
	if(ix >= 0)
		return sum(ix);
	return QVariant();
}

QVariant TreeTable::sum(int col_index) const
{
	//qfDebug() << QF_FUNC_NAME << "col index:" << col_index;
	if(col_index < 0 || col_index >= columnCount())
		return QVariant();
	QString ts = columns().value(col_index).toMap().value(KEY_TYPE).toString();
#if QT_VERSION_MAJOR >= 6
	auto t = QMetaType::fromName(qPrintable(ts)).id();
#else
	int t = QVariant::nameToType(qPrintable(ts));
#endif
	QVariant ret;
	//qfInfo() << "type:" << QVariant::typeToName(t);
	if(t == QMetaType::Int) {
		int s = 0;
		for(int i=0; i<rowCount(); i++) {
			s += row(i).value(col_index).toInt();
		}
		ret = s;
	}
	else {
		double s = 0;
		for(int i=0; i<rowCount(); i++) {
			s += row(i).value(col_index).toDouble();
		}
		ret = s;
	}
	return ret;
}

QVariant TreeTable::average(const QString & col_name) const
{
	//qfDebug() << QF_FUNC_NAME << col_name;
	int ix = columnIndex(col_name);
	if(ix >= 0)
		return average(ix);
	return QVariant();
}

QVariant TreeTable::average(int col_index) const
{
	QVariant ret;
	double s = sum(col_index).toDouble();
	int n = rowCount();
	if(n > 0)
		ret = s / n;
	return ret;
}

TreeTable TreeTable::cd(const QString& path) const
{
	qfLogFuncFrame() << "path:" << path;
	qfDebug() << "\t isValid():" << isValid();
	QStringList path_lst = path.split('/', SkipEmptyParts);
	TreeTable ret = *this;
	for(int path_ix=0; path_ix<path_lst.count(); path_ix++) {
		bool ok;
		int row_ix = path_lst.value(path_ix).toInt(&ok);
		path_ix++;
		if(ok) {
			qfDebug() << "\t loading row #" << row_ix;
			QString table_name = path_lst.value(path_ix);
			ret = row(row_ix).table(table_name);
		}
		else {
			qfError() << QString("cd() - Bad path, path '%1'.").arg(path_lst.join("/"));
			ret = TreeTable();
			break;
		}
	}
	return ret;
}

QVariantMap TreeTable::keyvals(int row_ix) const
{
	return rows().value(row_ix).toMap().value(KEY_KEYVALS).toMap();
}
namespace {
QString line_indent(const QString &ind, int level)
{
	QString s;
	for(int i=0; i<level; i++) s += ind;
	return s;
}
#define IND(level) offset + line_indent(ind, level)

QString keyvals_to_html(const QVariantMap &keyvals, const QVariantMap &opts)
{
	QString offset = opts.value("lineOffset").toString();
	QString eoln = opts.value("lineSeparator", "\n").toString();
	QString ind = opts.value("lineIndent", "  ").toString();
	QString ret;
	QStringList sl = keyvals.keys();
	if(!sl.isEmpty()) {
		ret += IND(0) + "<table border=1>" + eoln;
		foreach(QString s, sl) {
			ret += IND(1) + "<tr>" + eoln;
			ret += IND(2) + "<th>" + s + "</th>" + eoln;
			ret += IND(2) + "<td>" + keyvals.value(s).toString() + "</td>" + eoln;
			ret += IND(1) + "</tr>" + eoln;
		}
		ret += IND(0) + "</table>" + eoln;
	}
	return ret;
}
}
QString TreeTable::toHtml(const QVariantMap &opts) const
{
	QVariantMap opts2 = opts;
	QString offset = opts.value("lineOffset").toString();
	QString eoln = opts.value("lineSeparator", "\n").toString();
	QString ind = opts.value("lineIndent", "  ").toString();
	QString ret;
	ret += name();
	ret += IND(0) + "<table border=1>" + eoln;
	QVariantList cc = columns();
	ret += IND(1) + "<tr>" + eoln;
	for(int i=0; i<cc.count(); i++) {
		QVariantMap cd = cc[i].toMap();
		ret += IND(2) + "<th>";
		ret += cd.value(KEY_HEADER).toString();
		ret += "</th>" + eoln;
	}
	ret += IND(1) + "</tr>" + eoln;
	for(int i=0; i<rowCount(); i++) {
		TreeTableRow rr = row(i);
		ret += IND(1) + "<tr>" + eoln;
		for(int j=0; j<cc.count(); j++) {
			ret += IND(2) + "<td>";
			ret += rr.value(j).toString();
			ret += "</td>" + eoln;
		}
		ret += IND(1) + "</tr>" + eoln;
		opts2["lineOffset"] = IND(3);
		for(int j=0; j<rr.tablesCount(); j++) {
			TreeTable t = rr.table(j);
			ret += IND(1) + "<tr>" + eoln;
			ret += IND(2) + "<td colspan=" QF_IARG(cc.count()) ">" + eoln;
			ret += t.toHtml(opts2);
			ret += IND(2) + "</td>" + eoln;
			ret += IND(1) + "</tr>" + eoln;
		}
		QVariantMap kv = keyvals(i);
		if(kv.count()) {
			ret += IND(1) + "<tr>" + eoln;
			ret += IND(2) + "<td colspan=" QF_IARG(cc.count()) ">" + eoln;
			opts2["lineOffset"] = IND(3);
			ret += keyvals_to_html(kv, opts2);
			ret += IND(2) + "</td>" + eoln;
			ret += IND(1) + "</tr>" + eoln;
		}
	}
	ret += IND(0) + "</table>" + eoln;
	/// keyvals
	auto kvs = keyvals();
	if(kvs.count()) {
		opts2["lineOffset"] = IND(0);
		ret += keyvals_to_html(kvs, opts2);
	}
	return ret;
}

QByteArray TreeTable::toJson(QJsonDocument::JsonFormat format) const
{
	QJsonDocument doc = QJsonDocument::fromVariant(toVariant());
	QByteArray ba = doc.toJson(format);
	return ba;
}

QString TreeTable::toString(QJsonDocument::JsonFormat format) const
{
	auto ba = toJson(format);
	return QString::fromUtf8(ba);
}

void TreeTable::setKeyvals(const QVariantMap &keyvals)
{
	QVariantMap t = valueMap();
	t[KEY_KEYVALS] = keyvals;
	m_values = t;
}

void TreeTable::setColumns(const QVariantList &cols)
{
	QVariantMap t = valueMap();
	t[KEY_COLUMNS] = cols;
	m_values = t;
}

int TreeTable::columnIndex(const QVariantList &cols, const QString &col_name)
{
	for (int i = 0; i < cols.count(); ++i) {
		QVariantMap cd = cols[i].toMap();
		if(Utils::fieldNameEndsWith(cd.value(KEY_NAME).toString(), col_name))
			return i;
	}
	return -1;
}

void TreeTable::setRows(const QVariantList &rows)
{
	QVariantMap t = valueMap();
	t[KEY_ROWS] = rows;
	m_values = t;
}

