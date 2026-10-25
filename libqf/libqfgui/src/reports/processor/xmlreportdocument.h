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
/// Element <include src="file.xml" [attr="value" ...]/> includes other report file root element,
/// include element attributes override the included element ones.
///
/// Para text can contain {{expression}} placeholders evaluated during report print:
///   {{field}} value of field from nearest Detail current row or nearest Band data
///   {{$page}} current page number, {{$now}} current date-time
///   {{$name}} or {{$name.key}} report init property, nested maps can be accessed with dot notation
///   {{expression|formatter}} or {{expression|formatter:argument}} formats value with registered formatter,
///   formatters can be chained
class QFGUI_DECL_EXPORT XmlReportDocument : public ReportDocument
{
public:
	bool load(const QString &file_name);

	QUrl url() const override;
	ReportItemReport* createReport(const QVariantMap &init_properties) override;
public:
	using Formatter = std::function<QString (const QVariant &value, const QString &argument)>;
	/// Register formatter usable in Para text placeholders.
	/// Built-in formatters: date, time, datetime (argument is optional QDateTime format string),
	/// number (argument is count of decimal places), default (argument is used for empty value)
	static void registerFormatter(const QString &name, const Formatter &fn);

	/// Evaluate placeholders in \a text in the context of \a item, see class documentation.
	static QString evaluateTemplate(const QString &text, ReportItem *item);
private:
	QString m_fileName;
	QDomDocument m_document;
};

}}}

#endif // QF_GUI_REPORTS_XMLREPORTDOCUMENT_H
