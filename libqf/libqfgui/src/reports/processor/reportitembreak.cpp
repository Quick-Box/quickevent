#include "reportitembreak.h"

#include <qf/core/log.h>



namespace qf::gui::reports {

ReportItemBreak::ReportItemBreak(ReportItem *parent)
	: Super(parent)
{
	/// attribut type (page | column) zatim nedela nic
	//QF_ASSERT_EX(proc, "processor is NULL", return);
	designedRect.verticalUnit = Rect::UnitInvalid;
	//qfInfo() << element.attribute("id");
}

void ReportItemBreak::setSkipFirst(bool is_set)
{
	m_skipFirst = is_set;
	m_breaking = is_set;
}

ReportItem::PrintResult ReportItemBreak::printMetaPaint(QPaintDevice *paint_device, ReportItemMetaPaint *out, const ReportItem::Rect &bounding_rect )
{
	qfLogFuncFrame() << "is breaking:" << m_breaking;
	Q_UNUSED(paint_device);
	Q_UNUSED(bounding_rect);
	Q_UNUSED(out);
	if(!isVisible()) {
		return PrintResult::createPrintFinished();
	}
	PrintResult ret = PrintResult::createPrintFinished();
	if(!m_breaking) {
		ret = PrintResult::createPrintAgain();
		if(breakType() == BreakType::Page)
			ret.setPageBreak(true);
		else if(breakType() == BreakType::Column)
			ret.setColumnBreak(true);
	}
	m_breaking = !m_breaking;
	return ret;
}

} // namespace qf::gui::reports



