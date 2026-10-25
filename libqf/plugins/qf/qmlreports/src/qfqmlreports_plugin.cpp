#include "qfqmlreports_plugin.h"
#include "reportitemextensions.h"

#include <qf/gui/reports/processor/reportitempara.h>
#include <qf/gui/reports/processor/reportitemimage.h>
#include <qf/gui/reports/processor/reportitemframe.h>
#include <qf/gui/reports/processor/reportitemdetail.h>
#include <qf/gui/reports/processor/reportitemband.h>
#include <qf/gui/reports/processor/banddatamodel.h>
#include <qf/gui/reports/processor/reportitembreak.h>
#include <qf/gui/reports/processor/reportitemreport.h>
#include <qf/gui/reports/processor/style/sheet.h>
#include <qf/gui/reports/processor/style/color.h>
#include <qf/gui/reports/processor/style/pen.h>

#include <QtQml>

QT_BEGIN_NAMESPACE

void QFQmlReportPlugin::registerTypes(const char *uri)
{
	Q_ASSERT(uri == QLatin1String("qf.qmlreports"));

	qmlRegisterType<qf::gui::reports::ReportItem>(uri, 1, 0, "Item");
	qmlRegisterExtendedType<qf::gui::reports::ReportItemReport, FrameQmlExtension>(uri, 1, 0, "Report");
	qmlRegisterExtendedType<qf::gui::reports::ReportItemFrame, FrameQmlExtension>(uri, 1, 0, "Frame");
	qmlRegisterExtendedType<qf::gui::reports::ReportItemFrame, FrameQmlExtension>(uri, 1, 0, "Space");
	qmlRegisterType<qf::gui::reports::BandDataModel>(uri, 1, 0, "DataModel");
	qmlRegisterExtendedType<qf::gui::reports::ReportItemBand, FrameQmlExtension>(uri, 1, 0, "Band");
	qmlRegisterExtendedType<qf::gui::reports::ReportItemDetail, FrameQmlExtension>(uri, 1, 0, "Detail");
	qmlRegisterExtendedType<qf::gui::reports::ReportItemPara, ParaQmlExtension>(uri, 1, 0, "Para");
	qmlRegisterExtendedType<qf::gui::reports::ReportItemImage, ImageQmlExtension>(uri, 1, 0, "Image");
	qmlRegisterType<qf::gui::reports::ReportItemBreak>(uri, 1, 0, "Break");
	qmlRegisterExtendedType<qf::gui::reports::style::Sheet, SheetQmlExtension>(uri, 1, 0, "StyleSheet");
	qmlRegisterType<qf::gui::reports::style::Color>(uri, 1, 0, "Color");
	qmlRegisterType<qf::gui::reports::style::Pen>(uri, 1, 0, "Pen");
	qmlRegisterType<qf::gui::reports::style::Brush>(uri, 1, 0, "Brush");
	qmlRegisterType<qf::gui::reports::style::Font>(uri, 1, 0, "Font");
	qmlRegisterType<qf::gui::reports::style::Text>(uri, 1, 0, "TextStyle");

	// Auto-increment the import to stay in sync with ALL future QtQuick minor versions
	qmlRegisterModule(uri, 1, 0);
}

QT_END_NAMESPACE
