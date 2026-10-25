#ifndef QF_GUI_REPORTS_XMLREPORTDOCUMENT_H
#define QF_GUI_REPORTS_XMLREPORTDOCUMENT_H

#include "reportdocument.h"

#include <QDomDocument>

#include <functional>

namespace qf {
namespace gui {
namespace reports {

class ReportItem;

/// Report defined in XML file.
///
/// Element names are report item and style types (Report, Frame, Band, Detail, Para, Image, Break,
/// StyleSheet, Color, Pen, Brush, Font, TextStyle), attributes are their properties.
/// Enum values are written as enum key names, for example layout="LayoutHorizontal".
/// Style object properties (textStyle, border, fill, font, pen, color, ...) can be written as attribute,
/// value is name of style object from report style sheet, which the new object is based on.
/// Full object can be also defined in property element, for example <bottomBorder><Pen .../></bottomBorder>.
/// Empty style object attribute value sets no style object, it is useful in print time expressions.
///
/// Special elements:
///   <Parameter name="n" default="value"/> defines report parameter n, if it is not passed to report.
///     Report parameters are report init properties passed to ReportProcessor::setReport().
///   <include src="file.xml" [attr="value" ...]/> includes other report file root element.
///     Attributes, which are properties of included root element, override them, others are passed
///     as parameters to the included file.
///   <Repeat var="i" from="1" to="3"> creates its child elements repeatedly with parameter i set
///     to from, from + 1, ..., to.
///
/// Expressions:
///   ${expression} is evaluated when report is loaded, it can be used in any attribute or text.
///   {{expression}} is evaluated during report print, it can be used in Para text and report item properties,
///     for example textStyle="{{dropped|if:strikeOut:bold}}"
/// expression is value followed by optional formatters: value|formatter|formatter:argument
/// formatter argument starting with $ is report parameter value, for example add:$width
/// value can be:
///   'text' string literal, 123 number literal
///   name(arg, ...) function call, arguments are expressions
///     built-in functions: select(condition, then, else), coalesce(value, ...) first non empty value,
///     add(a, ...), sub(a, b), mul(a, ...), div(a, b), and(a, ...), or(a, ...), not(a), eq(a, b), lt(a, b), gt(a, b),
///     band('name') value from nearest Band data, it can be used to get band data in Detail
///     unknown function in {{}} is taken as field name, data can contain aggregated fields like SUM(field)
///   $name or $name.key report parameter, nested maps are accessed with dot notation, $ is optional in ${}
///   field  value of field from nearest Detail current row or nearest Band data, in {{}} only
///   $page, $now  current page number and current date-time, in {{}} only
///   $row, $rowCount, $lastRow  current row index, row count and if current row is the last one of nearest Detail, in {{}} only
/// Built-in formatters:
///   tr  translates value, translation context is report file base name
///   trn:text  translates text with plural forms, value is count
///   date, time, datetime  argument is optional QDateTime format string
///   number:decimals, add:number, clamp:min:max
///   default:text  text is used for empty value
///   if:then:else  then or else text is used depending on the value
///   not, nonzero (0 to empty), positive (not positive number to empty)
///   prefix:text, suffix:text  added to non empty value only
///   key:name  value of key in map
///   arg:value  replaces %1 in text by value
///   changed  value if it differs from value printed for previous Detail row, empty otherwise
class QFGUI_DECL_EXPORT XmlReportDocument : public ReportDocument
{
public:
	class QFGUI_DECL_EXPORT ExpressionContext
	{
	public:
		ExpressionContext(ReportItem *item, const QList<QVariantMap> *load_parameters = nullptr)
			: m_item(item), m_loadParameters(load_parameters) {}

		/// report parameter, dot notation is used for nested maps
		QVariant parameter(const QString &path) const;
		/// field value from nearest Detail current row or nearest Band data, available during print only
		QVariant field(const QString &name) const;
		/// report item, which is printed, nullptr when report is loaded
		ReportItem* item() const { return m_item; }
		bool isLoading() const { return m_loadParameters != nullptr; }
	private:
		ReportItem *m_item;
		const QList<QVariantMap> *m_loadParameters;
	};
public:
	bool load(const QString &file_name);

	QUrl url() const override;
	ReportItemReport* createReport(const QVariantMap &init_properties) override;
public:
	/// formatter with no argument gets null argument
	using Formatter = std::function<QVariant (const QVariant &value, const QString &argument)>;
	static void registerFormatter(const QString &name, const Formatter &fn);

	using Function = std::function<QVariant (const ExpressionContext &context, const QVariantList &args)>;
	static void registerFunction(const QString &name, const Function &fn);
private:
	QString m_fileName;
	QDomDocument m_document;
};

}}}

#endif // QF_GUI_REPORTS_XMLREPORTDOCUMENT_H
