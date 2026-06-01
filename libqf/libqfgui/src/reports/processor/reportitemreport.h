//
// Author: Frantisek Vacek <fanda.vacek@volny.cz>, (C) 2006, 2014
//
// Copyright: See COPYING file that comes with this distribution
//

#ifndef QF_GUI_REPORTS_REPORTITEMREPORT_H
#define QF_GUI_REPORTS_REPORTITEMREPORT_H

#include "reportitemband.h"

#include "../../guiglobal.h"

namespace qf {
namespace gui {
namespace reports {


class QFGUI_DECL_EXPORT ReportItemReport : public ReportItemFrame
{
	Q_OBJECT
	Q_PROPERTY(qf::gui::reports::style::Sheet* styleSheet READ styleSheet WRITE setStyleSheet NOTIFY styleSheetChanged)
	Q_PROPERTY(int debugLevel READ debugLevel WRITE setDebugLevel)
	Q_PROPERTY(bool created READ isCreated NOTIFY createdChanged)
private:
	typedef ReportItemFrame Super;
public:
	ReportItemReport(ReportItem *parent = nullptr);
	~ReportItemReport() Q_DECL_OVERRIDE;
public:
	QF_PROPERTY_OBJECT_IMPL(style::Sheet*, s, S, tyleSheet)
	QF_PROPERTY_IMPL2(int, d, D, ebugLevel, 0)
	QF_PROPERTY_BOOL_IMPL(c, C, reated)
public:
	void resetIndexToPrintRecursively(bool including_para_texts) Q_DECL_OVERRIDE
	{
		Super::resetIndexToPrintRecursively(including_para_texts);
	}
	PrintResult printMetaPaint(QPaintDevice *paint_device, ReportItemMetaPaint *out, const Rect &bounding_rect) Q_DECL_OVERRIDE;

	ReportProcessor* reportProcessor() {return m_reportProcessor;}
	void setReportProcessor(ReportProcessor *p) {m_reportProcessor = p;}
private:
	ReportProcessor *m_reportProcessor;
};
#if 0

class QFGUI_DECL_EXPORT ReportItemBody : public ReportItemDetail
{
	Q_OBJECT
private:
	typedef ReportItemDetail Super;
protected:
	/// body a report ma tu vysadu, ze se muze vickrat za sebou nevytisknout a neznamena to print forever.
	//virtual PrintResult checkPrintResult(PrintResult res) {return res;}
public:
	ReportItemBody(ReportItem *parent = nullptr)
		: Super(parent) {}
	virtual ~ReportItemBody() {}

	//virtual PrintResult printMetaPaint(QPaintDevice *paint_device, ReportItemMetaPaint *out, const Rect &bounding_rect);
};
#endif
}}}

#endif // QF_GUI_REPORTS_REPORTITEMREPORT_H
