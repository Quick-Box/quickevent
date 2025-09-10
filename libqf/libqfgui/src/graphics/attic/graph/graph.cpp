
//
// Author: Frantisek Vacek <fanda.vacek@volny.cz>, (C) 2007 - 2014
//
// Copyright: See COPYING file that comes with this distribution
//

#include "graph.h"

#include <qf/core/log.h>
#include <qf/core/string.h>
#include <qf/core/utils/treetable.h>

namespace qfc = qf::core;
namespace qfu = qf::core::utils;
using namespace qf::gui::graphics;

//======================================================
//          Graph::Serie
//======================================================
const Graph::Serie & Graph::Serie::sharedNull()
{
	static Serie n = Serie(SharedDummyHelper());
	return n;
}

Graph::Serie::Serie(Graph::Serie::SharedDummyHelper )
{
	d = new Data();
}

Graph::Serie::Serie()
{
	*this = sharedNull();
}

//======================================================
//          Graph::Axis
//======================================================
const Graph::Axis & Graph::Axis::sharedNull()
{
	static Axis n = Axis(SharedDummyHelper());
	return n;
}

Graph::Axis::Axis(SharedDummyHelper )
{
	d = new Data();
}

Graph::Axis::Axis()
{
	*this = sharedNull();
}

//======================================================
//            Graph::Legend
//======================================================
const Graph::Legend & Graph::Legend::sharedNull()
{
	static Legend n = Legend(SharedDummyHelper());
	return n;
}

Graph::Legend::Legend(Graph::Legend::SharedDummyHelper )
{
	d = new Data();
}

Graph::Legend::Legend()
{
	*this = sharedNull();
}

//======================================================
//              Graph::Axis
//======================================================

qreal Graph::Axis::value2pos(qreal value, const Rect &grid_rect)
{
	//qfInfo() << "value:" << value;
	qreal ret = 0;
	if(valuesFrom() == TakeValues) {
		ret = (value - min()) / (max() - min());
	}
	else if(valuesFrom() == TakeOrder) {
		ret = (value + tick()/2 - min()) / (max() - min());
	}
	if(direction() == DirectionX) {
		ret *= grid_rect.width();
		ret += grid_rect.left();
	}
	else {
		ret *= grid_rect.height();
		ret = grid_rect.bottom() - ret;
	}
	return ret;
}

qreal Graph::Axis::tickSize(const Rect &grid_rect)
{
	qreal ret = 0;
	ret = tick() / (max() - min());
	if(direction() == DirectionX) {
		ret *= grid_rect.width();
	}
	else {
		ret *= grid_rect.height();
	}
	return ret;
}

QString Graph::Axis::formatTickLabel(const QVariant & label_value)
{
	QString ret;
	QString fmt = labelFormat();
	if(!fmt.isEmpty()) {
		if(label_value.type() == QVariant::String) {
			ret = label_value.toString();
		}
		else if(label_value.type() == QVariant::Double) {
			ret = qfc::String::number(label_value.toDouble(), fmt);
		}
		else if(label_value.type() == QVariant::Int) {
			ret = qfc::String::number(label_value.toInt(), fmt);
		}
		else {
			ret = qfc::String::number(label_value.toDouble(), fmt);
		}
	}
	else
		ret = label_value.toString();
	return ret;
}

//======================================================
//                Graph
//======================================================
QStringList Graph::colorNamesPull;

Graph::Graph()
{
	d = &_d;
}

Graph::Graph(const QVariantMap &def, const qfu::TreeTable &_data)
{
	d = &_d;
	setDefinition(def);
	setData(_data);
}

Graph::~Graph()
{
}

struct SerieSort_helper
{
	int index;
	double value;

	bool operator<(const SerieSort_helper &other) const { return other.value < value; }

	SerieSort_helper(int ix, double val) : index(ix), value(val) {}
};

void Graph::createSeries()
{
	int serie_no = 0;
	QVariantList v_series = definition().value("series").toList();
	for(auto v : v_series) {
		QVariantMap m_serie = v.toMap();
		QString colname = m_serie.value("colname").toString();
		Serie serie;
		{
			QString s = m_serie.value("point").toMap().value("color").toString();
			QColor color;
			if(s.isEmpty())
				color = colorForIndex(serie_no);
			else
				color = styleCache().color(s);
			//qfInfo() << colname << color.name();
			serie.setColor(color);
		}
		{
			QString s = data().value(QString("CAPTION(%1)").arg(colname)).toString();
			serie.setColumnCaption(s);
		}
		{
			qfu::TreeTable t = data();
			int ix = t.columns().indexOf(colname);
			QVariantList vlst;
			for(int i=0; i<t.rowCount(); i++) {
				qfu::TreeTableRow r = t.row(i);
				QVariant v = r.value(ix);
				vlst << v;
			}
			serie.setValues(vlst);
		}
		QString sort_colname = m_serie.value("sortColName").toString();
		if(!sort_colname.isEmpty()) {
			QList<int> sorted_indexes;
			//qfInfo() << "sortcolname:" << sort_colname;
			QList<SerieSort_helper> sorted;
			qfu::TreeTable t = data();
			int sort_col_ix = t.columns().indexOf(sort_colname);
			for(int i=0; i<t.rowCount(); i++) {
				qfu::TreeTableRow r = t.row(i);
				sorted << SerieSort_helper(i, r.value(sort_col_ix).toDouble());
			}
			qSort(sorted);
			for(int i=0; i<sorted.count(); i++) {
			//qfInfo() << sorted[i].index << "-" << sorted[i].value;
				/// values obsahuji indexy setrideni podle velikosti
				sorted_indexes << sorted[i].index;
			}
			serie.setSortedValuesIndexes(sorted_indexes);
		}
		d->seriesMap[colname] = serie;
		serie_no++;
	}
}

void Graph::createAxes()
{
	QVariantList v_series = definition().value("series").toList();
	for(auto v : v_series) {
		QVariantMap m_serie = v.toMap();
		QString colname = m_serie.value("colname").toString();
		QVariantMap m_axis = m_serie.value("axis").toMap();

		Serie serie = seriesMap().value(colname);

		Axis axis;
		axis.setDirection(m_axis.value("direction", "y") == "y"? Axis::DirectionY: Axis::DirectionX);
		axis.setValuesFrom((m_axis.value("valuesFrom", "takeValue") == "takeValue")? Axis::TakeValues: Axis::TakeOrder);
		axis.setHidden(m_axis.value("hidden", false).toBool());
		QVariantMap m_label = m_axis.value("label").toMap();
		if(!m_label.isEmpty()) {
			QString s = m_label.value("text").toString();
			s.replace("${COLUMN_CAPTION}", serie.columnCaption());
			axis.setLabel(s);
			axis.setLabelStyle(m_label.value("textStyle", "default").toString());
		}
		QVariantMap m_gridlines = m_axis.value("gridLines").toMap();
		if(!m_gridlines.isEmpty()) {
			axis.setGridLines(true);
		}

		if(axis.valuesFrom() == Axis::TakeValues) {
			QVariantList values = serie.values();
			QVariantMap m_axis_ticks = m_axis.value("ticks").toMap();
			if(!m_axis_ticks.isEmpty()) {
				axis.setLabelFormat(m_axis_ticks.value("labelFormat").toString());

				QString same_as_colname = m_axis_ticks.value("sameAs").toString();
				if(same_as_colname.isEmpty()) {
					/// najdi min - max
					foreach(QVariant v, values) {
						double d = v.toDouble();
						axis.setMin(qMin(axis.min(), d));
						axis.setMax(qMax(axis.max(), d));
					}
					QVariantMap m_range = m_axis_ticks.value("range").toMap();
					if(m_range.value("min").isValid())
						axis.setMin(m_range.value("min").toDouble());
					if(m_range.value("max").isValid())
						axis.setMax(m_range.value("max").toDouble());
					//qfInfo() << "axis:" << axis.toString();
					double range = axis.max() - axis.min();
					if(range > 0) {
						double tick_range = range / 10;
						double exponent = floor(log10(tick_range));
						tick_range = tick_range * pow(10, -exponent);
						int int_tick_range;
						/// find closest 1, 2, 5 or 10
						if(tick_range < 1.5) int_tick_range = 1;
						else if(tick_range < 3.5) int_tick_range = 2;
						else if(tick_range < 7.5) int_tick_range = 5;
						else int_tick_range = 10;
						tick_range = int_tick_range * pow(10, exponent);
						axis.setMin(tick_range * floor(axis.min() / tick_range));
						axis.setMax(tick_range * ceil(axis.max() / tick_range));
						axis.setTick(tick_range);
					}
				}
				else {
					Axis original = axisForSerie(same_as_colname);
					axis.setMin(original.min());
					axis.setMax(original.max());
					axis.setTick(original.tick());
				}
			}
		}
		else if(axis.valuesFrom() == Axis::TakeOrder) {
			QVariantList values = serie.values();
			axis.setMin(0);
			axis.setMax(values.count());
			axis.setTick(1);
		}
		d->axesMap[colname] = axis;
	}
}

Graph::Axis Graph::axisForSerie(const QString & colname)
{
	return axesMap().value(colname);
}

QString Graph::title() const
{
	return definition().value("title").toMap().value("text").toString();
}

void Graph::draw(QPainter *painter, const QSizeF &size)
{
	qfLogFuncFrame() << "painter:" << painter << "size:" << size.width() << "x" << size.height();
	d->painter = painter;
	d->boundingRect = QRectF(0, 0, size.width(), size.height());
	d->gridRect = boundingRect();
	createSeries();
	createAxes();
	drawTitle();
	{
		Rect gr = gridRect();
		//qfInfo() << "gridRect1:" << gridRect().toString();
		d->gridRect.setWidth(0);
		Rect r = drawLegends(true);
		//qfInfo() << "r:" << r.toString();
		d->gridRect = gr;
		d->gridRect.adjust(0, 0, -r.width(), 0);
	}
	drawAxes();
	drawGrid();
	drawSeries();
	drawBox();
	drawLegends(false);
}

void Graph::drawTitle()
{
	qfLogFuncFrame() << "boundingRect width:" << boundingRect().width() << "boundingRect height:" << boundingRect().height();
	if(!title().isEmpty()) {
		QString t = title();
		TextStyle st = styleCache().style(definition().value("title").toMap().value("textStyle").toString());
		painter()->setFont(st.font);
		painter()->setPen(st.pen);
		painter()->setBrush(st.brush);
		//painter()->setBrush(QBrush(Qt::red));

		Rect br = mm2device(boundingRect());
		Rect br2;
		//qfInfo() << "font:" << painter()->font().toString();
		painter()->drawText(br, Qt::AlignHCenter, t, &br2);
		br2 = device2mm(br2);
		//qfInfo() << "rect:" << rect().toString();
		//qfInfo() << "br:" << br.toString();
		d->gridRect.setTop(br2.bottom());
		//qfInfo() << "grid rect:" << gridRect().toString();
	}
}

Rect Graph::drawLegends(bool do_not_draw)
{
	Rect bounding_rect;
	Rect br = gridRect();
	//qfInfo() << "gridRect" << gridRect().toString();
	//qfInfo() << "boundingRect" << boundingRect().toString();
	/// vytiskni to do mezery mezi gridRect a boundingRect
	br.setLeft(br.right());
	br.setRight(boundingRect().right());
	//qfInfo() << "br:" << br.toString();
	for(auto v : definition().value("legends").toList()) {
		QVariantMap m_legend = v.toMap();
		QString id = drawLegend(m_legend, br, do_not_draw);
		Legend legend = legendMap().value(id);
		if(!legend.isNull()) {
			if(bounding_rect.isNull())
				bounding_rect = legend.boundingRect();
			else {
				Rect r = legend.boundingRect();
				r.moveTo(bounding_rect.bottomLeft());
				bounding_rect = bounding_rect.united(r);
			}
			br.moveTo(bounding_rect.bottomLeft());
		}
	}
	//qfInfo() << "whole br:" << bounding_rect.toString();
	return bounding_rect;
}

void Graph::drawBox()
{
	qfLogFuncFrame() << "boundingRect width:" << boundingRect().width() << "boundingRect height:" << boundingRect().height();
	painter()->setPen(Qt::black);
	painter()->setBrush(QBrush());
	//painter()->drawRect(mm2device(rect()));
	Rect gr = mm2device(gridRect());
	//painter()->fillRect(gr, QBrush(QColor("khaki")));
	//painter()->setPen(QColor("maroon"));
	painter()->drawRect(gr);
	qfDebug() << "\t RETURN" << QF_FUNC_NAME;
	return;
	//painter()->drawLine(gr.topLeft(), gr.bottomRight());
	//painter()->drawLine(gr.topRight(), gr.bottomLeft());
}

void Graph::drawGrid()
{
	qfLogFuncFrame();
	painter()->setPen(styleCache().pen("graphgrid"));
	QMapIterator<QString, Axis> i(axesMap());
	while (i.hasNext()) {
		i.next();
		Axis axis = i.value();
		if(axis.isGridLines() && axis.tick() > 0) {
			Rect gr = gridRect();
			for(double dd = axis.min() + axis.tick(); dd < axis.max(); dd += axis.tick()) {
				Point p1, p2;
				qreal rr = axis.value2pos(dd, gridRect());
				if(axis.direction() == Axis::DirectionX) {
					p1 = gr.bottomLeft();
					p1.rx() += rr;
					p2 = p1;
					p2.ry() = gr.topLeft().y();
				}
				else if(axis.direction() == Axis::DirectionY) {
					p1 = gr.bottomLeft();
					p1.ry() -= rr;
					p2 = p1;
					p2.rx() = gr.topRight().x();
				}
				qfDebug() << "\t line" << p1.toString() << "->" << p2.toString();
				painter()->drawLine(mm2device(p1), mm2device(p2));
			}
		}
	}
}

void Graph::drawAxes()
{
	qfLogFuncFrame() << "boundingRect width:" << boundingRect().width() << "boundingRect height:" << boundingRect().height();
	//Rect gr = mm2device(gridRect());
	QVariantList v_series = definition().value("series").toList();
	/// vytiskni nanecisto osy Y, aby se nastavil gridrect ve smeru X
	Rect gr = gridRect();
	for(auto v : v_series) {
		QVariantMap m_serie = v.toMap();
		QVariantMap m_axis = m_serie.value("axis").toMap();
		QString colname = m_serie.value("colname", "invalid colname").toString();
		if(m_axis.value("direction", "y").toString() == "y") {
			drawAxis(colname, gr, true);
			Axis axis = axisForSerie(colname);
			if(!axis.isHidden())
				gr.setTopLeft(axis.boundingRect().topRight());
		}
	}
	/// vytiskni osy X, aby se nastavil gridrect ve smeru Y
	for(auto v : v_series) {
		QVariantMap m_serie = v.toMap();
		QVariantMap m_axis = m_serie.value("axis").toMap();
		QString colname = m_serie.value("colname", "invalid colname").toString();
		if(m_axis.value("direction", "y") == "x") {
			drawAxis(colname, gr, false);
			Axis axis = axisForSerie(colname);
			if(!axis.isHidden())
				gr.setBottomLeft(axis.boundingRect().topLeft());
		}
	}
	//gr.setHeight(gridRect().height());
	gr.setTopLeft(gridRect().topLeft());
	/// vytiskni nacisto osy Y
	for(auto v : v_series) {
		QVariantMap m_serie = v.toMap();
		QVariantMap m_axis = m_serie.value("axis").toMap();
		QString colname = m_serie.value("colname", "invalid colname").toString();
		if(m_axis.value("direction", "y") == "y") {
			drawAxis(colname, gr, false);
			Axis axis = axisForSerie(colname);
			if(!axis.isHidden())
				gr.setTopLeft(axis.boundingRect().topRight());
		}
	}
	d->gridRect = gr;
}

void Graph::drawAxis(const QString &colname, const Rect &_bounding_rect, bool do_not_draw)
{
	if(colname.isEmpty()) return;

	Serie serie = seriesMap().value(colname);
	Axis axis = axisForSerie(colname);
	do_not_draw = do_not_draw || axis.isHidden();

	Rect bounding_rect  = _bounding_rect;
	//axis.boundingRect = _bounding_rect;
	if(axis.direction() == Axis::DirectionY) {
		//axis.boundingRect = bounding_rect;
		//bnd_rect.setWidth(0);
		if(!axis.label().isEmpty()) {
			QString t = axis.label();
			TextStyle st = styleCache().style(axis.labelStyle());
			painter()->setFont(st.font);
			painter()->setPen(st.pen);
			painter()->setBrush(st.brush);

				//Rect br(gr.topLeft(), Size(gr.height(), gr.width()));
			Rect br = mm2device(_bounding_rect);
			Rect br1 = br.transposed();
				//painter()->drawText(gr, Qt::AlignHCenter, t, &br2);
			painter()->save();
				//painter()->translate(gr.left(), gr.top() + gr.height()/2);
				//qfInfo() << br.toString();
			painter()->translate(br.bottomLeft());
			/// label se kresli na vysku
			painter()->rotate(-90);
			//painter()->translate(-br.topLeft());
			//painter()->translate(mm2device(Point(-rect().height(), 0)));
			br1.moveTo(Point());
			//painter()->setPen(QPen(QColor(Qt::yellow)));
			//painter()->drawRect(br1);
			Rect br2;
			if(do_not_draw) br2 = painter()->boundingRect(br1, Qt::AlignHCenter, t);
			else painter()->drawText(br1, Qt::AlignHCenter, t, &br2);
			painter()->restore();
			br2 = device2mm(br2);
			bounding_rect.adjust(br2.height(), 0, 0 , 0);
			///- bnd_rect.adjust(0, 0, br2.height(), 0);
		}
		painter()->setPen(QPen());
		//QVariantMap m_ticks = m_axis.cd("ticks").toMap();
		if(axis.tick() > 0) {
			/// zjisti sirku textu hodnot
			Rect gr = mm2device(bounding_rect);
			Rect br;
			for(double dd=axis.min(); dd<=axis.max(); dd+=axis.tick()) {
				QString s = axis.formatTickLabel(dd);
				Rect r = painter()->boundingRect(gr, Qt::AlignLeft, s);
				if(br.isNull()) br = r;
				else br = br.united(r);
					//qfInfo() << dd;
			}
			Rect br2 = device2mm(br);
			bounding_rect.setLeft(br2.right() + 2); /// mezera mezi popisky a osou
			///- axis.boundingRect.adjust(0, 0, br2.widht() + 2, 0);
			if(!do_not_draw) {
				/// vytiskni osu Y
				Rect gr = mm2device(bounding_rect);
				painter()->setPen(styleCache().pen("graphaxis"));
				painter()->drawLine(gr.topLeft(), gr.bottomLeft());
				/// vytiskni popisky
				TextStyle st = styleCache().style("default");
				painter()->setFont(st.font);
				painter()->setPen(st.pen);
				painter()->setBrush(st.brush);
				for(double dd=axis.min(); dd<=axis.max(); dd+=axis.tick()) {
					//qfInfo() << "val:" << dd << "tomm:" << axis.value2mm(dd);
					qreal pos = axis.value2pos(dd, mm2device(bounding_rect));
					Rect r = br;
					r.moveTop(pos);
					r.translate(0, -QFontMetricsF(painter()->font()).height()/2);
					//qfInfo() << QFontMetricsF(painter()->font()).height();
					//painter()->setPen(QPen(QColor(Qt::red)));
					//painter()->drawRect(r);
					QString s = axis.formatTickLabel(dd);
					painter()->drawText(r, Qt::AlignRight, s);
				}
				/// vytiskni carky
				painter()->setPen(styleCache().pen("graphaxis"));
				for(double dd=axis.min(); dd<=axis.max(); dd+=axis.tick()) {
					//qfInfo() << "val:" << dd << "tomm:" << axis.value2mm(dd);
					qreal pos = axis.value2pos(dd, mm2device(bounding_rect));
					Rect r = br;
					r = br.translated(br.width() + x2device(1), 0);
					r.setWidth(x2device(2));
					r.moveTop(pos);
					painter()->drawLine(r.topLeft(), r.topRight());
				}
			}
		}
		{
			Rect r = _bounding_rect;
			r.setTopRight(bounding_rect.topLeft());
			axis.setBoundingRect(r);
		}
	}
	else if(axis.direction() == Axis::DirectionX) {
		//axis.boundingRect = gridRect();
		//bnd_rect.adjust(0, bnd_rect.height(), 0, 0);
		//QVariantMap m_label = m_axis.cd("label").toMap();
		if(!axis.label().isEmpty()) {
			QString t = axis.label();
			TextStyle st = styleCache().style(axis.labelStyle());
			painter()->setFont(st.font);
			painter()->setPen(st.pen);
			painter()->setBrush(st.brush);

			Rect br = mm2device(bounding_rect);
			Rect br2;
			if(do_not_draw) br2 = painter()->boundingRect(br, Qt::AlignHCenter  | Qt::AlignBottom, t);
			else painter()->drawText(br, Qt::AlignCenter | Qt::AlignBottom, t, &br2);
			br2 = device2mm(br2);
			bounding_rect.adjust(0, 0, 0, -br2.height());
			///- axis.boundingRect.adjust(0, -br2.height(), 0, 0);
		}
		painter()->setPen(QPen());
		//QVariantMap m_ticks = m_axis.cd("ticks").toMap();
		if(axis.tick() > 0) {
			/// zjisti vysku textu hodnot
			QStringList labels;
			if(axis.valuesFrom() == Axis::TakeValues) {
				for(double dd=axis.min(); dd<=axis.max(); dd+=axis.tick()) {
					QString s = axis.formatTickLabel(dd);
					labels << s;
				}
			}
			else if(axis.valuesFrom() == Axis::TakeOrder) {
				QStringList lbls;
				foreach(QVariant v, serie.values()) {
					lbls << v.toString();
				}
				/// pokud je sorting serad labels podle setrideni
				if(serie.isSorted()) {
					for(int i=0; i<serie.values().count() && i<lbls.count(); i++) {
						labels << lbls[serie.sortedValuesIndexes().value(i)];
					}
				}
				else {
					labels = lbls;
				}
			}
			TextStyle st = styleCache().style("default");
			painter()->setFont(st.font);
			painter()->setPen(st.pen);
			painter()->setBrush(st.brush);
			double angle = 0;
			double tick_size = axis.tickSize(bounding_rect);
			QList<Rect> rects;
			QFontMetricsF fm(painter()->font());
			foreach(QString s, labels) {
				Rect r = fm.boundingRect(s);
				if(r.width() >= tick_size) angle = 45; /// kdyz se popisky nevejdou, musi se naklonit
				rects << r;
			}
			//angle = 0;
			static const double 	Pi = 3.1415926535897932384626433832795028841968;
			double height = fm.height();
			foreach(const Rect &r, rects) {
				double h = r.width()*sin(angle*Pi/180) + r.height()*cos(angle*Pi/180);
				height = qMax(h, height);
			}
			height = device2y(height);

			bounding_rect.adjust(0, 0, 0, -(height + 2)); /// mezera mezi popisky a osou
			///- axis.boundingRect.adjust(0, -(height + 2), 0, 0);
			Rect gr = mm2device(bounding_rect);
			/// vytiskni osu X
			if(!do_not_draw) {
				painter()->setPen(styleCache().pen("graphaxis"));
				painter()->drawLine(gr.bottomLeft(), gr.bottomRight());
			}
			/// vytiskni popisky
			painter()->setFont(st.font);
			painter()->setPen(st.pen);
			painter()->setBrush(st.brush);
			int i = 0;
			for(double dd=axis.min(); dd<=axis.max() && i<labels.count(); dd+=axis.tick(), i++) {
					//qfInfo() << "val:" << dd << "tomm:" << axis.value2mm(dd);
				qreal pos = axis.value2pos(dd, bounding_rect);
				//qfInfo() << "dd:" << dd << "pos:" << pos;
				Point pt(pos, bounding_rect.bottom() + 2);
				Point pt1 = mm2device(pt);
				if(angle == 0) {
					pt1 -= Point(rects[i].width() / 2, -1.5*height);
					if(!do_not_draw) painter()->drawText(pt1, labels[i]);
				}
				else {
					//Rect r = rects[i];
					if(!do_not_draw) {
						Rect r = rects[i];
						//r.moveTo(pt1);
						//r.translate(-r.width(), 0);
						//painter()->drawRect(r);

						painter()->save();
						painter()->translate(pt1);
						painter()->rotate(-angle);
						r.moveTo(-r.width(), 0);
						//painter()->setPen(QColor(Qt::green));
						//painter()->drawRect(r);
						painter()->drawText(Point(-r.width(), r.height() / 2), labels[i]);
						painter()->restore();
						//painter()->drawLine(Point(), pt1);
					}
					//pt1 += Point(rects[i].width(), 0);
				}
				//painter()->setPen(QColor(Qt::red));
				//painter()->drawRect(r);
				//if(!do_not_draw) painter()->drawText(r.topLeft(), labels[i]);

				pt = Point(pos, bounding_rect.bottom() - 1);
				pt = mm2device(pt);
				pt1 = Point(pos, bounding_rect.bottom() + 1);
				pt1 = mm2device(pt1);
				//r = Rect(pos, br.top() - Graphics::x2device(3), pos, br.top() - Graphics::x2device(1));
				if(!do_not_draw) painter()->drawLine(pt, pt1);
			}
		}
		{
			Rect r = _bounding_rect;
			r.setTopLeft(bounding_rect.bottomLeft());
			axis.setBoundingRect(r);
		}
	}
	//qfInfo() << "adding axis" << colname << axis.colname;
	axesMapRef()[colname] = axis;
}
/*
QString Graph::createLegend(const QFDomElement & el_legend)
{
	Legend legend;
	QString legend_id = el_legend.value("id");
	QVariantMap m_labels = el_legend.cd("labels").toMap();
	QStringList labels;
	QList<QColor> colors;
	//int label_no = 0;
	for(QVariantMap m_label=el_labels.firstChildElement("label"); !!el_label; el_label = el_label.nextSiblingElement("label")) {
		QString serie_name = el_label.value("serie");
		QString text = el_label.text();
		if(serie_name.isEmpty()) {
			QString caption_serie_name = el_label.value("captionForEachInSerie");
			QString value_serie_name = el_label.value("valuesFromSerie");
			if(!caption_serie_name.isEmpty()) {
				Serie caption_serie = seriesMap().value(caption_serie_name);
				Serie value_serie = seriesMap().value(value_serie_name);
				/// legenda je z nazvu serie a hodnoty (napr. pie graph)
				int ix = 0;
				foreach(QVariant v, caption_serie.values()) {
					QString s = text;
					s.replace("CAPTION", v.toString());
					if(!value_serie.isNull()) {
						QVariant v = value_serie.values().value(ix);
						s.replace("VALUE", v.toString());
					}
					labels << s;
					colors << colorForIndex(ix);
					ix++;
				}
			}
		}
		else {
			Serie serie = seriesMap().value(serie_name);
			Axis axis = axisForSerie(serie_name);
			QString s = text;
			s.replace("AXIS_LABEL", axis.label());
			labels << s;
			colors << serie.color();
		}
	}
	legend.setColors(colors);
	legend.setLabels(labels);
	d->legendMap[legend_id] = legend;
	return legend_id;
}
*/
QString Graph::drawLegend(const QVariantMap& m_legend, const Rect & _bounding_rect, bool do_not_draw)
{
	QStringList labels;
	QList<QColor> colors;
	Serie caption_serie;
	//bool legend_from_series = false;
	QString legend_id = m_legend.value("id").toString();
	Legend legend;
	{
		QVariantList v_labels = m_legend.value("labels").toList();
		for(auto v : v_labels) {
			QVariantMap m_label = v.toMap();
			QString serie_name = m_label.value("serie").toString();
			QString text = m_label.value("text").toString();
			if(serie_name.isEmpty()) {
				QString caption_serie_name = m_label.value("captionForEachInSerie").toString();
				QString value_serie_name = m_label.value("valuesFromSerie").toString();
				QString value_format = m_label.value("valueFormat").toString();
				if(!caption_serie_name.isEmpty()) {
					caption_serie = seriesMap().value(caption_serie_name);
					Serie value_serie = seriesMap().value(value_serie_name);
					/// legenda je z nazvu serie a hodnoty (napr. pie graph)
					int i = 0;
					foreach(QVariant v, caption_serie.values()) {
						QString s = text;
						s.replace("${CAPTION}", v.toString());
						if(!value_serie.isNull()) {
							QVariant v = value_serie.values().value(i);
							QString val_str = v.toString();
							if(v.type() == QVariant::Double)
								val_str = qfc::String::number(v.toDouble(), value_format);
							else if(v.type() == QVariant::Int)
								val_str = qfc::String::number(v.toInt(), value_format);
							s.replace("${VALUE}", val_str);
						}
						labels << s;
						QColor c = colorForIndex(i);
						//qfInfo() << c.name();
						colors << c;
						i++;
					}
				}
			}
			else {
				//legend_from_series = true;
				Serie serie = seriesMap().value(serie_name);
				Axis axis = axisForSerie(serie_name);
				QString s = text;
				s.replace("${AXIS_LABEL}", axis.label());
				labels << s;
				colors << serie.color();
			}
		}
	}
	Rect bounding_rect = _bounding_rect;
	/// zmensi o ramecek
	bounding_rect.adjust(2, 2, -2, -2);
	//qfInfo() << "*** bounding_rect" << bounding_rect.toString();
	//if(bounding_rect.isNull()) bounding_rect = legend.boundingRect();
	QPainter *p = painter();
	p->setFont(styleCache().font("default"));
	QFontMetricsF fm(p->font());
	Rect legend_br;
	Rect br1 = mm2device(bounding_rect);
	//qfInfo() << "\t br1:" << br1.toString();
	int box_h = (int)fm.height() / 2;
	br1.adjust(2*box_h, 0, 0, 0);
	for(int i=0; i<labels.count(); i++) {
		int ix = i;
		if(!caption_serie.isNull() && caption_serie.isSorted()) {
			ix = caption_serie.sortedValuesIndexes().value(i);
		}
		QString s = labels[ix];
		Rect r;
		//qfInfo() << "\t br1:" << br1.toString() << "label:" << s;
		if(do_not_draw) r = fm.boundingRect(br1, Qt::AlignLeft, s);
		else p->drawText(br1, Qt::AlignLeft, s, &r);
		//qfInfo() << "r:" << r.toString();
		if(legend_br.isNull()) legend_br = r;
		else legend_br = legend_br.united(r);
		/// vytiskni ctverecek
		Rect box_r = r;
		box_r.translate(-2 * box_h, box_h / 2);
		box_r.setWidth(box_h);
		box_r.setHeight(box_h);
		if(!do_not_draw) {
			//qfInfo() << "colors.count():" << colors.count();
			//qfInfo() << "ix:" << ix << colors.value(ix).name();
			//p->fillRect(box_r, colors.value(ix));
			p->setBrush(colors.value(ix));
			p->drawRect(box_r);
		}
		br1.translate(0, r.height());
	}
	legend_br.adjust(-2 * box_h, 0, 0, 0);
	//p->drawRect(legend_br);
	//br1 = mm2device(bounding_rect);
	legend_br = device2mm(legend_br);
	/// zvets o ramecek
	legend_br.adjust(-2, -2, 2, 2);
	/// legendu vycentrujeme vertikalne
	//double legend_y_offset = (br1.height() - legend_br.height()) / 2;
	//if(legend_y_offset < 0) legend_y_offset = 0;
	//legend_br.translate(0, legend_y_offset);
	/// a zarovname doleva
	//legend_br.translate(legend_br.width() - br1.width(), 0);
	{
		//qfInfo() << "drawLegend:" << legend_br.toString();
		legend.setBoundingRect(legend_br);
		d->legendMap[legend_id] = legend;
	}
	return legend_id;
}

QColor Graph::colorForIndex(int ix)
{
	QColor ret;
	if(colorNamesPull.isEmpty()) {
		colorNamesPull
				<< "lightskyblue"
				<< "gold"
				<< "plum"
				<< "orange"
				<< "burlywood"
				<< "coral"
				<< "navajowhite"
				<< "olive"
				<< "chartreuse"
				<< "aquamarine"
				<< "peru"
				<< "royalblue"
				<< "silver"
				<< "red"
				<< "blue"
				<< "green"
				<< "violet";
	}
	if(ix < colorNamesPull.count()) {
		ret = QColor(colorNamesPull.value(ix));
	}
	else {
		int h = (ix * 60) % 360; /// na hrubo po 60 stupnich
		ix %= 60;
		h += (ix * 15) % 60;
		ix %= 15;
		h += ix;
		ret.setHsv(h, 128, 255);
	}
	return ret;
}

Graph* Graph::createGraph(const QVariantMap &m_def, const qfu::TreeTable &data)
{
	QString type = m_def.value("type").toString();
	if(type == "histogram")
		return new HistogramGraph(m_def, data);
	if(type == "pie")
		return new PieGraph(m_def, data);
	qfWarning() << "Unsupported graph type:" << type;
	return NULL;
}

//======================================================
//                    HistogramGraph
//======================================================
void HistogramGraph::drawSeries()
{
	qfLogFuncFrame() << "boundingRect width:" << boundingRect().width() << "boundingRect height:" << boundingRect().height();
	//return;
	QPainter *p = painter();
	p->setPen(styleCache().pen("graphaxis"));

	QVariantList v_series = definition().value("series").toList();
	for(auto v : v_series) {
		/// najdi osu X
		QVariantMap m_serie_x = v.toMap();
		QString colname_x = m_serie_x.value("colname").toString();
		Axis axis_x = axisForSerie(colname_x);
		Serie serie_x = seriesMap().value(colname_x);
		if(axis_x.direction() == Axis::DirectionX) {
			for(auto v : v_series) {
				QVariantMap m_serie_y = v.toMap();
				QString colname_y = m_serie_y.value("colname").toString();
				//qfInfo() << "colname_y:" << colname_y;
				Axis axis_y = axisForSerie(colname_y);
				Serie serie_y = seriesMap().value(colname_y);
				/// najdi osy Y
				if(axis_y.direction() == Axis::DirectionY) {
					QColor color = serie_y.color();
					//qfInfo() << colname_y << "color:" << color.name();
					for(int i=0; i<serie_y.values().count() && i<serie_y.values().count(); i++) {
						//qfDebug() << axis_y.colname << "value:" << i << axis_y.values[i];
						//qfDebug() << "grid rect:" << gridRect().toString();
						int ix = i;
						if(serie_x.isSorted()) ix = serie_x.sortedValuesIndexes()[i];
						QVariant v_y = serie_y.values()[ix];
						double dd = v_y.toDouble();
						double x1 = axis_x.value2pos(i - 0.25, gridRect());
						double y1 = axis_y.value2pos(dd, gridRect());
						double x2 = axis_x.value2pos(i + 0.25, gridRect());
						double y2 = axis_y.value2pos(axis_y.min(), gridRect());
						Rect r(x1, y1, x2-x1, y2-y1);
						qfDebug() << "\t rect:" << r.toString();
						r = mm2device(r);
						//p->fillRect(r, QBrush(color));
						p->setBrush(QBrush(color));
						//p->setBrush(QBrush(QColor(255, 0, 0, 127)));
						p->setPen(QPen());
						//qfInfo() << "brush color:" << QString::number(p->brush().color().rgba(), 16);
						p->drawRect(r);
						//break;
					}
				}
			}
			break;
		}
	}
}

//======================================================
//                PieGraph
//======================================================
void PieGraph::drawSeries()
{
	qfLogFuncFrame() << "boundingRect width:" << boundingRect().width() << "boundingRect height:" << boundingRect().height();
	//return;
	double h3d = definition().value("h3d").toDouble();
	QVariantList v_series = definition().value("series").toList();
	for(auto v : v_series) {
		QVariantMap m_serie_x = v.toMap();
		QString colname_x = m_serie_x.value("colname").toString();
		Axis axis_x = axisForSerie(colname_x);
		if(axis_x.direction() == Axis::DirectionX) {
			Serie serie_x = seriesMap().value(colname_x);
			for(auto v : v_series) {
				QVariantMap m_serie_y = v.toMap();
				QString colname_y = m_serie_y.value("colname").toString();
				Axis axis_y = axisForSerie(colname_y);
				if(axis_y.direction() == Axis::DirectionY) {
					Serie serie_y = seriesMap().value(colname_y);

					QPainter *p = painter();

					/// udelej ctverec
					Rect top_gr = gridRect();
					top_gr.adjust(0, 0, 0, -h3d);
					Rect h3d_gr = top_gr;
					h3d_gr.translate(0, h3d);
					//gr.setWidth(qMin(gr.width(), gr.height()));
					//gr.setHeight(qMin(gr.width(), gr.height()));
					/// vycentruj ho
					//gr.translate((gridRect().width()-gr.width()) / 2, (gridRect().height()-gr.height()) / 2);
					top_gr = mm2device(top_gr);
					h3d_gr = mm2device(h3d_gr);

					double sum = 0;
					foreach(QVariant v, serie_y.values()) sum += v.toDouble();
					if(sum > 0) {
						painter()->setPen(styleCache().pen("graphaxis"));
						if(h3d > 0) {
						/// spodni graf
							Rect gr = h3d_gr;
							qreal start = 0;
							double top_main_axis_y = top_gr.top() + top_gr.height() / 2;
							Point last_point = top_gr.topLeft() + QPointF(0, top_main_axis_y);
							for(int i=0; i<serie_x.values().count() && i<serie_y.values().count(); i++) {
								int ix = i;
								if(serie_x.isSorted()) ix = serie_x.sortedValuesIndexes()[i];
								QVariant v_y = serie_y.values()[ix];
								double y = v_y.toDouble();
								y = 360 * y / sum;
								qreal span;
								if(i == serie_y.values().count()-1) span = 360 - start;
								else span = (int)y;
								if(span == 0) continue; /// nulove vysece nema cenu tisknout, jenom to dela zbytecny chyby pri kresleni
								if(start + span > 180) {
									qreal st = start;
									qreal sp = span;
									if(start < 180) {
										st = 180;
										sp -= 180 - start;
									}
									QPainterPath pp(last_point);
									pp.arcTo(gr, st, sp);
									last_point = pp.pointAtPercent(1);
									pp.lineTo(QPointF(last_point.x(), top_main_axis_y));
									pp.lineTo(QPointF(pp.pointAtPercent(0).x(), top_main_axis_y));
									pp.lineTo(pp.pointAtPercent(0));
									pp.closeSubpath();
									p->setBrush(colorForIndex(ix));
									p->drawPath(pp);
								//break;
								}
								start += span;
							}
						/// obdelniky
						}
						{
						/// horni graf
							Rect gr = top_gr;
							qreal start = 0;
							for(int i=0; i<serie_x.values().count() && i<serie_y.values().count(); i++) {
							//qfInfo() << "grid rect:" << gridRect().toString();
							//qfInfo() << "x:" << axis_x.values[i].toString() << "y:" << axis_y.values[i].toString();
							//qfInfo() << "x:" << axis_x.value2pos(axis_x.values[i].toDouble() - 0.5, gridRect()) << "-" << axis_x.value2pos(axis_x.values[i].toDouble() + 0.5, gridRect());
							//qfInfo() << "y:" << axis_y.value2pos(axis_y.min, gridRect()) << "-" << axis_y.value2pos(axis_y.values[i].toDouble(), gridRect());
								int ix = i;
								if(serie_x.isSorted()) ix = serie_x.sortedValuesIndexes()[i];
								QVariant v_y = serie_y.values()[ix];
								double y = v_y.toDouble();
								y = 1 * 360 * y / sum;
							//qfInfo() << "rect:" << r.toString();
								p->setBrush(colorForIndex(ix));
								qreal span;
								if(i == serie_y.values().count()-1) span = 1 * 360 - start;
								else span = (int)y;
								if(span == 0) continue; /// nulove vysece nema cenu tisknout, jenom to dela zbytecny chyby pri kresleni
								{
									QPainterPath pp(gr.center());
									pp.arcTo(gr, start, span);
									pp.closeSubpath();
									p->setBrush(colorForIndex(ix));
									p->drawPath(pp);
								}
								start += span;
							}
						}
					}
				}
			}
			break;
		}
	}
}












