//
// Author: Frantisek Vacek <fanda.vacek@volny.cz>, (C) 2006, 2014
//
// Copyright: See COPYING file that comes with this distribution
//

#ifndef QF_GUI_REPORTS_REPORTITEMPARA_H
#define QF_GUI_REPORTS_REPORTITEMPARA_H

#include "reportitemframe.h"
#include "../../guiglobal.h"

#include <functional>

namespace qf {
namespace gui {
namespace reports {

class QFGUI_DECL_EXPORT ReportItemPara : public ReportItemFrame
{
	Q_OBJECT

	Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
	Q_PROPERTY(bool omitEmptyText READ isOmitEmptyText WRITE setOmitEmptyText NOTIFY omitEmptyTextChanged)
	Q_PROPERTY(QString sqlId READ sqlId WRITE setSqlId NOTIFY sqlIdChanged)
	Q_PROPERTY(HAlignment textHAlign READ textHAlign WRITE setTextHAlign NOTIFY textHAlignChanged)
	Q_PROPERTY(VAlignment textVAlign READ textVAlign WRITE setTextVAlign NOTIFY textVAlignChanged)
	Q_PROPERTY(bool textWrap READ isTextWrap WRITE setTextWrap NOTIFY textWrapChanged)
private:
	typedef ReportItemFrame Super;

	QF_PROPERTY_IMPL(QString, t, T, ext)
	QF_PROPERTY_BOOL_IMPL2(o, O, mitEmptyText, true)
	QF_PROPERTY_IMPL(QString, s, S, qlId)
	QF_PROPERTY_IMPL2(HAlignment, t, T, extHAlign, AlignLeft)
	QF_PROPERTY_IMPL2(VAlignment, t, T, extVAlign, AlignTop)
	QF_PROPERTY_BOOL_IMPL2(t, T, extWrap, true)
public:
	ReportItemPara(ReportItem *parent = nullptr);
	virtual ~ReportItemPara() {}

	using GetTextFunction = std::function<QString ()>;
	/// when set, it is used instead of text property
	void setGetTextFunction(const GetTextFunction &fn) {m_getTextFn = fn;}
protected:
	/// tiskne se printed text od indexToPrint, pouziva se pouze v pripade, ze text pretece na dalsi stranku
	QString printedText;
	QTextLayout textLayout;
protected:
	virtual PrintResult printMetaPaintChildren(QPaintDevice *paint_device, ReportItemMetaPaint *out, const ReportItem::Rect &bounding_rect);
	virtual QString paraText();
public:
	virtual void resetIndexToPrintRecursively(bool including_para_texts);
	virtual PrintResult printMetaPaint(QPaintDevice *paint_device, ReportItemMetaPaint *out, const Rect &bounding_rect);
	virtual PrintResult printHtml(HTMLElement &out);
private:
	GetTextFunction m_getTextFn;
};

}}}

#endif // QF_GUI_REPORTS_REPORTITEMPARA_H
