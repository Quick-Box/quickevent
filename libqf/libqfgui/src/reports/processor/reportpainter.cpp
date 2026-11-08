
//
// Author: Frantisek Vacek <fanda.vacek@gmail.com>, (C) 2014
//
// Copyright: See COPYING file that comes with this distribution
//

#include "reportpainter.h"
#include "reportprocessor.h"
#include "style/pen.h"

#include <qf/core/log.h>
#include <qf/core/assert.h>

#include <QJsonDocument>
#include <QRegularExpression>

#include <algorithm>
#include <typeinfo>

using namespace qf::gui::reports;

//=================================================
// ReportItemMetaPaint
//=================================================
const QString ReportItemMetaPaint::pageCountReportSubstitution = "@{n}";
const QString ReportItemMetaPaint::checkReportSubstitution = "@{check:${STATE}}";
const QRegularExpression ReportItemMetaPaint::checkReportSubstitutionRegExp = QRegularExpression(QRegularExpression::anchoredPattern(R"(@\{check:(\d)\})"));

ReportItemMetaPaint::ReportItemMetaPaint()
	: Super(nullptr)
{
}

ReportItemMetaPaint::ReportItemMetaPaint(ReportItemMetaPaint *_parent, ReportItem *report_item)
	: Super(_parent)
{
	Q_ASSERT(report_item);
	Q_ASSERT(report_item->processor());
	double fill_vertical_layout_ratio = report_item->childSize(ReportItem::LayoutVertical).fillLayoutRatio();
	setFillVLayoutRatio(fill_vertical_layout_ratio);
}

ReportItemMetaPaint::~ ReportItemMetaPaint() = default;

ReportItemMetaPaint* ReportItemMetaPaint::child(int ix) const
{
	auto *ret = dynamic_cast<ReportItemMetaPaint*>(Super::child(ix));
	if(!ret) {
		qfWarning() << "Child at index" << ix << "is not a kind of ReportItemMetaPaint.";
		//qfInfo() << NecroLog::stackTrace();
	}
	return ret;
}

void ReportItemMetaPaint::paint(ReportPainter *painter, unsigned mode)
{
	foreach(Super *_it, children()) {
		auto *it = static_cast<ReportItemMetaPaint*>(_it);
		it->paint(painter, mode);
	}
}

void ReportItemMetaPaint::setInset(qreal hinset, qreal vinset)
{
	if(insetHorizontal() == hinset && insetVertical() == vinset) {
	}
	else {
		f_layoutSettings[LayoutSetting::HInset] = hinset;
		f_layoutSettings[LayoutSetting::VInset] = vinset;
	}
}

void ReportItemMetaPaint::shiftChildren(const ReportItem::Point offset)
{
	foreach(Super *_it, children()) {
		auto *it = static_cast<ReportItemMetaPaint*>(_it);
		it->renderedRect.translate(offset);
		it->shiftChildren(offset);
	}
}

void ReportItemMetaPaint::expandChildFrames()
{
	if(renderedRect.flags & ReportItem::Rect::LayoutHorizontalFlag) {
		/// ve smeru layoutu natahni jen posledni dite az po inset
		ReportItemMetaPaint *it = lastChild();
		if(it && it->isExpandable()) {
			it->renderedRect.setRight(renderedRect.right() - insetHorizontal());
		}
	}
	else {
		/// ve smeru layoutu natahni jen posledni dite az po inset
		ReportItemMetaPaint *it = lastChild();
		if(it && it->isExpandable()) {
			it->renderedRect.setBottom(renderedRect.bottom() - insetVertical());
		}
	}
	foreach(Super *_it, children()) {
		/// ve smeru ortogonalnim k layoutu natahni vsechny deti
		auto *it = static_cast<ReportItemMetaPaint*>(_it);
		if(it->isExpandable()) {
			if(renderedRect.flags & ReportItem::Rect::LayoutHorizontalFlag) {
				it->renderedRect.setTop(renderedRect.top() + insetVertical());
				it->renderedRect.setBottom(renderedRect.bottom() - insetVertical());
			}
			else {
				it->renderedRect.setLeft(renderedRect.left() + insetHorizontal());
				it->renderedRect.setRight(renderedRect.right() - insetHorizontal());
			}
		}
	}
}

bool ReportItemMetaPaint::hasSpringChildrenFramesInVerticalLayout()
{
	bool has_expandable_children = false;
	for(int i=0; i<childrenCount(); i++) {
		ReportItemMetaPaint *it = child(i);
		double d = it->fillVLayoutRatio();
		if(d >= 0) {
			has_expandable_children = true;
			break;
		}
	}
	return has_expandable_children;
}

style::CompiledTextStyle ReportItemMetaPaint::effectiveTextStyle()
{
	style::CompiledTextStyle ret;
	ReportItemMetaPaint *it = this;
	while(it) {
		ret = it->textStyle();
		if(!ret.isNull())
			break;
		it = it->parent();
	}
	QF_ASSERT(!ret.isNull(), "Cannot find TextStyle definition in parents", return ret);
	return ret;
}

void ReportItemMetaPaint::expandChildVerticalSpringFrames()
{
	qfLogFuncFrame() << "rendered rect:" << renderedRect.toString();
	bool has_expandable_children = false;
	for(int i=0; i<childrenCount(); i++) {
		ReportItemMetaPaint *it = child(i);
		double d = it->fillVLayoutRatio();
		if(d >= 0) {
			has_expandable_children = true;
			break;
		}
	}
	if(!has_expandable_children)
		return;

	double layout_size = renderedRect.height();
	double layout_inset = insetVertical();
	layout_size -= 2 * layout_inset;
	/// rozpocitej procenta, je to napsano pro oba layouty, ale asi to nepujde kvuli zalamovani textu pouzit nikdy pro horizontalni layout
	if(layout() == qf::gui::graphics::LayoutVertical) {
		qreal sum_percent = 0;
		int cnt_0_percent = 0;
		qreal sum_mm = 0;
		QList<int> spring_children_ixs;
		for(int i=0; i<childrenCount(); i++) {
			ReportItemMetaPaint *it = child(i);
			double d = it->fillVLayoutRatio();
			if(d < 0) {
				sum_mm += it->renderedRect.height();
			}
			else {
				if(d == 0)
					cnt_0_percent++;
				else
					sum_percent += d;
				spring_children_ixs << i;
			}
		}
		qreal rest_percent = 1 - sum_percent;
		rest_percent = std::max<qreal>(rest_percent, 0);
		qreal percent_0 = 0;
		if(cnt_0_percent > 0)
			percent_0 = rest_percent / cnt_0_percent;
		if(spring_children_ixs.count()) {

			double rest_mm = layout_size - sum_mm;
			rest_mm = std::max<double>(rest_mm, 0);

			double children_ly_offset = 0; //insetVertical();
			for(int i=0; i<childrenCount(); i++) {
				ReportItemMetaPaint *it = child(i);
				if(children_ly_offset > 0) {
					/// nejdriv posun deti, nafouknuti itemu
					ReportItem::Point p;
					p.setY(children_ly_offset);
					it->shift(p);
				}
				if(spring_children_ixs.contains(i)) {
					double curr_ly_size = it->renderedRect.height();
					double new_ly_size = it->fillVLayoutRatio();
					if(new_ly_size == 0)
						new_ly_size = percent_0;
					new_ly_size = new_ly_size * rest_mm;
					//qfInfo() << reccnt << "expanding:" << it->reportItem()->element.tagName() << " id:" << it->reportItem()->element.attribute("id") << "#" << i << "rest mm:" << rest_mm << "new ly_size:" << new_ly_size;
					double ly_size_offset = new_ly_size - curr_ly_size;
					if(ly_size_offset < 0) {
						//qfWarning() << "new_ly_size:" << new_ly_size << "is smaller than old one:" << curr_ly_size << "ignoring";
					}
					else {
						//qfInfo() << it->renderedRect.height() << "->" << new_ly_size;
						it->renderedRect.setHeight(new_ly_size);
						it->alignChildren(); /// kdyz neco expanduju, tak to musim taky zarovnat
						it->expandChildVerticalSpringFrames();
						children_ly_offset += ly_size_offset;
					}
				}
			}
		}
	}
	else if(layout() == qf::gui::graphics::LayoutStacked) {
		for(int i=0; i<childrenCount(); i++) {
			ReportItemMetaPaint *it = child(i);
			double d = it->fillVLayoutRatio();
			//qfDebug() << it->reportItem() << "d:" << d;
			if(d >= 0) {
				if(d == 0)
					d = 1;
				qreal new_ly_size = d * layout_size;
				it->renderedRect.setHeight(new_ly_size);
				it->alignChildren(); /// kdyz neco expanduju, tak to musim taky zarovnat
				it->expandChildVerticalSpringFrames();
			}
		}
	}
	qfDebug() << "\t expanded rendered rect:" << renderedRect.toString();
}

void ReportItemMetaPaint::alignChildren()
{
	qfLogFuncFrame();
	Rect dirty_rect = renderedRect.adjusted(insetHorizontal(), insetVertical(), -insetHorizontal(), -insetVertical());
	//qfDebug() << "\t TAG:" << reportItem()->element.tagName();
	qfDebug() << "\t dirty_rect:" << dirty_rect.toString();
	qfDebug() << "\t alignment:" << alignment();
	qfDebug() << "\t layout:" << layout();
	if(dirty_rect.isValid()) {
		if(alignment() & ~(Qt::AlignLeft | Qt::AlignTop)) {
			Point offset;
			if(layout() == qf::gui::graphics::LayoutVertical && hasSpringChildrenFramesInVerticalLayout()) {
				/// pokud ma spring item ve vertikalnim layoutu, neni treba nic posouvat, o to se postara expandChildVerticalSpringFrames()
			}
			else {
				/// ve smeru layoutu posun cely blok
				Rect r1;
				/// vypocitej velikost potisknuteho bloku
				for(int i=0; i<childrenCount(); i++) {
					ReportItemMetaPaint *it = child(i);
					qfDebug() << "\t\t item potisknuty blok:" << it->renderedRect.toString();
					if(i == 0)
						r1 = it->renderedRect;  // NOLINT(cppcoreguidelines-slicing)
					else
						r1 = r1.united(it->renderedRect);
				}
				qfDebug() << "\t potisknuty blok:" << r1.toString();
				qreal al = 0, d;
				if(layout() == qf::gui::graphics::LayoutHorizontal) {
					if(alignment() & Qt::AlignHCenter)
						al = 0.5;
					else if(alignment() & Qt::AlignRight)
						al = 1;
					d = dirty_rect.width() - r1.width();
					if(al > 0 && d > 0)  {
						offset.rx() = d * al - (r1.left() - dirty_rect.left());
					}
				}
				else if(layout() == qf::gui::graphics::LayoutVertical) {
					if(alignment() & Qt::AlignVCenter) al = 0.5;
					else if(alignment() & Qt::AlignBottom) al = 1;
					d = dirty_rect.height() - r1.height();
					if(al > 0 && d > 0)  {
						offset.ry() = d * al - (r1.top() - dirty_rect.top());
					}
				}
			}
			qfDebug() << "\t offset ve smeru layoutu:" << offset.toString();
			/// v orthogonalnim smeru kazdy item
			for(int i=0; i<childrenCount(); i++) {
				ReportItemMetaPaint *it = child(i);
				const Rect &r1 = it->renderedRect;
				qfDebug() << "\t\titem renderedRect:" << r1.toString();
				qreal al = 0, d;

				if(orthogonalLayout() == qf::gui::graphics::LayoutHorizontal) {
					offset.rx() = 0;
					if(alignment() & Qt::AlignHCenter) al = 0.5;
					else if(alignment() & Qt::AlignRight) al = 1;
					d = dirty_rect.width() - r1.width();
					if(al > 0 && d > 0)  {
						qfDebug() << "\t\thorizontal alignment:" << al;
						offset.rx() = d * al - (r1.left() - dirty_rect.left());
					}
				}
				else if(orthogonalLayout() == qf::gui::graphics::LayoutVertical) {
					offset.ry() = 0;
					al = 0;
					if(alignment() & Qt::AlignVCenter) al = 0.5;
					else if(alignment() & Qt::AlignBottom) al = 1;
					d = dirty_rect.height() - r1.height();
					if(al > 0 && d > 0)  {
						qfDebug() << "\t\tvertical alignment:" << al;
						offset.ry() = d * al - (r1.top() - dirty_rect.top());
					}
				}
				qfDebug() << "\t\talign offset:" << offset.toString();
				if(!offset.isNull()) it->shift(offset);
			}
		}
	}
}

QString ReportItemMetaPaint::dump(int indent)
{
	QString indent_str;
	indent_str.fill(' ', indent);
	const char *type_name = typeid(*this).name();
	QString ret = QString("%1[%2] 0x%3").arg(indent_str).arg(type_name).arg(reinterpret_cast<qulonglong>(this), 0, 16);
	//QString ret = QString("%1[%2] 0x%3").arg(indent_str).arg(type_name).arg((qulonglong)this, 0, 16);
	auto *frm = dynamic_cast<ReportItemMetaPaintFrame*>(this);
	if(frm) ret += " : " + frm->renderedRect.toString();
	ret += "\n";
	foreach(Super *_it, children()) {
		auto *it = static_cast<ReportItemMetaPaint*>(_it);
		ret += it->dump(indent + 2);
	}
	return ret;
}

//=================================================
//                              ReportItemMetaPaintReport
//=================================================
ReportItemMetaPaintReport::ReportItemMetaPaintReport(ReportItem *report_item)
	: ReportItemMetaPaint(nullptr, report_item), orientation()
{
	//f_reportProcessor = report_item->processor;
	/*--
	QString s = report_item->property("orientation", "portrait");
	if(s == "landscape") orientation = QPrinter::Landscape;
	else orientation = QPrinter::Portrait;
	pageSize = QSize(report_item->property("w").toInt(), report_item->property("h").toInt());
	--*/
}

//=================================================
//           ReportItemMetaPaintFrame
//=================================================
ReportItemMetaPaintFrame::ReportItemMetaPaintFrame(ReportItemMetaPaint *_parent, ReportItem *report_item)
	: ReportItemMetaPaint(_parent, report_item)
	, lbrd(Qt::NoPen)
	, rbrd(Qt::NoPen)
	, tbrd(Qt::NoPen)
	, bbrd(Qt::NoPen)
{
	//qfDebug() << QF_FUNC_NAME << reportElement.tagName();
	QF_ASSERT_EX(report_item != nullptr, "ReportItem is NULL");
	fill = Qt::NoBrush;
	bool design_mode = false;
	ReportProcessor *proc = report_item->processor();
	if(proc)
		design_mode = proc->isDesignMode();
	auto *frame_item = qobject_cast<ReportItemFrame*>(report_item);
	if(frame_item) {
		{
			style::Brush *b = frame_item->fill();
			if(b)
				fill = b->brush();
		}
		{
			if(design_mode) {
				QPen pn(Qt::DotLine);
				pn.setColor(Qt::blue);
				lbrd = rbrd = tbrd = bbrd = pn;
			}
			if(frame_item->borderLeft())
				lbrd = frame_item->borderLeft()->pen();
			if(frame_item->borderRight())
				rbrd = frame_item->borderRight()->pen();
			if(frame_item->borderTop())
				tbrd = frame_item->borderTop()->pen();
			if(frame_item->borderBottom())
				bbrd = frame_item->borderBottom()->pen();
			//qfInfo() << report_item << p;
		}
		{
			style::Text *pts = frame_item->textStyle();
			if(pts) {
				style::CompiledTextStyle ts = pts->textStyle();
				setTextStyle(ts);
			}
		}
	}
}

void ReportItemMetaPaintFrame::paint(ReportPainter *painter, unsigned mode)
{
	//qfDebug() << QF_FUNC_NAME << reportElement.tagName();
	QF_ASSERT(painter, "painter is NULL", return);
	//qfDebug() << "\trenderedRect:" << renderedRect.toString();
	auto selected_item = painter->selectedItem();
	bool selected = (selected_item == this);
	if(selected)
		qfDebug() << selected_item;
	if(mode & PaintFill)
		fillItem(painter, selected);
	Super::paint(painter, mode);
	if(mode & PaintBorder)
		frameItem(painter, selected);
}

void ReportItemMetaPaintFrame::fillItem(QPainter *painter, bool selected)
{
	Rect r = qf::gui::graphics::mm2device(renderedRect, painter->device());
	//qfDebug().color(QFLog::Yellow) << QF_FUNC_NAME << reportElement.tagName();
	//qfInfo() << "\t logicalDpiX:" << painter->device()->logicalDpiX();
	//qfInfo() << "\t logicalDpiY:" << painter->device()->logicalDpiY();
	//qfInfo() << "\t rendered rect:" << renderedRect.toString();
	//qfInfo() << "\t br:" << r.toString();
	//qfDebug() << "\tbrush color:"
	if(selected) {
		painter->fillRect(r, QColor(0xFFEEEE));
	}
	else {
		if(fill.style() != Qt::NoBrush)
			painter->fillRect(r, fill);
	}
	//painter->fillRect(r, QColor("orange"));
}

void ReportItemMetaPaintFrame::frameItem(QPainter *painter, bool selected)
{
	Q_UNUSED(selected);
	if(selected) {
		QPen p(Qt::SolidLine);
		p.setColor(Qt::magenta);
		p.setWidth(2);
		painter->setPen(p);
		painter->setBrush(QBrush());
		painter->drawRect(qf::gui::graphics::mm2device(renderedRect, painter->device()));
	}
	else {
		drawLine(painter, LBrd, lbrd);
		drawLine(painter, TBrd, tbrd);
		drawLine(painter, RBrd, rbrd);
		drawLine(painter, BBrd, bbrd);
	}
}

void ReportItemMetaPaintFrame::drawLine(QPainter *painter, LinePos where, const QPen &_pen)
{
	if(_pen.style() == Qt::NoPen || _pen.widthF() == 0)
		return;
	QPen pen = _pen;
	/// preved tiskarske body na body vystupniho zarizeni
	qreal w = pen.widthF() * 25.4 / 72;
	/// ted je w v milimetrech
	bool horizontal = (where == TBrd || where == BBrd);
	if(horizontal)
		pen.setWidthF(qf::gui::graphics::y2device(w, painter->device()));
	else
		pen.setWidthF(qf::gui::graphics::x2device(w, painter->device()));
	Point p1, p2;
	Rect r = qf::gui::graphics::mm2device(renderedRect, painter->device());
	if(where == TBrd) { p1 = r.topLeft(); p2 = r.topRight(); }
	else if(where == LBrd) { p1 = r.topLeft(); p2 = r.bottomLeft(); }
	else if(where == BBrd) { p1 = r.bottomLeft(); p2 = r.bottomRight(); }
	else if(where == RBrd) { p1 = r.topRight(); p2 = r.bottomRight(); }
	if(!(p1 == p2)) {
		//qfInfo() << f_reportItem << "draw line from:" << p1.toString() << "to:" << p2.toString() << "pen width:" << pen.widthF() << "style:" << pen.style() << "color:" << pen.color().name();
		painter->setPen(pen);
		painter->drawLine(p1, p2);
	}
}
//=================================================
// ReportItemMetaPaintText
//=================================================
void ReportItemMetaPaintText::paint(ReportPainter *painter, unsigned mode)
{
	//qfDebug() << QF_FUNC_NAME << reportElement.tagName();
	QF_ASSERT(painter, "painter is NULL", return);
	if(mode != PaintFill)
		return;

	//bool is_yellow = false;
	auto *rep_painter = painter;
	if(rep_painter && rep_painter->isMarkEditableSqlText() && !sqlId.isEmpty()) {
		/// zazlut cely parent frame, az do ktereho se muze editovatelny text roztahnout
		auto *it = dynamic_cast<ReportItemMetaPaintFrame*>(parent());
		if(it) {
			Rect r = qf::gui::graphics::mm2device(it->renderedRect, painter->device());
			painter->fillRect(r, Qt::yellow);
			//is_yellow = true;
		}
	}

	QFontMetricsF font_metrics = QFontMetricsF(painter->font(), painter->device());
	//ReportItemMetaPaintFrame::paint(painter);
	//qfDebug() << "\trenderedRect:" << renderedRect.toString();
	//painter->setBrush(brush);
	painter->setPen(pen);
	painter->setFont(font);
	QString s = text;
	//if(is_yellow) qfInfo() << s.toUtf8().toHex() << s.length();// << s.replace("\n", "<LF>").replace("\r", "<CR>");
	if(text.indexOf('@') >= 0) {
		//s = s.replace(currentPageReportSubstitution, QString::number(painter->currentPage + 1));
		s = s.replace(pageCountReportSubstitution, QString::number(painter->pageCount));
	}
	Rect br = renderedRect; // NOLINT(cppcoreguidelines-slicing)
	auto parent_item = parent();
	if(parent_item) {
		Rect pbr = parent_item->renderedRect; // NOLINT(cppcoreguidelines-slicing)
		qreal hinset = br.left() - pbr.left();
		if(textOption.alignment() & Qt::AlignRight) {
			br.moveLeft(br.left() + pbr.width() - br.width() - hinset - hinset);
		}
		else if(textOption.alignment() & Qt::AlignHCenter) {
			br.moveLeft(br.left() + ((pbr.width() - br.width()) / 2) - hinset);
		}
	}
	br = qf::gui::graphics::mm2device(br, painter->device());
	br.adjust(0, 0, 1, 1); /// nekdy se stane, kvuji nepresnostem prepocitavani jednotek, ze se to vyrendruje pri tisku jinak, nez pri kompilaci, tohle trochu pomaha:)

	/// to samy jako v #if 0, jen se to tiskne stejnym zpusobem, jako se to kompilovalo, coz muze ukazat, proc to vypada jinak, nez cekam
	qreal leading = font_metrics.leading();
	qreal height = 0;
	//qreal width = 0;
	QTextLayout textLayout;
	//Qt::Alignment alignment = (~Qt::Alignment()) & flags;
	QTextOption opt = textOption;
	opt.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
	textLayout.setTextOption(opt);
	textLayout.setFont(painter->font());
	textLayout.setText(s);
	textLayout.beginLayout();
	while (true) {
		QTextLine line = textLayout.createLine();
		if(!line.isValid()) {
			break;
		}
		line.setLineWidth(br.width());
		if(height > 0)
			height += leading;
		line.setPosition(QPointF(0., height));
		height += line.height();
		//width = qMax(width, line.naturalTextWidth());
	}
	textLayout.endLayout();
	textLayout.draw(painter, br.topLeft());
}

QString ReportItemMetaPaintText::dump(int indent)
{
	QString indent_str;
	indent_str.fill(' ', indent);
	QString ret = QString("%1[%2] 0x%3").arg(indent_str).arg(typeid(*this).name()).arg(reinterpret_cast<qulonglong>(this), 0, 16);
	//QString ret = QString("%1[%2] 0x%3").arg(indent_str).arg(typeid(*this).name()).arg((qulonglong)this, 0, 16);
	ret += QString(" '%1'\n").arg(text);
	foreach(auto _it, children()) {
		auto *it = static_cast<ReportItemMetaPaint*>(_it);
		ret += it->dump(indent + 2);
	}
	return ret;
}

//=================================================
//           ReportItemMetaPaintCheck
//=================================================
void ReportItemMetaPaintCheck::paint(ReportPainter * painter, unsigned mode)
{
	//qfDebug() << QF_FUNC_NAME << reportElement.tagName();
	QF_ASSERT(painter, "painter is NULL", return);
	if(mode != PaintFill)
		return;

	QFontMetricsF font_metrics = QFontMetricsF(painter->font(), painter->device());

	//qfInfo() << "paint" << text;
	QRegularExpression rx = ReportItemMetaPaint::checkReportSubstitutionRegExp;
	if(auto match = rx.match(text); match.hasMatch()) {
		bool check_on = match.capturedTexts().value(1) == "1";
		/// V tabulkach by jako check OFF slo netisknout vubec nic,
		/// ale na ostatnich mistech repotu je to zavadejici .

		/// BOX
		painter->setPen(QPen(Qt::SolidLine));
		painter->setBrush(QBrush());
		ReportItem::Rect r = renderedRect;
		r.translate(0, -font_metrics.leading());
		qreal w = renderedRect.width();
		Qt::Alignment alignment_flags = textOption.alignment();
		if(alignment_flags & Qt::AlignHCenter)
			r.translate((w - r.width())/2., 0);
		else if(alignment_flags & Qt::AlignRight)
			r.translate(w - r.width(), 0);
		painter->drawRect(qf::gui::graphics::mm2device(r, painter->device()));

		if(check_on) {
#ifdef CHECK_MARK_HOOK
			/// CHECK hook
			static QString s_check = "color: teal; style: solid; size:2";
			painter->setPen(context().styleCache().pen(s_check));
			r = qf::gui::graphics::mm2device(r, painter->device());
			QPointF p1(r.left(), r.top() + r.height() / 2);
			QPointF p2(r.left() + r.width() / 2, r.bottom());
			painter->drawLine(p1, p2);
			p1 = QPointF(r.right() + 0.2 * r.width(), r.top());
			painter->drawLine(p2, p1);
#else
			/// CHECK cross
			//--static QString s_check = "color: maroon; style: solid; size:2";
			QPen p(Qt::SolidLine);
			auto c = QColor::fromString("maroon");
			p.setColor(c);
			painter->setPen(p);
			r = qf::gui::graphics::mm2device(r, painter->device());
			painter->drawLine(r.topLeft(), r.bottomRight());
			painter->drawLine(r.bottomLeft(), r.topRight());
#endif
		}
		return;
	}
}

//=================================================
//                              ReportItemMetaPaintImage
//=================================================
void ReportItemMetaPaintImage::paint(ReportPainter *painter, unsigned mode)
{
	//qfDebug().color(QFLog::Green) << QF_FUNC_NAME << reportElement.tagName() << "mode:" << mode;
	QF_ASSERT(painter, "painter is nullptr", return);
	auto *printer = dynamic_cast<QPrinter*>(painter->device());
	//if(printer) { qfInfo() << "printer output format:" << printer->outputFormat() << "is native printer:" << (printer->outputFormat() == QPrinter::NativeFormat); }
	if(printer && printer->outputFormat() == QPrinter::NativeFormat) {
		if(isSuppressPrintOut()) {
			qfInfo() << "print out suppressed";
			return;
		}
	}
	if(mode != PaintFill) return;

	Rect br = qf::gui::graphics::mm2device(renderedRect, painter->device());
	//br.adjust(0, 0, 1, 1); /// nekdy se stane, kvuji nepresnostem prepocitavani jednotek, ze se to vyrendruje pri tisku jinak, nez pri kompilaci, tohle trochu pomaha:)
	//r.setHeight(500);
	//painter->fillRect(r, QColor("#DDDDDD"));
	//qfWarning().noSpace() << "'" << s << "' flags: " << flags;
	if(image.isImage()) {
		//qfInfo() << "\t is image";
		painter->save();
		QSize sz = image.image.size();
		painter->translate(br.topLeft());
		painter->scale(br.width() / sz.width(), br.height() / sz.height());
		painter->drawImage(QPoint(0, 0), image.image);
		painter->restore();
		//painter->drawImage(br.topLeft(), image.image.scaled(br.size().toSize(), aspectRatioMode, Qt::SmoothTransformation));
	}
	else if(image.isPicture()) {
		painter->save();
		QRect pict_r = image.picture.boundingRect();
		QSize pict_sz = pict_r.size();
		painter->translate(br.topLeft());
		{
			/// dosud nepodporuji aspect ratio, zda se
			//painter->fillRect(image.picture.boundingRect(), Qt::cyan);
			//painter->scale(renderedRect.width() / sz.width(), renderedRect.height() / sz.height());
			/// pokud se rozmery a posunuti obrazku lisi od velikosti regionu pro nej urceneho, zmensi/zvetsi a posun ho tak,
			/// aby vyplnil cely region.
			painter->scale(br.width() / pict_sz.width(), br.height() / pict_sz.height());
			/// pokud pict_r nezacina na [0,0], je treba posunout painter na zacatek obrazku
			painter->translate(-pict_r.topLeft());
			//qfInfo() << "\t picture scale:" << br.width() / sz.width() << br.height() / sz.height();
			//qfInfo() << "\t rendered rect mm:" << renderedRect.toString();
			//qfInfo() << "\t rendered rect px:" << br.toString();
			//qfInfo() << "\t picture bounding rect:" << Rect(image.picture.boundingRect()).toString();
		}
		painter->drawPicture(QPoint(0, 0), image.picture);
		painter->restore();
	}
	//if(image.isImage()) painter->drawImage(br.topLeft(), image.image.scaled(br.size().toSize(), Qt::KeepAspectRatioByExpanding));
}

QString ReportItemMetaPaintImage::dump(int indent)
{
	QString indent_str;
	indent_str.fill(' ', indent);
	QString ret = QString("%1[%2] 0x%3").arg(indent_str).arg(typeid(*this).name()).arg(reinterpret_cast<qulonglong>(this), 0, 16);
	//QString ret = QString("%1[%2] 0x%3").arg(indent_str).arg(typeid(*this).name()).arg((qulonglong)this, 0, 16);
	ret += QString(" '%1'\n").arg("image");
	return ret;
}

//=================================================
//                              ReportPainter
//=================================================
ReportPainter::ReportPainter(QPaintDevice *device)
	: QPainter(device)
{
	//currentPage = 0;
	pageCount = 0;
	f_selectedItem = nullptr;
	setMarkEditableSqlText(false);
}

void ReportPainter::drawMetaPaint(ReportItemMetaPaint *item)
{
	if(item) {
		item->paint(this, ReportItemMetaPaint::PaintFill);
		item->paint(this, ReportItemMetaPaint::PaintBorder);
	}
}




