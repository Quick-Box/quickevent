#ifndef QF_GUI_REPORTS_REPORTDOCUMENT_H
#define QF_GUI_REPORTS_REPORTDOCUMENT_H

#include "../../guiglobal.h"

#include <QUrl>
#include <QVariantMap>

namespace qf {
namespace gui {
namespace reports {

class ReportItemReport;

/// Report definition, which can instantiate report item tree.
class QFGUI_DECL_EXPORT ReportDocument
{
public:
	virtual ~ReportDocument() = default;

	/// Report location, relative paths in report are resolved against it.
	virtual QUrl url() const = 0;
	/// Creates new report item tree, caller takes ownership.
	/// Report root properties are set to \a init_properties values.
	/// Returns nullptr on error.
	virtual ReportItemReport* createReport(const QVariantMap &init_properties) = 0;
};

}}}

#endif // QF_GUI_REPORTS_REPORTDOCUMENT_H
