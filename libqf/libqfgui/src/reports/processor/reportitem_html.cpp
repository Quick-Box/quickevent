//
// C++ Implementation: Reportitem_html
//
// Description:
//
//
// Author: Fanda Vacek <fanda@JedovaChyse>, (C) 2008
//
// Copyright: See COPYING file that comes with this distribution
//
//
#include "reportitemband.h"
#include "reportitemdetail.h"
#include "banddatamodel.h"
#include "reportitempara.h"
#include "reportprocessor.h"
#include "reportpainter.h"

#include <qf/core/log.h>

#include <QDomElement>
#include <QRegularExpression>

using namespace qf::gui::reports;

namespace {
QDomText setElementText(QDomElement &el, const QString &str)
{
	QDomNode nd = el.firstChild();
	QDomText eltxt = nd.toText();
	if(eltxt.isNull()) {
		eltxt = el.ownerDocument().createTextNode(str);
		el.insertBefore(eltxt, nd);
	}
	else {
		eltxt.setData(str);
	}
	return eltxt;
}
}
//===================================================================
//                           ReportItemFrame
//===================================================================
ReportItem::PrintResult ReportItemFrame::printHtml(HTMLElement & out)
{
	qfLogFuncFrame();
	updatePrintBindings();
	PrintResult res = Super::printHtml(out);
	if(!isVisible())
		return res;
	if(!res.isPrintFinished())
		return res;

	qfDebug() << "\tparent html element:" << out.tagName();
	if(itemsToPrintCount() > 0) {
		if(false && itemsToPrintCount() == 1) {
			/// jedno dite vyres tak, ze se vubec nevytiskne rodicovsky frame
			/// does not work for bands, they cannot be converted to table than
			ReportItem *it = itemToPrintAt(0);
			res = it->printHtml(out);
		}
		else {
			QDomElement el_div = out.ownerDocument().createElement("div");
			createHtmlExportAttributes(el_div);
			if(layout() == LayoutHorizontal) {
				el_div.setAttribute(ReportProcessor::HTML_ATTRIBUTE_LAYOUT, QStringLiteral("horizontal"));
			}
			for(int i=0; i<itemsToPrintCount(); i++) {
				ReportItem *it = itemToPrintAt(i);
				PrintResult ch_res;
				do {
					ch_res = it->printHtml(el_div);
				} while(ch_res.isPrintFinished() && ch_res.isNextDetailRowExists());
				res = ch_res;
			}
			out.appendChild(el_div);
		}
	}
	return res;
}

//===================================================================
//                           ReportItemBand
//===================================================================

ReportItem::PrintResult ReportItemBand::printHtml(ReportItem::HTMLElement &out)
{
	qfLogFuncFrame() << this;
	PrintResult res = Super::printHtml(out);
	if(res.isPrintFinished()) {
		if(isHtmlExportAsTable()) {
			QDomElement el_band = out.lastChild().toElement();
			el_band.setAttribute(ReportProcessor::HTML_ATTRIBUTE_ITEM, QStringLiteral("band"));
		}
	}
	return res;
}

//===================================================================
//                           ReportItemDetail
//===================================================================
ReportItem::PrintResult ReportItemDetail::printHtml(HTMLElement & out)
{
	qfLogFuncFrame() << "current index:" << currentIndex();
	auto *band = qobject_cast<ReportItemBand*>(parent());
	BandDataModel *model = nullptr;
	if(band) {
		model = band->model();
		if(model) {
			if(currentIndex() < 0) {
				setCurrentIndex(0);
			}
		}
	}
	PrintResult res;
	res = Super::printHtml(out);
	if(res.isPrintFinished()) {
		{
			QDomElement el = out.lastChild().toElement();
			createHtmlExportAttributes(el);
			el.setAttribute(ReportProcessor::HTML_ATTRIBUTE_ITEM, QStringLiteral("detail"));
		}
		if(model) {
			/// take next data row
			int ix = currentIndex() + 1;
			if(ix < model->rowCount()) {
				setCurrentIndex(ix);
				resetIndexToPrintRecursively(ReportItem::IncludingParaTexts);
				res.setNextDetailRowExists(true);
			}
			else {
				resetCurrentIndex();
			}
		}
	}
	return res;
}

//===================================================================
//                           ReportItemPara
//===================================================================
ReportItem::PrintResult ReportItemPara::printHtml(HTMLElement & out)
{
	qfLogFuncFrame();// << element.tagName() << "id:" << element.attribute("id");
	PrintResult res = Super::printHtml(out);
	if(!res.isPrintFinished())
		return res;

	QDomElement el_div = out.ownerDocument().createElement("div");
	QDomElement el_p = out.ownerDocument().createElement("p");
	QString text = paraText();
	auto rx = ReportItemMetaPaint::checkReportSubstitutionRegExp;
	if(auto match = rx.match(text); match.hasMatch()) {
		bool check_on = match.capturedTexts().value(1) == "1";
		text = (check_on)? "X": QString();
	}
	setElementText(el_p, text);
	out.appendChild(el_div);
	el_div.appendChild(el_p);
	createHtmlExportAttributes(el_p);
	return res;
}
