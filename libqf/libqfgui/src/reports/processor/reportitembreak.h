#ifndef QF_GUI_REPORTS_REPORTITEMBREAK_H
#define QF_GUI_REPORTS_REPORTITEMBREAK_H

#include "reportitem.h"

#include <qf/core/utils.h>

namespace qf {
namespace gui {
namespace reports {

class QFGUI_DECL_EXPORT ReportItemBreak : public ReportItem
{
	Q_OBJECT
	Q_PROPERTY(BreakType breakType READ breakType WRITE setBreakType NOTIFY breakTypeChanged)
	Q_PROPERTY(bool skipFirst READ isSkipFirst WRITE setSkipFirst)
private:
	typedef ReportItem Super;
public:
	ReportItemBreak(ReportItem *parent = nullptr);

	enum BreakType { Column, Page };
	Q_ENUM(BreakType)

	QF_PROPERTY_IMPL2(BreakType, b, B, reakType, Column)

	//bool isBreak() {return true;} Q_DECL_OVERRIDE;
	bool isSkipFirst() const {return m_skipFirst;}
	void setSkipFirst(bool is_set);


	virtual ChildSize childSize(Layout parent_layout) {
		Q_UNUSED(parent_layout);
		return ChildSize(0, Rect::UnitInvalid);
	}
	virtual PrintResult printMetaPaint(QPaintDevice *paint_device, ReportItemMetaPaint *out, const Rect &bounding_rect);
private:
	bool m_breaking = false;
	bool m_skipFirst = false; // skip first page break
};

} // namespace reports
} // namespace gui
} // namespace qf

#endif // QF_GUI_REPORTS_REPORTITEMBREAK_H
