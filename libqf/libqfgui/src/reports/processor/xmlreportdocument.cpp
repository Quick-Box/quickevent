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

#include <QDateTime>
#include <QDir>
#include <memory>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMetaProperty>
#include <QRegularExpression>

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
const auto SRC_ATTR = QStringLiteral("src");

//==========================================================
// text templates
//==========================================================
struct TemplateSegment
{
	QString literal;
	QString expression;
	QStringList formatters;
};
using Template = QList<TemplateSegment>;

Template compileTemplate(const QString &text)
{
	static const QRegularExpression rx(QStringLiteral(R"(\{\{(.*?)\}\})"));
	Template ret;
	qsizetype pos = 0;
	auto it = rx.globalMatch(text);
	while(it.hasNext()) {
		auto m = it.next();
		if(m.capturedStart() > pos)
			ret << TemplateSegment{text.mid(pos, m.capturedStart() - pos), {}, {}};
		auto parts = m.captured(1).split('|');
		TemplateSegment seg;
		seg.expression = parts.takeFirst().trimmed();
		for(const auto &f : parts)
			seg.formatters << f.trimmed();
		ret << seg;
		pos = m.capturedEnd();
	}
	if(pos < text.length())
		ret << TemplateSegment{text.mid(pos), {}, {}};
	return ret;
}

bool isTemplate(const QString &text)
{
	return text.contains(QLatin1String("{{"));
}

QMap<QString, XmlReportDocument::Formatter>& formatters()
{
	static QMap<QString, XmlReportDocument::Formatter> fmts {
		{"date", [](const QVariant &v, const QString &arg) {
			auto d = v.toDate();
			return arg.isEmpty()? d.toString(Qt::ISODate): d.toString(arg);
		}},
		{"time", [](const QVariant &v, const QString &arg) {
			auto t = v.toTime();
			return arg.isEmpty()? t.toString(Qt::ISODate): t.toString(arg);
		}},
		{"datetime", [](const QVariant &v, const QString &arg) {
			auto dt = v.toDateTime();
			return arg.isEmpty()? dt.toString(Qt::TextDate): dt.toString(arg);
		}},
		{"number", [](const QVariant &v, const QString &arg) {
			bool ok;
			auto d = v.toDouble(&ok);
			if(!ok)
				return QString();
			return QString::number(d, 'f', arg.isEmpty()? 0: arg.toInt());
		}},
		{"default", [](const QVariant &v, const QString &arg) {
			auto s = v.toString();
			return s.isEmpty()? arg: s;
		}},
	};
	return fmts;
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

QVariant evaluateExpression(const QString &expression, ReportItem *item)
{
	if(expression.startsWith('$')) {
		auto path = expression.mid(1).split('.');
		auto name = path.takeFirst();
		if(name == QLatin1String("page")) {
			auto *proc = item->processor(!qf::core::Exception::Throw);
			return proc? proc->processedPageNo() + 1: QVariant();
		}
		if(name == QLatin1String("now")) {
			return QDateTime::currentDateTime();
		}
		auto *root = qf::core::Utils::findParent<ReportItemReport*>(item, false);
		if(!root) {
			qfWarning() << "Cannot find report root for expression:" << expression;
			return {};
		}
		QVariant v = root->property(name.toUtf8().constData());
		for(const auto &key : path)
			v = v.toMap().value(key);
		return v;
	}
	for(QObject *o = item; o; o = o->parent()) {
		if(auto *detail = qobject_cast<ReportItemDetail*>(o))
			return detail->rowData(expression);
		if(auto *band = qobject_cast<ReportItemBand*>(o))
			return band->data(expression);
	}
	qfWarning() << "Cannot find Detail or Band to get data for expression:" << expression;
	return {};
}

QString evaluateCompiledTemplate(const Template &tmpl, ReportItem *item)
{
	QString ret;
	for(const auto &seg : tmpl) {
		if(seg.expression.isEmpty()) {
			ret += seg.literal;
			continue;
		}
		QVariant v = evaluateExpression(seg.expression, item);
		if(seg.formatters.isEmpty()) {
			ret += valueToString(v);
			continue;
		}
		for(const auto &fmt : seg.formatters) {
			auto name = fmt.section(':', 0, 0).trimmed();
			auto arg = fmt.section(':', 1);
			auto it = formatters().constFind(name);
			if(it == formatters().constEnd()) {
				qfWarning() << "Unknown formatter:" << name;
				v = valueToString(v);
				continue;
			}
			v = it.value()(v, arg);
		}
		ret += v.toString();
	}
	return ret;
}

//==========================================================
// loader
//==========================================================
class Loader
{
public:
	explicit Loader(const QString &file_name) { m_fileNames << file_name; }

	QObject* createObject(const QDomElement &el, QObject *parent);
	bool isError() const { return m_isError; }
private:
	QObject* createIncluded(const QDomElement &el, QObject *parent);
	void addChildObject(QObject *obj, QObject *child, const QDomElement &child_el);
	void setPropertyElement(QObject *obj, const QDomElement &el);
	void setProperty(QObject *obj, const QString &name, const QString &value);
	void setText(ReportItemPara *para, const QString &text);
	void writePointerProperty(QObject *obj, const QMetaProperty &prop, QObject *value);

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
	bool m_isError = false;
};

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
	m_fileNames << file_name;
	QObject *obj = createObject(doc.documentElement(), parent);
	m_fileNames.removeLast();
	if(obj) {
		auto attrs = el.attributes();
		for(int i = 0; i < attrs.count(); i++) {
			auto attr = attrs.item(i).toAttr();
			if(attr.name() != SRC_ATTR)
				setProperty(obj, attr.name(), attr.value());
		}
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
	QObject *obj = it->create(parent);
	auto attrs = el.attributes();
	for(int i = 0; i < attrs.count(); i++) {
		auto attr = attrs.item(i).toAttr();
		setProperty(obj, attr.name(), attr.value());
	}
	QString text;
	for(auto nd = el.firstChild(); !nd.isNull(); nd = nd.nextSibling()) {
		if(nd.isText() || nd.isCDATASection()) {
			text += nd.toCharacterData().data();
			continue;
		}
		auto child_el = nd.toElement();
		if(child_el.isNull())
			continue;
		if(child_el.tagName() == INCLUDE_TAG || objectTypes().contains(child_el.tagName())) {
			QObject *child = createObject(child_el, obj);
			if(child)
				addChildObject(obj, child, child_el);
		}
		else {
			setPropertyElement(obj, child_el);
		}
	}
	text = text.trimmed();
	if(!text.isEmpty()) {
		if(auto *para = qobject_cast<ReportItemPara*>(obj))
			setText(para, text);
		else
			error("Only Para can contain text " + elementPath(el));
	}
	return obj;
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
	const QMetaObject *prop_mo = prop.metaType().metaObject();
	if(!(prop.metaType().flags() & QMetaType::PointerToQObject) || !prop_mo || !value->metaObject()->inherits(prop_mo)) {
		error(QStringLiteral("Cannot assign %1 to property %2").arg(value->metaObject()->className(), prop.name()));
		delete value;
		return;
	}
	if(!value->parent())
		value->setParent(obj);
	if(!prop.write(obj, QVariant(prop.metaType(), &value)))
		error(QStringLiteral("Cannot write property %1").arg(prop.name()));
}

void Loader::setText(ReportItemPara *para, const QString &text)
{
	if(isTemplate(text)) {
		auto tmpl = compileTemplate(text);
		para->setGetTextFunction([para, tmpl]() { return evaluateCompiledTemplate(tmpl, para); });
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
	QVariant v;
	if(prop.isEnumType()) {
		QMetaEnum me = prop.enumerator();
		bool ok;
		int n = prop.isFlagType()
				? me.keysToValue(value.toLatin1().constData(), &ok)
				: me.keyToValue(value.section('.', -1).toLatin1().constData(), &ok);
		if(!ok) {
			error(QStringLiteral("Invalid value %1 of enum property %2").arg(value, name));
			return;
		}
		v = n;
	}
	else if(prop.metaType().flags() & QMetaType::PointerToQObject) {
		// style object shorthand, new object based on named one
		const ObjectType *type = objectTypeForMetaObject(prop.metaType().metaObject());
		if(!type) {
			error(QStringLiteral("Property %1 cannot be set from attribute").arg(name));
			return;
		}
		QObject *o = type->create(obj);
		o->setProperty(qobject_cast<style::Color*>(o)? "def": "basedOn", value);
		writePointerProperty(obj, prop, o);
		return;
	}
	else if(prop.metaType().id() == QMetaType::QVariantMap) {
		QJsonParseError err;
		auto doc = QJsonDocument::fromJson(value.toUtf8(), &err);
		if(err.error != QJsonParseError::NoError) {
			error(QStringLiteral("Property %1 JSON parse error: %2").arg(name, err.errorString()));
			return;
		}
		v = doc.toVariant();
	}
	else if(prop.metaType().id() == QMetaType::QVariant) {
		v = value;
	}
	else {
		v = value;
		if(!v.convert(prop.metaType())) {
			error(QStringLiteral("Cannot convert %1 to type of property %2").arg(value, name));
			return;
		}
	}
	if(!prop.write(obj, v))
		error(QStringLiteral("Cannot write property %1").arg(name));
}

}

//==========================================================
// XmlReportDocument
//==========================================================
void XmlReportDocument::registerFormatter(const QString &name, const Formatter &fn)
{
	formatters()[name] = fn;
}

QString XmlReportDocument::evaluateTemplate(const QString &text, ReportItem *item)
{
	return evaluateCompiledTemplate(compileTemplate(text), item);
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
	Loader loader(m_fileName);
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
	// report properties are available in text templates as {{$name}}
	for(const auto &[key, val] : init_properties.asKeyValueRange())
		root->setProperty(key.toUtf8().constData(), val);
	return root;
}
