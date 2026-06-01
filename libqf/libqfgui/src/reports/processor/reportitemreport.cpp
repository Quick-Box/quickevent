#include "reportitemreport.h"

#include <qf/core/log.h>

using namespace qf::gui::reports;

//==========================================================
//                   ReportItemReport
//==========================================================
ReportItemReport::ReportItemReport(ReportItem *parent)
	: Super(parent), m_reportProcessor(nullptr)
{
	qfLogFuncFrame();
}

ReportItemReport::~ReportItemReport()
{
	qfLogFuncFrame();
}

ReportItem::PrintResult ReportItemReport::printMetaPaint(QPaintDevice *paint_device, ReportItemMetaPaint *out, const ReportItem::Rect &bounding_rect )
{
	qfLogFuncFrame() << "\x1B[1;31;40m***ROOT***ROOT***ROOT***ROOT***\x1B[0;37;40m" << this;
	Q_UNUSED(bounding_rect);
	//PrintResult res = PrintResult::createPrintFinished();
	//updateChildren();
	//ReportItemMetaPaintPage *pg = new ReportItemMetaPaintPage(out, element, processor()->context());
	//pg->renderedRect = designedRect;
	//indexToPrint = 0; /// vzdy vytiskni header a footer. (footer je absolutni header, umisteny pred detailem)
	PrintResult res = Super::printMetaPaint(paint_device, out, designedRect);
	//res = printMetaPaintChildren(pg, pg->renderedRect);
	qfDebug() << "\t\x1B[1;31;40m<<< ***ROOT***ROOT***ROOT***ROOT***\x1B[0;37;40m";
	//res = checkPrintResult(res);
	return res;
}

