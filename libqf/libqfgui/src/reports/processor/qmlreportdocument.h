#ifndef QF_GUI_REPORTS_QMLREPORTDOCUMENT_H
#define QF_GUI_REPORTS_QMLREPORTDOCUMENT_H

#include "reportdocument.h"

#include <QStringList>

#include <functional>

class QQmlEngine;
class QQmlComponent;

namespace qf {
namespace gui {
namespace reports {

class ReportProcessor;

/// Report defined in QML file, the only part of report processor depending on QML.
class QFGUI_DECL_EXPORT QmlReportDocument : public ReportDocument
{
public:
	explicit QmlReportDocument(ReportProcessor *processor);
	~QmlReportDocument() override;

	bool load(const QString &file_name);

	QUrl url() const override;
	ReportItemReport* createReport(const QVariantMap &init_properties) override;
public:
	/// Every QmlEngine created by QmlReportDocument will have this import paths
	static QStringList qmlEngineImportPaths();
	/// Register a callback invoked on every newly created QQmlEngine.
	/// Use this to inject global JS properties visible to .pragma library modules.
	using QmlEngineInitializer = std::function<void(QQmlEngine *)>;
	static void addQmlEngineInitializer(const QmlEngineInitializer &fn);
private:
	QQmlEngine *m_qmlEngine = nullptr;
	QQmlComponent *m_component = nullptr;
};

}}}

#endif // QF_GUI_REPORTS_QMLREPORTDOCUMENT_H
