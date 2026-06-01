#include "reportitemframe.h"

#include "reportitemband.h"
#include "reportpainter.h"
#include "reportprocessor.h"

#include <qf/core/log.h>
#include <qf/core/assert.h>
#include <qf/core/string.h>

//#define QF_TIMESCOPE_ENABLED
#include <qf/core/utils/timescope.h>

namespace qfc = qf::core;
using namespace qf::gui::reports;

//==========================================================
//                      ReportItemFrame
//==========================================================
ReportItemFrame::ReportItemFrame(ReportItem *parent)
	: Super(parent)
{
	qfLogFuncFrame();
	m_layout = LayoutVertical;
	m_expandChildFrames = false;
	m_horizontalAlignment = AlignLeft;
	m_verticalAlignment = AlignTop;

	m_indexToPrint = 0;
}

ReportItemFrame::~ReportItemFrame()
{
	qfLogFuncFrame();
}

style::Pen *ReportItemFrame::border() const
{
	style::Pen *ret = nullptr;
	if(leftBorder() == rightBorder() && rightBorder() == topBorder() && topBorder() == bottomBorder()) {
		ret = leftBorder();
	}
	return ret;
}

void ReportItemFrame::setBorder(style::Pen *b)
{
	if(b != border()) {
		setLeftBorder(b);
		setRightBorder(b);
		setTopBorder(b);
		setBottomBorder(b);
		if(b && !b->parent())
			b->setParent(this);
		emit borderChanged(b);
	}
}

void ReportItemFrame::componentComplete()
{
	initDesignedRect();
}

void ReportItemFrame::initDesignedRect()
{
	qfLogFuncFrame();
	//QF_TIME_SCOPE("ReportItemFrame::initDesignedRect");
	designedRect = Rect();
	{
		QVariant v = width();
		qreal d = 0;
		bool ok;
#if QT_VERSION_MAJOR >= 6
		if(v.typeId() == QMetaType::QString) {
#else
		if(v.type() == QVariant::String) {
#endif
			qfc::String s;
			s = v.toString().trimmed();
			{
				if(s.value(-1) == '%') {
					s = s.slice(0, -1).trimmed();
					designedRect.horizontalUnit = Rect::UnitPercent;
				}
				if(!s.isEmpty()) {
					d = s.toDouble(&ok);
					if(!ok)
						qfWarning() << "Cannot convert" << s << "to real number." << this;
				}
			}
		}
		else if(v.isValid()) {
			d = v.toReal(&ok);
			if(!ok)
				qfWarning() << "Cannot convert" << v.toString() << "to real number." << this;
		}
		designedRect.setWidth(d);
	}
	{
		QVariant v = height();
		qreal d = 0;
		bool ok;
#if QT_VERSION_MAJOR >= 6
		if(v.typeId() == QMetaType::QString) {
#else
		if(v.type() == QVariant::String) {
#endif
			qfc::String s;
			s = v.toString().trimmed();
			{
				if(s.value(-1) == '%') {
					s = s.slice(0, -1).trimmed();
					designedRect.verticalUnit = Rect::UnitPercent;
				}
				if(!s.isEmpty()) {
					d = s.toDouble(&ok);
					if(!ok)
						qfWarning() << "Cannot convert" << s << "to real number." << this;
				}
			}
		}
		else if(v.isValid()) {
			d = v.toReal(&ok);
			if(!ok)
				qfWarning() << "Cannot convert" << v.toString() << "to real number." << this;
		}
		designedRect.setHeight(d);
	}

	if(isExpandChildFrames()) {
		designedRect.flags |= Rect::ExpandChildFrames;
	}

	if(layout() == LayoutHorizontal)
		designedRect.flags |= Rect::LayoutHorizontalFlag;
	else
		designedRect.flags |= Rect::LayoutVerticalFlag;

	qfDebug() << "\tdesignedRect:" << designedRect.toString();
}

QString ReportItemFrame::toString(int indent, int indent_offset)
{
	QString ret = Super::toString(indent, indent_offset);
	for(int i=0; i<itemCount(); i++) {
		ret += '\n';
		ReportItem *it = itemAt(i);
		ret += it->toString(indent, indent_offset += indent);
	}
	return ret;
}

QQmlListProperty<ReportItem> ReportItemFrame::items()
{
	return QQmlListProperty<ReportItem>(this, nullptr,
										ReportItemFrame::addItemFunction,
										ReportItemFrame::countItemsFunction,
										ReportItemFrame::itemAtFunction,
										ReportItemFrame::removeAllItemsFunction
										);
}

void ReportItemFrame::insertItem(int ix, QObject *item_object)
{
	auto *item = qobject_cast<ReportItem *>(item_object);
	if (item) {
		item->setParent(this);
		this->m_items.insert(ix, item);
		auto *frm = qobject_cast<ReportItemFrame*>(item);
		if(frm)
			frm->initDesignedRect();
	}
}

void ReportItemFrame::addItem(QObject *item_object)
{
	insertItem(m_items.count(), item_object);
}

void ReportItemFrame::addItemFunction(QQmlListProperty<ReportItem> *list_property, ReportItem *item)
{
	if (item) {
		auto *that = static_cast<ReportItemFrame*>(list_property->object);
		item->setParent(that);
		that->m_items << item;
	}
}

ReportItem *ReportItemFrame::itemAtFunction(QQmlListProperty<ReportItem> *list_property, WidgetIndexType index)
{
	auto *that = static_cast<ReportItemFrame*>(list_property->object);
	return that->m_items.value(index);
}

void ReportItemFrame::removeAllItemsFunction(QQmlListProperty<ReportItem> *list_property)
{
	auto *that = static_cast<ReportItemFrame*>(list_property->object);
	qDeleteAll(that->m_items);
	that->m_items.clear();
}

ReportItemFrame::WidgetIndexType ReportItemFrame::countItemsFunction(QQmlListProperty<ReportItem> *list_property)
{
	auto *that = static_cast<ReportItemFrame*>(list_property->object);
	return that->itemCount();
}

int ReportItemFrame::itemCount() const
{
	return m_items.count();
}

ReportItem *ReportItemFrame::itemAt(int index)
{
	return m_items.value(index);
}

ReportItem::ChildSize ReportItemFrame::childSize(Layout parent_layout)
{
	if(parent_layout == LayoutHorizontal)
		return ChildSize(designedRect.width(), designedRect.horizontalUnit);
	return ChildSize(designedRect.height(), designedRect.verticalUnit);
}

ReportItem::PrintResult ReportItemFrame::printMetaPaintChildren(QPaintDevice *paint_device, ReportItemMetaPaint *out, const ReportItem::Rect &bounding_rect)
{
	qfLogFuncFrame();// << element.tagName() << "id:" << element.attribute("id") << "itemCount:" << itemsToPrintCount() << "indexToPrint:" << indexToPrint;
	qfDebug() << "\tbounding_rect:" << bounding_rect.toString();
	//N n;
	//QF_TIME_SCOPE(QString::number((int)n) + ": ReportItemFrame::printMetaPaintChildren");
	PrintResult res = PrintResult::createPrintFinished();
	Rect paint_area_rect = bounding_rect;
	if(layout() == LayoutStacked) {
		/// allways print all the children) in the stacked layout
		/// it is used mainly for page header & footers, they shoud be on each page
		//Rect rendered_rect;
		for(m_indexToPrint=0; m_indexToPrint<itemsToPrintCount(); m_indexToPrint++) {
			ReportItem *it = itemToPrintAt(m_indexToPrint);
			Rect children_paint_area_rect = paint_area_rect;
			ChildSize sz = it->childSize(LayoutVertical);
			children_paint_area_rect.setHeight(sz.fromParentSize(paint_area_rect.height()));
			PrintResult ch_res = it->printMetaPaint(paint_device, out, children_paint_area_rect);
			if(!ch_res.isPrintFinished()) {
				if(res.isPrintFinished()) {
					/// only one child can be printed again
					/// others are ignored in ch_res flags
					res = ch_res;
				}
			}
			else {
				// print item again on new page
				it->resetIndexToPrintRecursively(true);
			}
			//qfInfo() << indexToPrint << "ch res:" << ch_res.toString() << "res:" << res.toString();
		}
	}
	else if(layout() == LayoutHorizontal) {
		/// Break is ignored in horizontal layout
		QList<ChildSize> layout_sizes;
		m_indexToPrint = 0; /// allways print from 0 index (all the children) in horizontal layout
		/// horizontalni layout musi mit procenta rozpocitany dopredu, protoze jinak by se mi nezalamovaly texty v tabulkach
		{
			/// get layout sizes in the layout direction
			for(int i=m_indexToPrint; i<itemsToPrintCount(); i++) {
				ReportItem *it = itemToPrintAt(i);
				layout_sizes << it->childSize(layout());
			}
		}

		/// get layout sizes in the orthogonal layout direction
		/// je to bud absolutni hodnota nebo % z bbr
		QList<ChildSize> orthogonal_sizes;
		for(int i=m_indexToPrint; i<itemsToPrintCount(); i++) {
			ReportItem *it = itemToPrintAt(i);
			Layout ol = orthogonalLayout();
			ChildSize sz = it->childSize(ol);
			if(sz.unit == Rect::UnitPercent) {
				sz.size = paint_area_rect.sizeInLayout(ol); /// udelej z nej rubber, roztahne se dodatecne
			}
			orthogonal_sizes << sz;
		}

		/// Items should be printed in special order to make proper layout of all the kinds of size specifiers (xx, undefined, "xx%")
		/// in horizontal layout, print fixed mm size width items first, the rubber (without width specified) ones as second, rest of space divide according to % value
		/// when printed, rearange printed items to their original layout order

		QMap<int, int> layout_ix_to_print_ix; /// layout_ix->print_ix
		qreal sum_mm = 0;
		bool has_percent = false;
		/// print rubber and fixed
		for(int i=0; i<itemsToPrintCount(); i++) {
			ReportItem *it = itemToPrintAt(i);
			ChildSize sz = layout_sizes.value(i);
			//qfInfo() << "child:" << i << "size:" << sz.size << "unit:" << Rect::unitToString(sz.unit);
			if(sz.unit == Rect::UnitMM) {
				Rect ch_bbr = paint_area_rect;
				ch_bbr.setLeft(paint_area_rect.left() + sum_mm);
				if(sz.size > 0)
					ch_bbr.setWidth(sz.size);
				else
					ch_bbr.setWidth(paint_area_rect.width() - sum_mm);
				if(orthogonal_sizes[i].size > 0) {
					ch_bbr.setSizeInLayout(orthogonal_sizes[i].size, orthogonalLayout());
				}
				//qfInfo() << "\t tisknu fixed:" << it->designedRect.toString();
				int prev_children_cnt = out->childrenCount();
				PrintResult ch_res = it->printMetaPaint(paint_device, out, ch_bbr);
				if(out->children().count() > prev_children_cnt) {
					//qfInfo() << "rubber fixed:" << i << "->" << prev_children_cnt;
					layout_ix_to_print_ix[i] = prev_children_cnt;
					double width = out->lastChild()->renderedRect.width();
					sum_mm += width;
					//qfInfo() << "\t sum_mm:" << sum_mm;
					if(!ch_res.isPrintFinished())
						res = ch_res;
					/*
					if(!ch_res.isPrintAgain()) {
						/// para can be printed as NotFit if it owerflows its parent frame
						res = ch_res;
					}
					*/
				}
				else {
					if(ch_res.isPrintFinished()) {
						if(it->isVisible()) {
							qfWarning() << "jak to, ze se dite nevytisklo v horizontalnim layoutu?" << it;
						}
					}
					else {
						//qfInfo() << "\t NOT OK";
						res = ch_res;
						break;
					}
				}
			}
			else {
				has_percent = true;
			}
		}
		qreal rest_mm = paint_area_rect.width() - sum_mm;

		if(res.isPrintFinished()) {
			if(has_percent) {
				/// divide rest of space to xx% items
				qreal sum_percent = 0;
				int cnt_0_percent = 0;
				for(int i=0; i<itemsToPrintCount(); i++) {
					ReportItem *it = itemToPrintAt(i);
					ChildSize sz = it->childSize(layout());
					if(sz.unit == Rect::UnitPercent) {
						if(sz.size == 0)
							cnt_0_percent++;
						else
							sum_percent += sz.size;
					}
				}
				if(rest_mm <= 0) {
					qfWarning() << "Percent exist but rest_mm is" << rest_mm << ". Ignoring rest of frames";
				}
				else {
					/// print percent items
					qreal percent_0 = 0;
					if(cnt_0_percent > 0)
						percent_0 = (100 - sum_percent) / cnt_0_percent;
					for(int i=0; i<itemsToPrintCount(); i++) {
						ReportItem *it = itemToPrintAt(i);
						ChildSize sz = it->childSize(layout());
						if(sz.unit == Rect::UnitPercent) {
							qreal d;
							if(sz.size == 0)
								d = rest_mm * percent_0 / 100;
							else
								d = rest_mm * sz.size / 100;
							//qfInfo() << d;
							Rect ch_bbr = paint_area_rect;
							ch_bbr.setWidth(d);
							if(orthogonal_sizes[i].size > 0) {
								ch_bbr.setSizeInLayout(orthogonal_sizes[i].size, orthogonalLayout());
							}
							//qfInfo() << it << "tisknu percent" << it->designedRect.toString();
							//qfInfo() << "chbr" << ch_bbr.toString();
							int prev_children_cnt = out->childrenCount();
							PrintResult ch_res = it->printMetaPaint(paint_device, out, ch_bbr);
							if(out->children().count() > prev_children_cnt) {
								//qfInfo() << "percent:" << i << "->" << prev_children_cnt;
								layout_ix_to_print_ix[i] = prev_children_cnt;
								// chybejici vykricnik v nasl. podmince zpusoboval issue
								// chyba zalomeni pro dlouha jmena v tisku startovek pro startéry #264
								// pozor, stejna podminka je i v "print rubber and fixed" sekci, asi by to chtelo sjednotit
								//if(ch_res.isPrintAgain()) {
								//	/// para se muze vytisknout a pritom bejt not fit, pokud pretece
								//	res = ch_res;
								//}
								if(!ch_res.isPrintFinished())
									res = ch_res;
							}
							else {
								if(ch_res.isPrintFinished()) {
									/// jediny, kdo se nemusi vytisknout je band
									if(it->isVisible()) {
										qfWarning() << "jak to, ze se dite nevytisklo v horizontalnim layoutu?" << it;
									}
								}
								else {
									res = ch_res;
									break;
								}
							}
						}
					}
				}
				/// arrange printed children to their original order
				//qfInfo() << "\t poradi tisku cnt:<<" << poradi_tisku.count() << out->childrenCount();
				if(layout_ix_to_print_ix.count() == out->children().count()) {
					int children_count = out->children().count();
					//qfInfo() << "children cnt:" << children_count;
					//QF_ASSERT(poradi_tisku.count() == out->children().count(), "nevytiskly se vsechny deti v horizontalnim layoutu");
					QVector<qf::core::utils::TreeItemBase*> old_children(layout_ix_to_print_ix.count());
					/// get printed children pointers
					for(int i=0; i<children_count; i++)
						old_children[i] = out->children()[i];
					/// arrange them to the original order
					/// v mape nemusi byt rada klicu souvisla (kdyz se nejake dite nevytiskne)
					QMapIterator<int, int> iter(layout_ix_to_print_ix);
					int new_print_ix = 0;
					while(iter.hasNext()) {
						iter.next();
						int old_print_ix = iter.value();
						if(0 <= new_print_ix && new_print_ix < children_count) {
							//qfInfo() << old_print_ix << "->" << new_print_ix;
							if(new_print_ix != old_print_ix)
								out->childrenRef()[new_print_ix] = old_children[old_print_ix];
							new_print_ix++;
						}
						else
							qfWarning() << QF_FUNC_NAME << "order:" << old_print_ix << "new_ix:" << new_print_ix << "out of range:" << children_count;
					}

					/// set children proper offsets
					qreal offset_x = 0;
					for(int i=0; i<layout_ix_to_print_ix.count(); i++) {
						//qfInfo() << "\t poradi tisku <<" << i << "offset:" << offset_x;
						ReportItemMetaPaint *it = out->child(i);
						/// shift them including their own children recursively :(
						double shift_x = paint_area_rect.left() + offset_x - it->renderedRect.left();
						//if(parent_grid) qfInfo() << i << "offset_x:" << offset_x << "bbr left:" << bbr.left() << "chbbr left:" << ch_bbr.left();
						if(qFloatDistance(shift_x, 0) > 200)
							it->shift(Point(shift_x, 0));
						offset_x += it->renderedRect.width();
					}
				}
				else {
					qfWarning() << this << "nesedi poradi pocty tisku" << layout_ix_to_print_ix.count() << out->children().count();
				}
			}
		}
		if(res.isPrintAgain()) {
			/// detail by mel mit, pokud se ma zalamovat, vzdy vertikalni layout, jinak tato funkce zpusobi, ze se po zalomeni vsechny dcerine bandy budou tisknout cele znova
			/// zakomentoval jsem to a zda se, ze to zatim nicemu nevadi
			//resetIndexToPrintRecursively(!ReportItem::IncludingParaTexts);
		}
	}
	else {
		/// vertical layout
		/// print % like a rubber dimension and if print result is PrintOk resize rendered rect of printed metapaint item
		/// break funguje tak, ze pri 1., 3., 5. atd. tisku vraci PrintNotFit a pri sudych PrintOk
		/// prvni break na strance znamena, ze jsem tu uz po zalomeni, takze se tiskne to za break.
		//int index_to_print_0 = m_indexToPrint;
		for(; m_indexToPrint<itemsToPrintCount(); m_indexToPrint++) {
			ReportItem *child_item_to_print = itemToPrintAt(m_indexToPrint);
			Rect children_paint_area_rect = paint_area_rect;
			qfDebug() << "\tch_bbr v1:" << children_paint_area_rect.toString();

			{
				/// find child paint area size in layout direction
				qreal d = children_paint_area_rect.sizeInLayout(layout());
				ChildSize sz = child_item_to_print->childSize(layout());
				//qfInfo() << it << "chbrd:" << ch_bbr.toString() << "d:" << d;// << "size in ly:" << sz.fillLayoutRatio();
				if(sz.fillLayoutRatio() >= 0) {
				}
				else if(sz.unit == Rect::UnitMM) {
					if(sz.size > 0)
						d = sz.size;
				}
				else {
					auto *frit = qobject_cast<ReportItemFrame*>(child_item_to_print);
					if(frit)
						qfWarning() << "This should never happen" << child_item_to_print;
				}
				//qfInfo() << "\t ch_bbr.sizeInLayout(layout():" << ch_bbr.sizeInLayout(layout()) << "d:" << d;
				d = qMin(children_paint_area_rect.sizeInLayout(layout()), d);
				children_paint_area_rect.setSizeInLayout(d, layout());
				//qfInfo() << "\t ch_bbr:" << ch_bbr.toString();
			}
			{
				/// orthogonal size
				Layout ol = orthogonalLayout();
				ChildSize o_sz = child_item_to_print->childSize(ol);
				if(o_sz.unit == Rect::UnitPercent) {
					if(o_sz.size == 0)
						o_sz.size = paint_area_rect.sizeInLayout(ol);
					else
						o_sz.size = o_sz.size / 100 * paint_area_rect.sizeInLayout(ol);
				}
				//it->metaPaintOrthogonalLayoutLength = sz.size;
				qfDebug() << "\tsetting orthogonal length:" << o_sz.size;
				//if(it->isBreak() && i > indexToPrint && layout == LayoutVertical) break; /// v horizontalnim layoutu break ignoruj
				if(o_sz.size > 0) {
					children_paint_area_rect.setSizeInLayout(o_sz.size, orthogonalLayout());
				}
			}
			qfDebug() << "\tch_bbr v2:" << children_paint_area_rect.toString();
			int prev_children_cnt = out->childrenCount();
			PrintResult ch_res = child_item_to_print->printMetaPaint(paint_device, out, children_paint_area_rect);
			if(ch_res.isPrintFinished()) {
				/// muze se stat, ze se dite nevytiskne, napriklad band nema zadna data
				if(out->children().count() > prev_children_cnt) {
					ReportItemMetaPaint *mpi = out->lastChild();
					if(mpi) {
						const Rect &r = mpi->renderedRect;
						/// cut rendered area
						paint_area_rect.cutSizeInLayout(r, layout());
						if(ch_res.isNextDetailRowExists()) {
							m_indexToPrint--; /// vytiskni ho znovu
						}
					}
				}
			}
			else {
				/// pokud je vertikalni layout, a dite se nevejde vrat PrintNotFit
				res = ch_res;
				break;
			}
			//if(child_item_to_print->isBreak() && m_indexToPrint > index_to_print_0)
			//	break;
		}
	}
	//res = checkPrintResult(res);
	qfDebug() << "\t<<< CHILDREN return:" << res.toString();
	return res;
}

ReportItem::PrintResult ReportItemFrame::printMetaPaint(QPaintDevice *paint_device, ReportItemMetaPaint *out, const ReportItem::Rect &bounding_rect)
{
	qfLogFuncFrame() << this;
	qfDebug() << "\tbounding_rect:" << bounding_rect.toString();
	qfDebug() << "\tdesignedRect:" << designedRect.toString();// << "isLeftTopFloating:" << isLeftTopFloating() << "isRightBottomFloating:" << isRightBottomFloating();
	qfDebug() << "\tlayout:" << ((layout() == LayoutHorizontal)? "horizontal": "vertical") << ", is rubber:" << isRubber(layout());
	QF_TIME_SCOPE("ReportItemFrame::printMetaPaint ***");
	PrintResult res = PrintResult::createPrintFinished();
	if(!isVisible())
		return res;
	Rect frame_content_br = bounding_rect;
	qfDebug() << "\tbbr 0:" << frame_content_br.toString();
	if(designedRect.horizontalUnit == Rect::UnitMM && designedRect.width() - Epsilon > bounding_rect.width()) {
		qfDebug() << "\t<<<< FRAME NOT FIT WIDTH";
		return checkPrintResult(PrintResult::createPrintAgain());
	}
	if(designedRect.verticalUnit == Rect::UnitMM && designedRect.height() - Epsilon > bounding_rect.height()) {
		qfDebug() << "\t<<<< FRAME NOT FIT HEIGHT";
		return checkPrintResult(PrintResult::createPrintAgain());
	}
	frame_content_br.adjust(hinset(), vinset(), -hinset(), -vinset());

	QList<double> column_sizes;
	double columns_gap = 0;
	if(column_sizes.isEmpty()) {
		qf::core::String s = columns();
		QStringList sl = s.splitAndTrim(',');
		columns_gap = columnsGap();
		double ly_size = frame_content_br.width() - (columns_gap * (sl.count() - 1));
		column_sizes = qf::gui::graphics::makeLayoutSizes(sl, ly_size);
	}
	auto *metapaint_frame = new ReportItemMetaPaintFrame(out, this);
	QF_ASSERT_EX(metapaint_frame != nullptr, "Meta paint item for item " + QString(this->metaObject()->className()) + " not created.");
	metapaint_frame->setInset(hinset(), vinset());
	metapaint_frame->setLayout((qf::gui::graphics::Layout)layout());
	metapaint_frame->setAlignment(horizontalAlignment(), verticalAlignment());
	Rect column_br_helper = frame_content_br;
	for(int current_column_index=0; current_column_index<column_sizes.count(); current_column_index++) {
		Rect column_br = column_br_helper;
		column_br.setWidth(column_sizes.value(current_column_index));
		//if(column_sizes.count() > 1) {
		//	qfInfo() << current_column_index << "column bounding rect:" << column_br.toString();
		//}

		res = printMetaPaintChildren(paint_device, metapaint_frame, column_br);

		if(res.isPrintAgain()) {
			if(res.isPageBreak()) {
				current_column_index = column_sizes.count();
			}
			if(!canBreak()) {
				// always delete current metapaint item when it cannot be broken
				// when keepWithPrev is true, delete also metapaint item for all metapaint items in it's keepWithPrev chain
				ReportItemFrame *parent_frame = this->parentFrame();
				if(!parent_frame) {
					qfWarning() << "Parent frame shall not be NULL!";
				}
				else {
					QF_CHECK(parent_frame->itemToPrintAt(parent_frame->m_indexToPrint) == this, "Internal error!");
					while(true) {
						ReportItem *it = parent_frame->itemToPrintAt(parent_frame->m_indexToPrint);
						QF_ASSERT(it != nullptr, "Internal error!", break);
						//qfWarning() << "reseting index of:" << it;
						it->resetIndexToPrintRecursively(ReportItem::IncludingParaTexts);
						ReportItemMetaPaint *mpit = out->lastChild();
						QF_ASSERT(mpit != nullptr, "Cannot delete NULL metapaint item!", break);
						QF_SAFE_DELETE(mpit);
						if(it->isKeepWithPrev()) {
							if(parent_frame->m_indexToPrint > 0) {
								parent_frame->m_indexToPrint--;
								//qfWarning() << "new m_indexToPrint:" << parent_frame->m_indexToPrint;
								continue;
							}
															qfWarning() << "Index to print == 0: Internal error!";

						}
						break;
					}
				}
				/*
				resetIndexToPrintRecursively(ReportItem::IncludingParaTexts);
				QF_SAFE_DELETE(mp);
				*/
				return checkPrintResult(res);
			}
		}
		//column_br_helper = column_br;
		column_br_helper.moveLeft(column_br.right() + columns_gap);
	}

	/// set dirty_rect to painted area
	Rect dirty_rect;
	dirty_rect.flags = designedRect.flags;
	{
		/// musim to proste secist
		for(int i=0; i<metapaint_frame->childrenCount(); i++) {
			ReportItemMetaPaint *it = metapaint_frame->child(i);
			if(dirty_rect.isNull())
				dirty_rect = it->renderedRect;
			else
				dirty_rect = dirty_rect.united(it->renderedRect);
		}
		qfDebug() << "\trubber dirty_rect:" << dirty_rect.toString();
	}
	/// pokud je v nekterem smeru definovany, je jedno, kolik se toho potisklo a nastav ten rozmer
	if(designedRect.horizontalUnit == Rect::UnitPercent)
		dirty_rect.setWidth(frame_content_br.width()); /// horizontalni rozmer musi ctit procenta
	else if(designedRect.horizontalUnit == Rect::UnitMM && designedRect.width() > Epsilon)
		dirty_rect.setWidth(designedRect.width() - (2*hinset()));
	if(designedRect.verticalUnit == Rect::UnitMM && designedRect.height() > Epsilon)
		dirty_rect.setHeight(designedRect.height() - (2*vinset()));
	/// pri rendrovani se muze stat, ze dirtyRect nezacina na bbr, to ale alignment zase spravi
	dirty_rect.moveTopLeft(frame_content_br.topLeft());

	/// alignment
	//qfDebug() << "\tALIGN:" << QString::number((int)alignment, 16);
	dirty_rect.adjust(-hinset(), -vinset(), hinset(), vinset());
	metapaint_frame->renderedRect = dirty_rect;
	/// aby sly expandovat deti, musi mit parent spravne renderedRect
	{
		//qfInfo() << childSize(LayoutVertical).fillLayoutRatio();
		if(isExpandVerticalSprings()) {
			//qfInfo() << this << "rendered rect2:" << metapaint_frame->renderedRect;
			/// !!! will not work for children with more columns
			metapaint_frame->renderedRect = bounding_rect;
			metapaint_frame->expandChildVerticalSpringFrames();
		}
		//qfInfo() << "\t rendered rect2:" << mp->renderedRect.toString();
		metapaint_frame->alignChildren();
		metapaint_frame->renderedRect.flags = designedRect.flags;
		/// mohl bych to udelat tak, ze bych vsem detem dal %, ale je to moc klikani v repeditu
		if(designedRect.flags & ReportItem::Rect::ExpandChildFrames) {
			metapaint_frame->expandChildFrames();
		}
	}
	qfDebug() << "\trenderedRect:" << metapaint_frame->renderedRect.toString();
	res = checkPrintResult(res);
	setRenderedWidth(metapaint_frame->renderedRect.width());
	setRenderedHeight(metapaint_frame->renderedRect.height());
	//qfDebug().color(QFLog::Cyan) << "\t<<<< FRAME return:" << res.toString() << element.tagName() << "id:" << element.attribute("id");
	return res;
}

void ReportItemFrame::resetIndexToPrintRecursively(bool including_para_texts)
{
	//qfInfo() << "resetIndexToPrintRecursively()";
	m_indexToPrint = 0;
	for(int i=0; i<itemsToPrintCount(); i++) {
		ReportItem *it = itemToPrintAt(i);
		it->resetIndexToPrintRecursively(including_para_texts);
	}
	/*
	foreach(QObject *o, children()) {
		ReportItem *it = qobject_cast<ReportItem*>(o);
		it->resetIndexToPrintRecursively();
	}
	*/
}


