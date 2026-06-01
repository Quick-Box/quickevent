//
// Author: Frantisek Vacek <fanda.vacek@volny.cz>, (C) 2006, 2014
//
// Copyright: See COPYING file that comes with this distribution
//

#ifndef QF_GUI_REPORTS_REPORTITEMFRAME_H
#define QF_GUI_REPORTS_REPORTITEMFRAME_H

#include "reportitem.h"
#include "../../guiglobal.h"

namespace qf {
namespace gui {
namespace reports {

class QFGUI_DECL_EXPORT ReportItemFrame : public ReportItem
{
	Q_OBJECT
private:
	typedef ReportItem Super;
public:
	Q_CLASSINFO("DefaultProperty", "items")
	Q_PROPERTY(QQmlListProperty<qf::gui::reports::ReportItem> items READ items)
	Q_PROPERTY(qreal hinset READ hinset WRITE setHinset NOTIFY hinsetChanged)
	Q_PROPERTY(qreal vinset READ vinset WRITE setVinset NOTIFY vinsetChanged)
	//Q_PROPERTY(qreal inset READ inset WRITE setInset NOTIFY insetChanged)
	Q_CLASSINFO("property.width.doc",
				"text jsou procenta napr 20% nebo jen % nebo nic nebo rozmer v mm\n"
				"% znamena rozdel zbytek na stejne casti\n"
				"n% znamena dej frame n% ze zbytku\n"
				"ve vertikalnim layoutu nejde kombinovat % a nezadany rozmery (atribut chybi)\n"
				"v takovem pripade se chybici atributy prevedou na '%'\n"
				"jedinou vyjjimkou je pripad, kdy ma % pouze posledni dite (ne treba '20%'', ale pouze '%'')\n"
				"v takovem pripade dostane child frame cely nepopsany zbytek rodice.")
	Q_PROPERTY(QVariant width READ width WRITE setWidth NOTIFY widthChanged)
	Q_PROPERTY(QVariant height READ height WRITE setHeight NOTIFY heightChanged)
	Q_CLASSINFO("property.layout.doc",
				"LayoutHorizontal - place children in the row\n"
				"LayoutVertical - place children in the column\n"
				"LayoutStack - place all children on stack"
				)
	Q_PROPERTY(Layout layout READ layout WRITE setLayout NOTIFY layoutChanged)
	Q_CLASSINFO("property.expandChildFrames.doc",
				"ramecky deti, jsou roztazeny tak, aby vyplnily parent frame, "
				"jinymi slovy, pokud v radku tabulky natece kazde policko jinak vysoke, budou vsechny roztazeny na vysku parent frame.\n"
				"Natahuji se jen ramecky, poloha vyrendrovaneho obsahu zustava nezmenena, "
				"dela se to tak, ze se nejprve vyrendruje stranka a pak se prochazi vyrendrovane ramecky a pokud je treba, "
				"zvetsuji se tak, aby vyplnily cely parent frame.  Objekty typu QFReportItemMetaPaintText jsou ignorovany"
				)
	Q_PROPERTY(bool expandVerticalSprings READ isExpandVerticalSprings WRITE setExpandVerticalSprings NOTIFY expandVerticalSpringsChanged)
	Q_PROPERTY(bool expandChildFrames READ isExpandChildFrames WRITE setExpandChildFrames NOTIFY expandChildFramesChanged)
	Q_PROPERTY(qreal renderedWidth READ renderedWidth NOTIFY renderedWidthChanged)
	Q_PROPERTY(qreal renderedHeight READ renderedHeight NOTIFY renderedHeightChanged)
	Q_PROPERTY(HAlignment halign READ horizontalAlignment WRITE setHorizontalAlignment NOTIFY horizontalAlignmentChanged)
	Q_PROPERTY(VAlignment valign READ verticalAlignment WRITE setVerticalAlignment NOTIFY verticalAlignmentChanged)
	Q_PROPERTY(QString columns READ columns WRITE setColumns NOTIFY columnsChanged)
	Q_PROPERTY(double columnsGap READ columnsGap WRITE setColumnsGap NOTIFY columnsGapChanged)
	Q_PROPERTY(qf::gui::reports::style::Pen* border READ border WRITE setBorder NOTIFY borderChanged)
	Q_PROPERTY(qf::gui::reports::style::Pen* topBorder READ topBorder WRITE setTopBorder NOTIFY topBorderChanged)
	Q_PROPERTY(qf::gui::reports::style::Pen* bottomBorder READ bottomBorder WRITE setBottomBorder NOTIFY bottomBorderChanged)
	Q_PROPERTY(qf::gui::reports::style::Pen* leftBorder READ leftBorder WRITE setLeftBorder NOTIFY leftBorderChanged)
	Q_PROPERTY(qf::gui::reports::style::Pen* rightBorder READ rightBorder WRITE setRightBorder NOTIFY rightBorderChanged)
	Q_PROPERTY(qf::gui::reports::style::Brush* fill READ fill WRITE setFill NOTIFY fillChanged)
	Q_CLASSINFO("property.textStyle.doc",
				"Set text style for this frame and all the children recursively"
				)
	Q_PROPERTY(qf::gui::reports::style::Text* textStyle READ textStyle WRITE setTextStyle NOTIFY textStyleChanged)
public:
	ReportItemFrame(ReportItem *parent = nullptr);
	~ReportItemFrame() override;
public:
	enum HAlignment { AlignLeft = Qt::AlignLeft,
					  AlignRight = Qt::AlignRight,
					  AlignHCenter = Qt::AlignHCenter,
					  AlignJustify = Qt::AlignJustify };
	enum VAlignment { AlignTop = Qt::AlignTop,
					  AlignBottom = Qt::AlignBottom,
					  AlignVCenter = Qt::AlignVCenter };
	Q_ENUM(HAlignment)
	Q_ENUM(VAlignment)

	QF_PROPERTY_IMPL2(qreal, h, H, inset, 0)
	QF_PROPERTY_IMPL2(qreal, v, V, inset, 0)
	QF_PROPERTY_IMPL2(qreal, r, R, enderedWidth, 0)
	QF_PROPERTY_IMPL2(qreal, r, R, enderedHeight, 0)
	QF_PROPERTY_IMPL2(HAlignment, h, H, orizontalAlignment, AlignLeft)
	QF_PROPERTY_IMPL2(VAlignment, v, V, erticalAlignment, AlignTop)
	QF_PROPERTY_IMPL2(QString, c, C, olumns, QStringLiteral("%"))
	QF_PROPERTY_IMPL2(double, c, C, olumnsGap, 3)
	QF_PROPERTY_OBJECT_IMPL(style::Pen*, t, T, opBorder)
	QF_PROPERTY_OBJECT_IMPL(style::Pen*, b, B, ottomBorder)
	QF_PROPERTY_OBJECT_IMPL(style::Pen*, l, L, eftBorder)
	QF_PROPERTY_OBJECT_IMPL(style::Pen*, r, R, ightBorder)
	QF_PROPERTY_OBJECT_IMPL(style::Brush*, f, F, ill)
	QF_PROPERTY_OBJECT_IMPL(style::Text*, t, T, extStyle)

	QF_PROPERTY_BOOL_IMPL2(e, E, xpandVerticalSprings, false)

	style::Pen* border() const;
	void setBorder(style::Pen *b);
	Q_SIGNAL void borderChanged(style::Pen* b);

private:
	QVariant m_width;
	QVariant m_height;
	Layout m_layout = LayoutVertical;
	bool m_expandChildFrames = false;
public:
	Q_SIGNAL void widthChanged(const QVariant &new_val);
	Q_SIGNAL void heightChanged(const QVariant &new_val);
	Q_SIGNAL void layoutChanged(const Layout &new_val);
	Q_SIGNAL void expandChildFramesChanged(const bool &new_val);
public:
	QVariant width() const {return m_width;}
	QVariant height() const {return m_height;}
	Layout layout() const {return m_layout;}
	bool isExpandChildFrames() const {return m_expandChildFrames;}
public:
	Q_SLOT void setWidth(const QVariant &val) {
		if(m_width != val) {
			m_width = val;
			initDesignedRect();
			emit widthChanged(m_width);
		}
	}
	Q_SLOT void setHeight(const QVariant &val) {
		if(m_height != val) {
			m_height = val;
			initDesignedRect();
			emit heightChanged(m_height);
		}
	}
	Q_SLOT void setLayout(const Layout &val) {
		if(m_layout != val) {
			m_layout = val;
			initDesignedRect();
			emit layoutChanged(m_layout);
		}
	}
	Q_SLOT void setExpandChildFrames(const bool &val) {
		if(m_expandChildFrames != val) {
			m_expandChildFrames = val;
			initDesignedRect();
			emit expandChildFramesChanged(m_expandChildFrames);
		}
	}

public:
	bool isRubber(Layout ly) {
		ChildSize sz = childSize(ly);
		return (sz.size == 0 && sz.unit == Rect::UnitMM);
	}
	static Layout orthogonalLayout(Layout l) {
		if(l == LayoutHorizontal)
			return LayoutVertical;
		if(l == LayoutVertical)
			return LayoutHorizontal;
		return LayoutInvalid;
	}
	Layout orthogonalLayout() const {return orthogonalLayout(layout());}
protected:
	ChildSize childSize(Layout parent_layout) override;
	ReportItemFrame* toFrame() override {return this;}

	virtual PrintResult printMetaPaintChildren(QPaintDevice *paint_device, ReportItemMetaPaint *out, const ReportItem::Rect &bounding_rect);
	Layout parentLayout() const
	{
		ReportItemFrame *frm = parentFrame();
		if(!frm)
			return LayoutInvalid;
		return frm->layout();
	}

	void initDesignedRect();

	void componentComplete() override;
public:
	PrintResult printMetaPaint(QPaintDevice *paint_device, ReportItemMetaPaint *out, const Rect &bounding_rect) override;
	PrintResult printHtml(HTMLElement &out) override;

	//! Nastavi u sebe a u deti indexToPrint na nulu, aby se vytiskly na dalsi strance znovu.
	void resetIndexToPrintRecursively(bool including_para_texts) override;

	Q_INVOKABLE void insertItem(int ix, QObject *item_object);
	Q_INVOKABLE void addItem(QObject *item_object);

	QString toString(int indent = 2, int indent_offset = 0) override;
private:
#if QT_VERSION_MAJOR < 6
	using WidgetIndexType = int;
#else
	using WidgetIndexType = long long;
#endif
	static void addItemFunction(QQmlListProperty<ReportItem> *list_property, ReportItem *item);
	static ReportItem* itemAtFunction(QQmlListProperty<ReportItem> *list_property, WidgetIndexType index);
	static void removeAllItemsFunction(QQmlListProperty<ReportItem> *list_property);
	static WidgetIndexType countItemsFunction(QQmlListProperty<ReportItem> *list_property);
protected:
	virtual int itemsToPrintCount() {return itemCount();}
	virtual ReportItem* itemToPrintAt(int ix) {return itemAt(ix);}
private:
	QQmlListProperty<ReportItem> items();
	int itemCount() const;
	ReportItem* itemAt(int index);
protected:
	//! children, kterym se ma zacit pri tisku
	int m_indexToPrint;
private:
	QList<ReportItem*> m_items;
};

}}}

#endif // QF_GUI_REPORTS_REPORTITEMFRAME_H
