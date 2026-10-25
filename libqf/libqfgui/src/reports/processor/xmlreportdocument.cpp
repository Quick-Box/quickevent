#include "xmlreportdocument.h"
#include "reportitemband.h"
#include "reportitembreak.h"
#include "reportitemdetail.h"
#include "reportitemimage.h"
#include "reportitempara.h"
#include "reportitemreport.h"
#include "reportprocessor.h"
#include "style/sheet.h"

#include <qf/core/log.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMetaProperty>
#include <QRegularExpression>

#include <algorithm>
#include <memory>

using namespace qf::gui::reports;

namespace {

//==========================================================
// object types
//==========================================================
struct ObjectType
{
	const QMetaObject *metaObject;
	std::function<QObject* (QObject *parent)> create;
};

template<typename T>
ObjectType reportItemType()
{
	return ObjectType{&T::staticMetaObject, [](QObject *) -> QObject* { return new T(); }};
}

template<typename T>
ObjectType styleType()
{
	return ObjectType{&T::staticMetaObject, [](QObject *parent) -> QObject* { return new T(parent); }};
}

const QMap<QString, ObjectType>& objectTypes()
{
	static const QMap<QString, ObjectType> types {
		{"Report", reportItemType<ReportItemReport>()},
		{"Frame", reportItemType<ReportItemFrame>()},
		{"Space", reportItemType<ReportItemFrame>()},
		{"Band", reportItemType<ReportItemBand>()},
		{"Detail", reportItemType<ReportItemDetail>()},
		{"Para", reportItemType<ReportItemPara>()},
		{"Image", reportItemType<ReportItemImage>()},
		{"Break", reportItemType<ReportItemBreak>()},
		{"StyleSheet", styleType<style::Sheet>()},
		{"Color", styleType<style::Color>()},
		{"Pen", styleType<style::Pen>()},
		{"Brush", styleType<style::Brush>()},
		{"Font", styleType<style::Font>()},
		{"TextStyle", styleType<style::Text>()},
	};
	return types;
}

const ObjectType* objectTypeForMetaObject(const QMetaObject *mo)
{
	for(const auto &type : objectTypes()) {
		if(type.metaObject == mo)
			return &type;
	}
	return nullptr;
}

const auto INCLUDE_TAG = QStringLiteral("include");
const auto PARAMETER_TAG = QStringLiteral("Parameter");
const auto REPEAT_TAG = QStringLiteral("Repeat");
const auto SRC_ATTR = QStringLiteral("src");

//==========================================================
// expressions
//==========================================================
bool isTruthy(const QVariant &v)
{
	if(!v.isValid() || v.isNull())
		return false;
	switch (v.typeId()) {
	case QMetaType::Bool: return v.toBool();
	case QMetaType::QString: {
		auto s = v.toString();
		return !s.isEmpty() && s != QLatin1String("false") && s != QLatin1String("0");
	}
	default:
		bool ok;
		double d = v.toDouble(&ok);
		return ok? d != 0: true;
	}
}

QString valueToString(const QVariant &v)
{
	if(!v.isValid() || v.isNull())
		return {};
	switch (v.typeId()) {
	case QMetaType::QDateTime: return v.toDateTime().toString(Qt::ISODate);
	case QMetaType::QDate: return v.toDate().toString(Qt::ISODate);
	case QMetaType::QTime: return v.toTime().toString(Qt::ISODate);
	default: return v.toString();
	}
}

QMap<QString, XmlReportDocument::Formatter>& formatters()
{
	static QMap<QString, XmlReportDocument::Formatter> fmts {
		{"date", [](const QVariant &v, const QString &arg) -> QVariant {
			auto d = v.toDate();
			return arg.isEmpty()? d.toString(Qt::ISODate): d.toString(arg);
		}},
		{"time", [](const QVariant &v, const QString &arg) -> QVariant {
			auto t = v.toTime();
			return arg.isEmpty()? t.toString(Qt::ISODate): t.toString(arg);
		}},
		{"datetime", [](const QVariant &v, const QString &arg) -> QVariant {
			auto dt = v.toDateTime();
			return arg.isEmpty()? dt.toString(Qt::TextDate): dt.toString(arg);
		}},
		{"number", [](const QVariant &v, const QString &arg) -> QVariant {
			bool ok;
			auto d = v.toDouble(&ok);
			if(!ok)
				return QString();
			return QString::number(d, 'f', arg.toInt());
		}},
		{"add", [](const QVariant &v, const QString &arg) -> QVariant {
			double d = v.toDouble() + arg.toDouble();
			if(d == static_cast<qint64>(d))
				return static_cast<qint64>(d);
			return d;
		}},
		{"default", [](const QVariant &v, const QString &arg) -> QVariant {
			return valueToString(v).isEmpty()? QVariant(arg): v;
		}},
		{"clamp", [](const QVariant &v, const QString &arg) -> QVariant {
			return std::clamp(v.toDouble(), arg.section(':', 0, 0).toDouble(), arg.section(':', 1).toDouble());
		}},
		{"if", [](const QVariant &v, const QString &arg) -> QVariant {
			return isTruthy(v)? arg.section(':', 0, 0): arg.section(':', 1);
		}},
		{"not", [](const QVariant &v, const QString &) -> QVariant {
			return !isTruthy(v);
		}},
		{"nonzero", [](const QVariant &v, const QString &) -> QVariant {
			bool ok;
			double d = v.toDouble(&ok);
			return (ok && d == 0)? QVariant(QString()): v;
		}},
		{"positive", [](const QVariant &v, const QString &) -> QVariant {
			bool ok;
			double d = v.toDouble(&ok);
			return (ok && d > 0)? v: QVariant(QString());
		}},
		{"arg", [](const QVariant &v, const QString &arg) -> QVariant {
			return valueToString(v).arg(arg);
		}},
		{"key", [](const QVariant &v, const QString &arg) -> QVariant {
			return v.toMap().value(arg);
		}},
		{"prefix", [](const QVariant &v, const QString &arg) -> QVariant {
			auto s = valueToString(v);
			return s.isEmpty()? s: arg + s;
		}},
		{"suffix", [](const QVariant &v, const QString &arg) -> QVariant {
			auto s = valueToString(v);
			return s.isEmpty()? s: s + arg;
		}},
	};
	return fmts;
}

QMap<QString, XmlReportDocument::Function>& functions()
{
	static QMap<QString, XmlReportDocument::Function> fns {
		{"select", [](const XmlReportDocument::ExpressionContext &, const QVariantList &args) -> QVariant {
			return isTruthy(args.value(0))? args.value(1): args.value(2);
		}},
		{"coalesce", [](const XmlReportDocument::ExpressionContext &, const QVariantList &args) -> QVariant {
			for(const auto &v : args) {
				if(!valueToString(v).isEmpty())
					return v;
			}
			return {};
		}},
		{"add", [](const XmlReportDocument::ExpressionContext &, const QVariantList &args) -> QVariant {
			double d = 0;
			for(const auto &v : args)
				d += v.toDouble();
			return d;
		}},
		{"sub", [](const XmlReportDocument::ExpressionContext &, const QVariantList &args) -> QVariant {
			return args.value(0).toDouble() - args.value(1).toDouble();
		}},
		{"mul", [](const XmlReportDocument::ExpressionContext &, const QVariantList &args) -> QVariant {
			double d = 1;
			for(const auto &v : args)
				d *= v.toDouble();
			return d;
		}},
		{"div", [](const XmlReportDocument::ExpressionContext &, const QVariantList &args) -> QVariant {
			double d = args.value(1).toDouble();
			return (d == 0)? QVariant(): QVariant(args.value(0).toDouble() / d);
		}},
		{"and", [](const XmlReportDocument::ExpressionContext &, const QVariantList &args) -> QVariant {
			return std::all_of(args.cbegin(), args.cend(), isTruthy);
		}},
		{"not", [](const XmlReportDocument::ExpressionContext &, const QVariantList &args) -> QVariant {
			return !isTruthy(args.value(0));
		}},
		{"band", [](const XmlReportDocument::ExpressionContext &ctx, const QVariantList &args) -> QVariant {
			auto *band = qobject_cast<ReportItemBand*>(ctx.item());
			if(!band)
				band = qf::core::Utils::findParent<ReportItemBand*>(ctx.item(), false);
			return band? band->data(args.value(0).toString()): QVariant();
		}},
		{"or", [](const XmlReportDocument::ExpressionContext &, const QVariantList &args) -> QVariant {
			return std::any_of(args.cbegin(), args.cend(), isTruthy);
		}},
		{"eq", [](const XmlReportDocument::ExpressionContext &, const QVariantList &args) -> QVariant {
			return valueToString(args.value(0)) == valueToString(args.value(1));
		}},
		{"lt", [](const XmlReportDocument::ExpressionContext &, const QVariantList &args) -> QVariant {
			return args.value(0).toDouble() < args.value(1).toDouble();
		}},
		{"gt", [](const XmlReportDocument::ExpressionContext &, const QVariantList &args) -> QVariant {
			return args.value(0).toDouble() > args.value(1).toDouble();
		}},
	};
	return fns;
}

/// split on separator outside of quoted strings and outside of function call parentheses
QStringList splitOutsideQuotes(const QString &s, QChar sep, int max_parts = -1)
{
	QStringList ret;
	QString part;
	QChar quote;
	int depth = 0;
	QChar prev;
	for(QChar c : s) {
		auto prev_char = prev;
		prev = c;
		if(!quote.isNull()) {
			if(c == quote)
				quote = QChar();
		}
		else if(c == '\'' || c == '"') {
			quote = c;
		}
		else if(c == '(' && (prev_char.isLetterOrNumber() || prev_char == '_')) {
			// function call, '(' in formatter argument like prefix:( is not counted
			depth++;
		}
		else if(c == ')' && depth > 0) {
			depth--;
		}
		else if(c == sep && depth == 0 && (max_parts < 0 || ret.size() < max_parts - 1)) {
			ret << part;
			part.clear();
			continue;
		}
		part += c;
	}
	ret << part;
	return ret;
}

struct Expression
{
	QString value;
	QList<std::pair<QString, QString>> formatters; // name, argument
};

Expression parseExpression(const QString &expression)
{
	Expression ret;
	auto parts = splitOutsideQuotes(expression, '|');
	ret.value = parts.takeFirst().trimmed();
	for(const auto &part : parts) {
		auto fmt = splitOutsideQuotes(part, ':', 2);
		// formatter without argument gets null argument, with empty one gets empty non-null string
		QString arg;
		if(fmt.size() > 1)
			arg = fmt.value(1).isNull()? QStringLiteral(""): fmt.value(1);
		ret.formatters << std::make_pair(fmt.value(0).trimmed(), arg);
	}
	return ret;
}

QString unquote(const QString &s)
{
	if(s.size() >= 2 && (s[0] == '\'' || s[0] == '"') && s.back() == s[0])
		return s.mid(1, s.size() - 2);
	return s;
}

QVariant evaluateExpression(const Expression &expr, const XmlReportDocument::ExpressionContext &ctx, const QString &tr_context);

QVariant evaluateValue(const QString &value, const XmlReportDocument::ExpressionContext &ctx, const QString &tr_context)
{
	if(value.isEmpty())
		return {};
	if(value[0] == '\'' || value[0] == '"')
		return unquote(value);
	{
		bool ok;
		double d = value.toDouble(&ok);
		if(ok)
			return d;
	}
	if(value.endsWith(')')) {
		auto ix = value.indexOf('(');
		auto name = value.left(ix).trimmed();
		auto it = functions().constFind(name);
		if(ix < 0 || it == functions().constEnd()) {
			// data can contain aggregated fields like SUM(field)
			if(!ctx.isLoading())
				return ctx.field(value);
			qfWarning() << "Unknown report function:" << name;
			return {};
		}
		QVariantList args;
		auto args_str = value.mid(ix + 1).chopped(1);
		if(!args_str.trimmed().isEmpty()) {
			for(const auto &arg : splitOutsideQuotes(args_str, ','))
				args << evaluateExpression(parseExpression(arg.trimmed()), ctx, tr_context);
		}
		return it.value()(ctx, args);
	}
	if(value[0] == '$') {
		auto name = value.mid(1);
		if(!ctx.isLoading()) {
			if(name == QLatin1String("page")) {
				auto *proc = ctx.item()->processor(!qf::core::Exception::Throw);
				return proc? proc->processedPageNo() + 1: QVariant();
			}
			if(name == QLatin1String("now"))
				return QDateTime::currentDateTime();
			if(name == QLatin1String("row") || name == QLatin1String("rowCount") || name == QLatin1String("lastRow")) {
				auto *detail = qobject_cast<ReportItemDetail*>(ctx.item());
				if(!detail)
					detail = qf::core::Utils::findParent<ReportItemDetail*>(ctx.item(), false);
				if(!detail)
					return {};
				if(name == QLatin1String("row"))
					return detail->currentIndex();
				if(name == QLatin1String("rowCount"))
					return detail->rowCount();
				return detail->currentIndex() == detail->rowCount() - 1;
			}
		}
		return ctx.parameter(name);
	}
	if(ctx.isLoading())
		return ctx.parameter(value);
	return ctx.field(value);
}

/// returns value, if it is different from value returned for previous detail row
QVariant changedValue(const QVariant &v, const XmlReportDocument::ExpressionContext &ctx)
{
	auto *item = ctx.item();
	if(!item)
		return v;
	auto *detail = qobject_cast<ReportItemDetail*>(item);
	if(!detail)
		detail = qf::core::Utils::findParent<ReportItemDetail*>(item, false);
	int row = detail? detail->currentIndex(): -1;
	// item can be printed more times for the same row, when it does not fit to page
	constexpr auto ROW_PROP = "__qf_changed_row";
	constexpr auto VALUE_PROP = "__qf_changed_value";
	constexpr auto RESULT_PROP = "__qf_changed_result";
	if(item->property(ROW_PROP).isValid() && item->property(ROW_PROP).toInt() == row)
		return item->property(RESULT_PROP);
	QVariant ret = (item->property(ROW_PROP).isValid() && item->property(VALUE_PROP) == v)? QVariant(QString()): v;
	item->setProperty(ROW_PROP, row);
	item->setProperty(VALUE_PROP, v);
	item->setProperty(RESULT_PROP, ret);
	return ret;
}

QVariant evaluateExpression(const Expression &expr, const XmlReportDocument::ExpressionContext &ctx, const QString &tr_context)
{
	QVariant v = evaluateValue(expr.value, ctx, tr_context);
	for(const auto &[name, arg] : expr.formatters) {
		if(name == QLatin1String("tr")) {
			auto s = valueToString(v);
			v = s.isEmpty()? s: QCoreApplication::translate(tr_context.toUtf8().constData(), s.toUtf8().constData());
			continue;
		}
		if(name == QLatin1String("changed")) {
			v = changedValue(v, ctx);
			continue;
		}
		if(name == QLatin1String("trn")) {
			v = QCoreApplication::translate(tr_context.toUtf8().constData(), unquote(arg).toUtf8().constData(), nullptr, v.toInt());
			continue;
		}
		auto it = formatters().constFind(name);
		if(it == formatters().constEnd()) {
			qfWarning() << "Unknown report formatter:" << name;
			continue;
		}
		// argument starting with $ is report parameter
		auto fmt_arg = arg.startsWith('$')? valueToString(evaluateValue(arg, ctx, tr_context)): unquote(arg);
		v = it.value()(v, fmt_arg);
	}
	return v;
}

/// text with embedded expressions
struct Template
{
	struct Segment
	{
		QString literal;
		std::optional<Expression> expression;
	};
	QList<Segment> segments;
	QString trContext;

	static Template compile(const QString &text, const QRegularExpression &rx, const QString &tr_context)
	{
		Template ret;
		ret.trContext = tr_context;
		qsizetype pos = 0;
		auto it = rx.globalMatch(text);
		while(it.hasNext()) {
			auto m = it.next();
			if(m.capturedStart() > pos)
				ret.segments << Segment{text.mid(pos, m.capturedStart() - pos), {}};
			ret.segments << Segment{{}, parseExpression(m.captured(1))};
			pos = m.capturedEnd();
		}
		if(pos < text.length())
			ret.segments << Segment{text.mid(pos), {}};
		return ret;
	}
	/// returns typed value, when template consists of single expression
	QVariant evaluate(const XmlReportDocument::ExpressionContext &ctx) const
	{
		if(segments.size() == 1 && segments[0].expression)
			return evaluateExpression(*segments[0].expression, ctx, trContext);
		QString ret;
		for(const auto &seg : segments)
			ret += seg.expression? valueToString(evaluateExpression(*seg.expression, ctx, trContext)): seg.literal;
		return ret;
	}
};

const QRegularExpression& loadTimeRx()
{
	static const QRegularExpression rx(QStringLiteral(R"(\$\{(.*?)\})"));
	return rx;
}

const QRegularExpression& printTimeRx()
{
	static const QRegularExpression rx(QStringLiteral(R"(\{\{(.*?)\}\})"));
	return rx;
}

//==========================================================
// loader
//==========================================================
QString resolveFileName(const QString &file_name, const QString &relative_to)
{
	QString fn = file_name;
	if(fn.startsWith(QLatin1String("qrc:/")))
		fn = fn.mid(3);
	if(QFileInfo(fn).isRelative() && !fn.startsWith(':'))
		fn = QFileInfo(relative_to).dir().filePath(fn);
	return fn;
}

bool loadDomDocument(const QString &file_name, QDomDocument &doc)
{
	QFile f(file_name);
	if(!f.open(QFile::ReadOnly)) {
		qfError() << "Cannot open report file:" << file_name << f.errorString();
		return false;
	}
	auto res = doc.setContent(&f);
	if(!res) {
		qfError() << "Report file:" << file_name << "parse error:" << res.errorMessage << "line:" << res.errorLine << "column:" << res.errorColumn;
		return false;
	}
	return true;
}

/// write QObject pointer property, returns error description
QString writePointerPropertyValue(QObject *obj, const QMetaProperty &prop, QObject *value)
{
	const QMetaObject *prop_mo = prop.metaType().metaObject();
	if(!(prop.metaType().flags() & QMetaType::PointerToQObject) || !prop_mo || !value->metaObject()->inherits(prop_mo))
		return QStringLiteral("Cannot assign %1 to property %2").arg(value->metaObject()->className(), prop.name());
	if(!value->parent())
		value->setParent(obj);
	if(!prop.write(obj, QVariant(prop.metaType(), &value)))
		return QStringLiteral("Cannot write property %1").arg(prop.name());
	return {};
}

/// write property from string value, returns error description
QString writePropertyValue(QObject *obj, const QMetaProperty &prop, const QString &value, QHash<QString, QObject*> *style_cache)
{
	const QString name = prop.name();
	QVariant v;
	if(prop.isEnumType()) {
		QMetaEnum me = prop.enumerator();
		bool ok;
		int n = prop.isFlagType()
				? me.keysToValue(value.toLatin1().constData(), &ok)
				: me.keyToValue(value.section('.', -1).toLatin1().constData(), &ok);
		if(!ok)
			return QStringLiteral("Invalid value %1 of enum property %2").arg(value, name);
		v = n;
	}
	else if(prop.metaType().flags() & QMetaType::PointerToQObject) {
		if(value.isEmpty()) {
			// no style object, it is inherited from parent
			QObject *null_obj = nullptr;
			if(!prop.write(obj, QVariant(prop.metaType(), &null_obj)))
				return QStringLiteral("Cannot write property %1").arg(name);
			return {};
		}
		// style object shorthand, new object based on named one
		QObject *o = style_cache? style_cache->value(value): nullptr;
		if(!o) {
			const ObjectType *type = objectTypeForMetaObject(prop.metaType().metaObject());
			if(!type)
				return QStringLiteral("Property %1 cannot be set from attribute").arg(name);
			o = type->create(obj);
			o->setProperty(qobject_cast<style::Color*>(o)? "def": "basedOn", value);
			if(style_cache)
				style_cache->insert(value, o);
		}
		return writePointerPropertyValue(obj, prop, o);
	}
	else if(prop.metaType().id() == QMetaType::QVariantMap) {
		QJsonParseError err;
		auto doc = QJsonDocument::fromJson(value.toUtf8(), &err);
		if(err.error != QJsonParseError::NoError)
			return QStringLiteral("Property %1 JSON parse error: %2").arg(name, err.errorString());
		v = doc.toVariant();
	}
	else if(prop.metaType().id() == QMetaType::QVariant) {
		v = value;
	}
	else {
		v = value;
		if(!v.convert(prop.metaType()))
			return QStringLiteral("Cannot convert '%1' to type of property %2").arg(value, name);
	}
	if(!prop.write(obj, v))
		return QStringLiteral("Cannot write property %1").arg(name);
	return {};
}

class Loader
{
public:
	Loader(const QString &file_name, const QVariantMap &parameters)
	{
		m_fileNames << file_name;
		m_parameters << parameters;
	}

	QObject* createObject(const QDomElement &el, QObject *parent);
	bool isError() const { return m_isError; }
	const QVariantMap& reportParameters() const { return m_parameters.first(); }
private:
	QObject* createIncluded(const QDomElement &el, QObject *parent);
	void createChild(QObject *obj, const QDomElement &child_el);
	void createRepeated(QObject *obj, const QDomElement &el);
	void addChildObject(QObject *obj, QObject *child, const QDomElement &child_el);
	void setPropertyElement(QObject *obj, const QDomElement &el);
	void setProperty(QObject *obj, const QString &name, const QString &value);
	void setText(ReportItemPara *para, const QString &text);
	void writePointerProperty(QObject *obj, const QMetaProperty &prop, QObject *value);

	QString trContext() const { return QFileInfo(m_fileNames.last()).completeBaseName(); }
	QVariant evaluate(const QString &text) const
	{
		if(!text.contains(QLatin1String("${")))
			return text;
		auto tmpl = Template::compile(text, loadTimeRx(), trContext());
		return tmpl.evaluate(XmlReportDocument::ExpressionContext(nullptr, &m_parameters));
	}
	QString interpolate(const QString &text) const { return valueToString(evaluate(text)); }

	void error(const QString &msg)
	{
		qfError() << m_fileNames.last() << msg;
		m_isError = true;
	}
	static QString elementPath(const QDomElement &el)
	{
		return QStringLiteral("<%1> line: %2").arg(el.tagName()).arg(el.lineNumber());
	}
private:
	QStringList m_fileNames;
	QList<QVariantMap> m_parameters;
	bool m_isError = false;
};

QObject *Loader::createIncluded(const QDomElement &el, QObject *parent)
{
	auto file_name = resolveFileName(el.attribute(SRC_ATTR), m_fileNames.last());
	if(m_fileNames.contains(file_name)) {
		error("Recursive include of: " + file_name);
		return nullptr;
	}
	QDomDocument doc;
	if(!loadDomDocument(file_name, doc)) {
		m_isError = true;
		return nullptr;
	}
	auto root_el = doc.documentElement();
	auto type_it = objectTypes().constFind(root_el.tagName());
	if(type_it == objectTypes().constEnd()) {
		error(QStringLiteral("Unknown root element %1 of included file %2").arg(elementPath(root_el), file_name));
		return nullptr;
	}
	QVariantMap params;
	QList<std::pair<QString, QString>> property_overrides;
	auto attrs = el.attributes();
	for(int i = 0; i < attrs.count(); i++) {
		auto attr = attrs.item(i).toAttr();
		if(attr.name() == SRC_ATTR)
			continue;
		if(type_it->metaObject->indexOfProperty(attr.name().toLatin1().constData()) >= 0 || attr.name() == QLatin1String("text"))
			property_overrides << std::make_pair(attr.name(), interpolate(attr.value()));
		else
			params[attr.name()] = evaluate(attr.value());
	}
	m_parameters << params;
	m_fileNames << file_name;
	QObject *obj = createObject(root_el, parent);
	m_fileNames.removeLast();
	m_parameters.removeLast();
	if(obj) {
		for(const auto &[name, value] : property_overrides)
			setProperty(obj, name, value);
	}
	return obj;
}

QObject *Loader::createObject(const QDomElement &el, QObject *parent)
{
	if(el.tagName() == INCLUDE_TAG)
		return createIncluded(el, parent);
	auto it = objectTypes().constFind(el.tagName());
	if(it == objectTypes().constEnd()) {
		error("Unknown element " + elementPath(el));
		return nullptr;
	}
	// parameters first, they can be used in attributes
	for(auto child_el = el.firstChildElement(PARAMETER_TAG); !child_el.isNull(); child_el = child_el.nextSiblingElement(PARAMETER_TAG)) {
		auto name = child_el.attribute(QStringLiteral("name"));
		if(name.isEmpty()) {
			error("Parameter name is empty " + elementPath(child_el));
			continue;
		}
		bool is_defined = std::any_of(m_parameters.cbegin(), m_parameters.cend(), [&name](const QVariantMap &m) { return m.contains(name); });
		if(!is_defined)
			m_parameters.last()[name] = evaluate(child_el.attribute(QStringLiteral("default")));
	}
	QObject *obj = it->create(parent);
	auto attrs = el.attributes();
	for(int i = 0; i < attrs.count(); i++) {
		auto attr = attrs.item(i).toAttr();
		setProperty(obj, attr.name(), interpolate(attr.value()));
	}
	QString text;
	for(auto nd = el.firstChild(); !nd.isNull(); nd = nd.nextSibling()) {
		if(nd.isText() || nd.isCDATASection()) {
			text += nd.toCharacterData().data();
			continue;
		}
		auto child_el = nd.toElement();
		if(!child_el.isNull())
			createChild(obj, child_el);
	}
	text = text.trimmed();
	if(!text.isEmpty()) {
		if(auto *para = qobject_cast<ReportItemPara*>(obj))
			setText(para, interpolate(text));
		else
			error("Only Para can contain text " + elementPath(el));
	}
	return obj;
}

void Loader::createChild(QObject *obj, const QDomElement &child_el)
{
	auto tag = child_el.tagName();
	if(tag == PARAMETER_TAG) {
		// already processed
	}
	else if(tag == REPEAT_TAG) {
		createRepeated(obj, child_el);
	}
	else if(tag == INCLUDE_TAG || objectTypes().contains(tag)) {
		QObject *child = createObject(child_el, obj);
		if(child)
			addChildObject(obj, child, child_el);
	}
	else {
		setPropertyElement(obj, child_el);
	}
}

void Loader::createRepeated(QObject *obj, const QDomElement &el)
{
	auto var = el.attribute(QStringLiteral("var"), QStringLiteral("i"));
	bool ok_from, ok_to;
	int from = interpolate(el.attribute(QStringLiteral("from"), QStringLiteral("1"))).toInt(&ok_from);
	int to = interpolate(el.attribute(QStringLiteral("to"))).toInt(&ok_to);
	if(!ok_from || !ok_to) {
		// can happen when report is validated without parameters
		qfDebug() << "Repeat range is not valid" << elementPath(el);
		return;
	}
	for(int i = from; i <= to; i++) {
		m_parameters << QVariantMap{{var, i}};
		for(auto child_el = el.firstChildElement(); !child_el.isNull(); child_el = child_el.nextSiblingElement())
			createChild(obj, child_el);
		m_parameters.removeLast();
	}
}

void Loader::addChildObject(QObject *obj, QObject *child, const QDomElement &child_el)
{
	if(auto *frame = qobject_cast<ReportItemFrame*>(obj)) {
		if(auto *item = qobject_cast<ReportItem*>(child)) {
			frame->addItem(item);
			return;
		}
	}
	else if(auto *sheet = qobject_cast<style::Sheet*>(obj)) {
		// style objects are looked up by name among sheet children
		child->setParent(sheet);
		if(auto *o = qobject_cast<style::Color*>(child)) { sheet->colors() << o; return; }
		if(auto *o = qobject_cast<style::Pen*>(child)) { sheet->pens() << o; return; }
		if(auto *o = qobject_cast<style::Brush*>(child)) { sheet->brushes() << o; return; }
		if(auto *o = qobject_cast<style::Font*>(child)) { sheet->fonts() << o; return; }
		if(auto *o = qobject_cast<style::Text*>(child)) { sheet->textStyles() << o; return; }
		// nested sheet, for example included one, its style objects are found among sheet children too
		if(qobject_cast<style::Sheet*>(child)) { return; }
	}
	error(QStringLiteral("Element %1 cannot be child of %2").arg(elementPath(child_el), obj->metaObject()->className()));
	delete child;
}

void Loader::setPropertyElement(QObject *obj, const QDomElement &el)
{
	const QMetaObject *mo = obj->metaObject();
	int ix = mo->indexOfProperty(el.tagName().toLatin1().constData());
	if(ix < 0) {
		error(QStringLiteral("Unknown element or property %1 of %2").arg(elementPath(el), mo->className()));
		return;
	}
	auto value_el = el.firstChildElement();
	if(value_el.isNull() || !value_el.nextSiblingElement().isNull()) {
		error("Property element must contain exactly one object element " + elementPath(el));
		return;
	}
	QObject *value = createObject(value_el, obj);
	if(value)
		writePointerProperty(obj, mo->property(ix), value);
}

void Loader::writePointerProperty(QObject *obj, const QMetaProperty &prop, QObject *value)
{
	auto err = writePointerPropertyValue(obj, prop, value);
	if(!err.isEmpty()) {
		error(err);
		delete value;
	}
}

void Loader::setText(ReportItemPara *para, const QString &text)
{
	if(text.contains(QLatin1String("{{"))) {
		auto tmpl = Template::compile(text, printTimeRx(), trContext());
		para->setGetTextFunction([para, tmpl]() {
			return valueToString(tmpl.evaluate(XmlReportDocument::ExpressionContext(para)));
		});
	}
	else {
		para->setText(text);
	}
}

void Loader::setProperty(QObject *obj, const QString &name, const QString &value)
{
	if(name == QLatin1String("text")) {
		if(auto *para = qobject_cast<ReportItemPara*>(obj)) {
			setText(para, value);
			return;
		}
	}
	const QMetaObject *mo = obj->metaObject();
	int ix = mo->indexOfProperty(name.toLatin1().constData());
	if(ix < 0) {
		error(QStringLiteral("Unknown property %1 of %2").arg(name, mo->className()));
		return;
	}
	QMetaProperty prop = mo->property(ix);
	if(value.contains(QLatin1String("{{"))) {
		auto *item = qobject_cast<ReportItem*>(obj);
		if(!item) {
			error(QStringLiteral("Print time expression can be used in report item properties only, property: %1").arg(name));
			return;
		}
		// evaluate before every print, style objects are created once for every value
		auto tmpl = Template::compile(value, printTimeRx(), trContext());
		auto style_cache = std::make_shared<QHash<QString, QObject*>>();
		item->addPrintBinding([item, prop, tmpl, style_cache]() {
			auto v = valueToString(tmpl.evaluate(XmlReportDocument::ExpressionContext(item)));
			auto err = writePropertyValue(item, prop, v, style_cache.get());
			if(!err.isEmpty())
				qfWarning() << "Print time binding error:" << err;
		});
		return;
	}
	auto err = writePropertyValue(obj, prop, value, nullptr);
	if(!err.isEmpty())
		error(err);
}

}

//==========================================================
// XmlReportDocument::ExpressionContext
//==========================================================
QVariant XmlReportDocument::ExpressionContext::parameter(const QString &path) const
{
	auto keys = path.split('.');
	auto name = keys.takeFirst();
	QVariant v;
	if(m_loadParameters) {
		for(auto it = m_loadParameters->crbegin(); it != m_loadParameters->crend(); ++it) {
			if(it->contains(name)) {
				v = it->value(name);
				break;
			}
		}
	}
	else {
		auto *root = qobject_cast<ReportItemReport*>(m_item);
		if(!root)
			root = qf::core::Utils::findParent<ReportItemReport*>(m_item, false);
		if(!root) {
			qfWarning() << "Cannot find report root for parameter:" << path;
			return {};
		}
		v = root->property(name.toUtf8().constData());
	}
	for(const auto &key : keys)
		v = v.toMap().value(key);
	return v;
}

QVariant XmlReportDocument::ExpressionContext::field(const QString &name) const
{
	for(QObject *o = m_item; o; o = o->parent()) {
		if(auto *detail = qobject_cast<ReportItemDetail*>(o))
			return detail->rowData(name);
		if(auto *band = qobject_cast<ReportItemBand*>(o))
			return band->data(name);
	}
	qfWarning() << "Cannot find Detail or Band to get data field:" << name;
	return {};
}

//==========================================================
// XmlReportDocument
//==========================================================
void XmlReportDocument::registerFormatter(const QString &name, const Formatter &fn)
{
	formatters()[name] = fn;
}

void XmlReportDocument::registerFunction(const QString &name, const Function &fn)
{
	functions()[name] = fn;
}

bool XmlReportDocument::load(const QString &file_name)
{
	m_fileName = resolveFileName(file_name, {});
	if(!loadDomDocument(m_fileName, m_document))
		return false;
	// report errors early, when report is set to processor
	std::unique_ptr<ReportItemReport> report(createReport({}));
	return report != nullptr;
}

QUrl XmlReportDocument::url() const
{
	if(m_fileName.startsWith(':'))
		return QUrl("qrc" + m_fileName);
	return QUrl::fromLocalFile(m_fileName);
}

ReportItemReport *XmlReportDocument::createReport(const QVariantMap &init_properties)
{
	Loader loader(m_fileName, init_properties);
	QObject *o = loader.createObject(m_document.documentElement(), nullptr);
	auto *root = qobject_cast<ReportItemReport*>(o);
	if(!root) {
		qfError() << m_fileName << "root element must be Report";
		delete o;
		return nullptr;
	}
	if(loader.isError()) {
		delete root;
		return nullptr;
	}
	// report parameters are available in print time expressions as {{$name}}
	for(const auto &[key, val] : loader.reportParameters().asKeyValueRange())
		root->setProperty(key.toUtf8().constData(), val);
	return root;
}
