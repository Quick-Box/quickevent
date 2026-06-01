//
// Author: Frantisek Vacek <fanda.vacek@volny.cz>, (C) 2006
//
// Copyright: See COPYING file that comes with this distribution
//

#include "reportpainter.h"
#include "reportprocessor.h"
#include "reportitemband.h"
#include "reportitemreport.h"
#include "style/compiledtextstyle.h"
//#include "../../graphics/graph/graph.h"

#include <qf/core/log.h>
#include <qf/core/assert.h>
#include <qf/core/string.h>
#include <qf/core/utils/fileutils.h>

#include <QDomElement>
#include <QDate>
#include <QStringBuilder>
#include <QUrl>


using namespace qf::gui::reports;

//==========================================================
//           ReportItem
//==========================================================
const double ReportItem::Epsilon = 1e-10;
const QString ReportItem::INFO_IF_NOT_FOUND_DEFAULT_VALUE = "$INFO";

ReportItem::ReportItem(ReportItem *parent)
	: Super(parent) //--, processor(proc), element(el)
{
	m_keepAll = false;
	m_visible = true;
	m_recentPrintNotFinished = false;
}

ReportItem::~ReportItem() = default;

bool ReportItem::isVisible()
{
	bool ret = processor()->isDesignMode() || m_visible;
	return ret;
}

ReportItemBand* ReportItem::parentBand()
{
	auto *ret = qf::core::Utils::findParent<ReportItemBand*>(this, false);
	return ret;
}

ReportItem::PrintResult ReportItem::checkPrintResult(ReportItem::PrintResult res)
{
	PrintResult ret = res;
	if(!canBreak() && m_recentPrintNotFinished && !res.isPrintFinished()) {
		//qfWarning().noSpace() << "PrintNeverFit element: '" << element.tagName() << "' id: '" << element.attribute("id") << "'";
		ret = PrintResult::createPrintError();
	}
	m_recentPrintNotFinished = !res.isPrintFinished();
	return ret;
}

ReportProcessor *ReportItem::processor(bool throw_exc)
{
	ReportProcessor *ret = nullptr;
	QObject *it = this;
	while(it) {
		auto *rir = qobject_cast<ReportItemReport*>(it);
		if(rir) {
			ret = rir->reportProcessor();
			break;
		}
		it = it->QObject::parent();
	}
	if(ret == nullptr && throw_exc) {
		QF_EXCEPTION("ReportItem without ReportProcessor");
	}
	return ret;
}

ReportItem::PrintResult ReportItem::printMetaPaint(QPaintDevice *paint_device, ReportItemMetaPaint *out, const Rect &bounding_rect)
{
	Q_UNUSED(paint_device);
	Q_UNUSED(out);
	Q_UNUSED(bounding_rect);
	return PrintResult::createPrintFinished();
}

ReportItem::PrintResult ReportItem::printHtml(ReportItem::HTMLElement &out)
{
	if(out.isNull()) {
		return PrintResult::createPrintError();
	}
	return PrintResult::createPrintFinished();
}

void ReportItem::createHtmlExportAttributes(ReportItem::HTMLElement &out) const
{
	QMapIterator<QString, QVariant> it(htmlExportAttributes());
	while(it.hasNext()) {
		it.next();
		out.setAttribute(it.key(), it.value().toString());
	}
}

void ReportItem::classBegin()
{
	qfLogFuncFrame();
}

void ReportItem::componentComplete()
{
	qfLogFuncFrame();
}

QString ReportItem::toString(int indent, int indent_offset)
{
	Q_UNUSED(indent);
	QString ret;
	QString indent_str;
	indent_str.fill(' ', indent_offset);
	ret += indent_str + metaObject()->className();
	return ret;
}

style::Text *ReportItem::effectiveTextStyle()
{
	style::Text *ret = nullptr;
	ReportItem *it = this;
	while(it) {
		auto *frit = qobject_cast<ReportItemFrame*>(it);
		if(frit) {
			ret = frit->textStyle();
			if(ret)
				break;
		}
		it = it->parent();
	}
	QF_ASSERT(ret != nullptr, "Cannot find TextStyle definition in parents", return ret);
	return ret;
}

#ifdef REPORT_ITEM_TABLE
//==========================================================
//                                    ReportItemTable
//==========================================================
ReportItemTable::ReportItemTable(ReportItem *parent)
	: ReportItemBand(parent)
{
	//qfDebug() << QF_FUNC_NAME << "parent:" << parent();
	//QF_ASSERT(!!_el, "element is null.");
}

void ReportItemTable::syncChildren()
{
	qfLogFuncFrame() << this;
	deleteChildren();
	for(QDomElement el = fakeBand.firstChildElement(); !!el; el = el.nextSiblingElement()) {
		if(!ReportProcessor::isProcessible(el)) continue;
		/// vytvor chybejici item
		processor()->createProcessibleItem(el, this);
	}
	qfDebug() << QF_FUNC_NAME << "<<<<<<<<<<<<<<<< OUT";
}

void ReportItemTable::createFakeBand()
{
	//return;
	if(!!fakeBand) return;

	bool from_data = element.attribute("createfromdata").toBool();
	QString decoration = element.attribute("decoration");
	bool grid = (decoration == "grid");
	bool horizontal_lines = (decoration == "horizontallines");
	bool line = !decoration.isEmpty();
	QString line_pen = (grid)? "black1": "black1";
	bool shadow = element.attribute("shadow", "1").toBool();

	fakeBand = fakeBandDocument.createElement("band");
	fakeBandDocument.appendChild(fakeBand);
	fakeBand.setAttribute("__fake", 1); /// napomaha pri selekci v report editoru

	QDomElement el_header, el_detail, el_footer;
	{
		QDomElement el, el1;

		el1 = fakeBand.ownerDocument().createElement("row");
		el = element.firstChildElement("headerframe");
		el1.copyAttributesFrom(el);
		/// v tabulce ma smysl mit deti zarovnany
		if(grid && el1.attribute("expandChildFrames").isEmpty()) el1.setAttribute("expandChildFrames", "1");
		if(line && el1.attribute("bbrd").isEmpty()) el1.setAttribute("bbrd", line_pen);
		else if(horizontal_lines) el1.setAttribute("bbrd", line_pen);
		if(shadow && el1.attribute("fill").isEmpty()) el1.setAttribute("fill", "tblshadow");
		el_header = el1;
		el_header.setAttribute("__fakeBandHeaderRow", 1); /// napomaha exportu do HTML

		el1 = fakeBand.ownerDocument().createElement("detail");
		el = element.firstChildElement("detailframe");
		el1.copyAttributesFrom(el);
		if(grid && el1.attribute("expandChildFrames").isEmpty()) el1.setAttribute("expandChildFrames", "1");
		if(el1.attribute("keepall").isEmpty()) el1.setAttribute("keepall", "1");
		if(horizontal_lines) el1.setAttribute("bbrd", line_pen);
		el_detail = el1;
		el_detail.setAttribute("__fakeBandDetail", 1); /// napomaha exportu do HTML

		el1 = fakeBand.ownerDocument().createElement("row");
		el = element.firstChildElement("footerframe");
		el1.copyAttributesFrom(el);
		if(grid && el1.attribute("expandChildFrames").isEmpty()) el1.setAttribute("expandChildFrames", "1");
		if(line) el1.setAttribute("tbrd", line_pen);
		else if(horizontal_lines) el1.setAttribute("bbrd", line_pen);
		if(shadow && el1.attribute("fill").isEmpty()) el1.setAttribute("fill", "tblshadow");
		el_footer = el1;
		el_footer.setAttribute("__fakeBandFooterRow", 1); /// napomaha exportu do HTML
	}

	bool has_footer = false;
	if(from_data) {
		qfDebug() << "\tcreating from data";
		qfu::TreeTable t = dataTable();
		//foreach(const QFXmlTableColumnDef cd, t.columns()) if(!t.columnFooter(cd.name).isEmpty()) {has_footer = true; break;}
		qfu::TreeTableColumns cols = t.columns();
		for(int i=0; i<cols.count(); i++) {
			qfu::TreeTableColumn cd = cols.column(i);
			QString col_w = cd.width();
			if(col_w.isEmpty()) col_w = "%";
			{
				QDomElement el = fakeBand.ownerDocument().createElement("para");
				el.setAttribute("w", col_w);
				el.setAttribute("hinset", "1");
				el.setAttribute("style", "tblheading");
				el.setAttribute("halign", "center");
				if(grid) el.setAttribute("brd", "black1");
				//QDomText txt = fakeBand.ownerDocument().createTextNode(t.columnHeader(cd.name));
				//el.appendChild(txt);
				QDomElement el1 = fakeBand.ownerDocument().createElement("data");
				el1.setAttribute("src", "HEADER(" + cd.name() + ')');
				el1.setAttribute("domain", "table");
				el.appendChild(el1);
				el_header.appendChild(el);
			}
			{
				QDomElement el = fakeBand.ownerDocument().createElement("para");
				el.setAttribute("w", col_w);
				el.setAttribute("hinset", "1");
				el.setAttribute("style", "tbltext");
				el.setAttribute("halign", cd.hAlignment());
				if(grid) el.setAttribute("brd", "black1");
				QDomElement el1 = fakeBand.ownerDocument().createElement("data");
				el1.setAttribute("src", cd.name());
				el.appendChild(el1);
				el_detail.appendChild(el);
			}
			{
				QString footer = t.columnFooter(cd.name());
				if(!footer.isEmpty()) has_footer = true;
				QDomElement el = fakeBand.ownerDocument().createElement("para");
				el.setAttribute("w", col_w);
				el.setAttribute("hinset", "1");
				el.setAttribute("style", "tbltextB");
				el.setAttribute("halign", cd.hAlignment());
				if(grid) el.setAttribute("brd", "black1");
				//QDomText txt = fakeBand.ownerDocument().createTextNode(footer);
				//el.appendChild(txt);
				if(has_footer) {
					QDomElement el1 = fakeBand.ownerDocument().createElement("data");
					el1.setAttribute("src", "FOOTER(" + cd.name() + ')');
					el1.setAttribute("domain", "table");
					el.appendChild(el1);
				}
				el_footer.appendChild(el);
			}
		}
	}
	else {
		QDomElement el = element.firstChildElement("cols");
		int col_no = 0;
		for(el=el.firstChildElement("col"); !!el; el=el.nextSiblingElement("col")) {
			QString fake_path = QString("cols/col[%1]").arg(col_no);
			QDomElement el1;
			el1 = el.firstChildElement("colheader");
			if(!!el1) {
				el1 = el1.cloneNode().toElement();
				el1.setTagName("cell");
				el1.setAttribute("__fakePath", fake_path + "/colheader");
			}
			else {
				el1 = fakeBand.ownerDocument().createElement("cell");
				el1.setAttribute("__fakePath", fake_path);
			}
			el1.copyAttributesFrom(el);
			if(grid) el1.setAttribute("brd", "black1");
			el_header.appendChild(el1);

			el1 = el.firstChildElement("coldetail");
			if(!!el1) {
				el1 = el1.cloneNode().toElement();
				el1.setTagName("cell");
				if(grid) el1.setAttribute("brd", "black1");
				el1.setAttribute("__fakePath", fake_path + "/coldetail");
			}
			else {
				el1 = fakeBand.ownerDocument().createElement("cell");
				el1.setAttribute("__fakePath", fake_path);
			}
			el1.copyAttributesFrom(el);
			el_detail.appendChild(el1);

			el1 = el.firstChildElement("colfooter");
			if(!!el1) {
				has_footer = true;
				el1 = el1.cloneNode().toElement();
				el1.setTagName("cell");
				if(grid) el1.setAttribute("brd", "black1");
				el1.setAttribute("__fakePath", fake_path + "/colfooter");
			}
			else {
				el1 = fakeBand.ownerDocument().createElement("cell");
				el1.setAttribute("__fakePath", fake_path);
			}
			el1.copyAttributesFrom(el);
			el_footer.appendChild(el1);
			col_no++;
		}
	}

	QDomElement el_columns;
	for(QDomElement el=element.firstChildElement(); !!el; el=el.nextSiblingElement()) {
		if(el.tagName() == "cols") {
			el_columns = el;
			break;
		}
		QDomElement el1 = el.cloneNode().toElement();
		fakeBand.appendChild(el1);
	}
	fakeBand.appendChild(el_header);
	fakeBand.appendChild(el_detail);
	if(has_footer) fakeBand.appendChild(el_footer);
	for(QDomElement el=el_columns.nextSiblingElement(); !!el; el=el.nextSiblingElement()) {
		QDomElement el1 = el.cloneNode().toElement();
		fakeBand.appendChild(el1);
	}
	//qfInfo() << fakeBand.toString();
}

ReportItem::PrintResult ReportItemTable::printMetaPaint(QPaintDevice *paint_device, ReportItemMetaPaint *out, const ReportItem::Rect &bounding_rect)
{
	createFakeBand();
	return ReportItemBand::printMetaPaint(paint_device, out, bounding_rect);
}
#endif

//===============================================================
//                ReportItemGraph
//===============================================================
#if 0
void ReportItemGraph::syncChildren()
{
	qfDebug() << QF_FUNC_NAME << element.tagName() << "id:" << element.attribute("id");
	static int graph_no = 0;
	src = QString("key:/graph-%1").arg(++graph_no);

	childrenSyncedFlag = true;
	ReportItem::syncChildren();
}
#endif

#ifdef REPORT_ITEM_GRAPH
ReportItemImage::PrintResult ReportItemGraph::printMetaPaintChildren(QPaintDevice *paint_device, ReportItemMetaPaint *out, const ReportItem::Rect &bounding_rect)
{
	qfLogFuncFrame() << this;
	PrintResult res = PrintOk;
	Rect br = bounding_rect;

	/// vykresli graf a pridej ho do processor()->images()
	QPicture pict;
	QVariantMap definition;
	qfError() << "Graphs are not fully implemented yet.";
	graphics::Graph *graph = graphics::Graph::createGraph(definition, findDataTable(dataSource()));
	if(graph) {
		graph->setStyleCache(processor()->context().styleCache());
		QPainter painter;
		painter.begin(&pict);
		graph->draw(&painter, br.size());
		painter.end();

		/// nastav velikost grafu v pixelech na bounding rect, bez ohledu na to, co vymysli QPainter
		/// nevim proc, ale u grafu si mysli, ze pokreslil jenom gridrect
		/// vypada to, jako ze tisk textu se do boundingrect nepocita

		/// jako zaklad vem rect, ktery je predepsan v reportu, do nej by se mel graf vejit
		QRect r = gui::graphics::mm2device(br, &pict).toRect();
		/// bounding rect obrazku grafu je vzdy od [0,0]
		r.moveTo(0, 0);
		//qfInfo() << "graph bounding_rect:" << Rect(r).toString();
		//qfInfo() << "pict bounding_rect:" << Rect(pict.boundingRect()).toString();
		/// muze se stat, ze treba data jsou tak rozsahla, ze presahnou ramecek grafu
		/// v takovem pripade zvets popsany ctverec o tyto pripadne presahy
		/// vysledkem teto akce je, ze graf se vzdy vejde do predepsaqneho ramecku, bez ohledu na to, jestli ho pri vykreslovani nekde nepretah
		/// pokud ho pretahne, skutecne popsana plocha se o toto pretazeni zvetsi
		/// pokud bych zakomentoval nasledujici radek, sloupce caroveho grafu by pro urcite kombinace vykreslovanych dat mohly vylezt nad region urceny pro graf
		/// (napr. v pripade, ze je vice serii a osy se nastavi podle serie s mensi max. hodnotou nez maji ostatni serie (plan obchodu - sestava plneni planu))
		r = r.united(pict.boundingRect());
		pict.setBoundingRect(r);
		//qfInfo() << "pict united rect:" << Rect(pict.boundingRect()).toString();
		processor()->addImage(m_resolvedDataSource, ReportItem::Image(pict));
		delete graph;
	}
	//qfInfo() << "physicalDpiX:" << pict.physicalDpiX();
	//qfInfo() << "logicalDpiX:" << pict.logicalDpiX();

	res = ReportItemImage::printMetaPaintChildren(out, br);
	return res;
}
#endif
