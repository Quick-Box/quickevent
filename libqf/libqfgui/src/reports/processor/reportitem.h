//
// Author: Frantisek Vacek <fanda.vacek@volny.cz>, (C) 2006, 2014
//
// Copyright: See COPYING file that comes with this distribution
//

#ifndef QF_GUI_REPORTS_REPORTITEM_H
#define QF_GUI_REPORTS_REPORTITEM_H

#include "../../guiglobal.h"
#include "../../graphics/graphics.h"
//#include "style/pen.h"
//#include "style/brush.h"
#include "style/text.h"
#include "style/sheet.h"

#include <qf/core/utils.h>
#include <qf/core/utils/treetable.h>
#include <qf/core/utils/treeitembase.h>

#include <QPointF>
#include <QSizeF>
#include <QRectF>
#include <QTextLayout>
#include <QPicture>
#include <QQmlParserStatus>
#include <QQmlListProperty>

class QDomElement;
class QDomText;

namespace qf {
namespace gui {
namespace reports {

class ReportProcessor;
class ReportItemMetaPaint;

class ReportItemFrame;
class ReportItemBand;
class ReportItemMetaPaintFrame;

//! Base class of report elements.
class QFGUI_DECL_EXPORT ReportItem : public QObject, public QQmlParserStatus
{
	Q_OBJECT
	Q_INTERFACES(QQmlParserStatus)
	/// Pokud ma frame keepAll atribut a dvakrat za sebou se nevytiskne, znamena to, ze se nevytiskne uz nikdy.
	Q_PROPERTY(bool keepAll READ isKeepAll WRITE setKeepAll)
	Q_PROPERTY(bool keepWithPrev READ isKeepWithPrev WRITE setKeepWithPrev)
	Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged)
	Q_PROPERTY(QVariantMap htmlExportAttributes READ htmlExportAttributes WRITE setHtmlExportAttributes NOTIFY htmlExportAttributesChanged)
private:
	typedef QObject Super;
public:
	ReportItem(ReportItem *parent = nullptr);
	~ReportItem() override;

	QF_PROPERTY_BOOL_IMPL2(k, K, eepAll, false)
	QF_PROPERTY_BOOL_IMPL2(k, K, eepWithPrev, false)
	QF_PROPERTY_IMPL(QVariantMap, h, H, tmlExportAttributes)
public:
	enum Layout {LayoutInvalid = graphics::LayoutInvalid,
				 LayoutHorizontal = graphics::LayoutHorizontal,
				 LayoutVertical = graphics::LayoutVertical,
				 LayoutStacked = graphics::LayoutStacked
				};
	Q_ENUM(Layout)

	Q_INVOKABLE bool isVisible();
	Q_SLOT void setVisible(bool b) {
		if(m_visible != b) {
			m_visible = b;
			emit visibleChanged(m_visible);
		}
	}
	Q_SIGNAL void visibleChanged(bool new_val);
public:
	typedef QDomElement HTMLElement;
	static const double Epsilon;
	static const QString INFO_IF_NOT_FOUND_DEFAULT_VALUE;
	//typedef Layout Layout;
	class PrintResult
	{
	private:
		enum class Result : quint8 {
			Invalid,
			PrintFinished, ///< whole frame printed successfully
			PrintAgain, ///< partialy printed, repeat print in next column
			PrintError, ///< When keepAll is set and frame can is rendered higher than page, stop to try print it on the next page
		};
		enum Flag : quint8 {
			ColumnBreak = (1 << 0),
			PageBreak = (1 << 1),
			NextDetailRowExists = (1 << 2),
			//ErrorNeverFit = (1 << 3),
		};

		Result result;
		quint8 flags = 0;

		PrintResult(Result r) : result(r) {}
	public:
		PrintResult() : PrintResult(Result::Invalid) {}

		//static PrintResult createInvalid() {return PrintResult(Result::Invalid);}
		static PrintResult createPrintFinished() {return PrintResult(Result::PrintFinished);}
		static PrintResult createPrintAgain() {return PrintResult(Result::PrintAgain);}
		static PrintResult createPrintError() {return PrintResult(Result::PrintError);}

		bool isValid() const {return result == Result::Invalid;}
		bool isPrintFinished() const {return result == Result::PrintFinished;}
		bool isPrintAgain() const {return result == Result::PrintAgain;}
		//bool isPrintAgain() const {return result != Result::PrintFinished && result != Result::PrintError;}
		bool isNextDetailRowExists() const {return flags & NextDetailRowExists;}
		void setNextDetailRowExists(bool b) {flags = b? flags | NextDetailRowExists: flags & ~NextDetailRowExists;}
		bool isColumnBreak() const {return flags & ColumnBreak;}
		void setColumnBreak(bool b) {flags = b? flags | ColumnBreak: flags & ~ColumnBreak;}
		bool isPageBreak() const {return flags & PageBreak;}
		void setPageBreak(bool b) {flags = b? flags | PageBreak: flags & ~PageBreak;}
		bool isPrintError() const {return result == Result::PrintError;}

		QString toString() const {
			QString ret;
			switch(result) {
			case Result::Invalid: ret = QStringLiteral("Invalid"); break;
			case Result::PrintFinished: ret = QStringLiteral("PrintFinished"); break;
			case Result::PrintAgain: ret = QStringLiteral("PrintAgain"); break;
			case Result::PrintError: ret = QStringLiteral("PrintError"); break;
			default: ret = QStringLiteral("UNKNOWN"); break;
			}
			return ret;
		}
	};
public:
	typedef graphics::Point Point;
	class Size : public graphics::Size
	{
	public:
		Size() : graphics::Size(0, 0) {}
		Size(qreal x, qreal y) : graphics::Size(x, y) {}
		Size(const graphics::Size &s) : graphics::Size(s) {}
		Size(const QSizeF &s) : graphics::Size(s) {}

		qreal sizeInLayout(Layout ly)
		{
			if(ly == LayoutHorizontal)
				return width();
			return height();
		}

		Size& addSizeInLayout(const Size &sz, Layout ly)
		{
			if(ly == LayoutHorizontal) {
				setWidth(width() + sz.width());
				setHeight(qMax(height(), sz.height()));
			}
			else {
				setHeight(height() + sz.height());
				setWidth(qMax(width(), sz.width()));
			}
			return *this;
		}
	};
	class Rect : public graphics::Rect
	{
	public:
		enum Flag {
			//LeftFixed = 1,
			//TopFixed = 2,
			//BottomFixed = 4,
			//RightFixed = 8,
			FillLayout = 1, /// tento item se natahne ve smeru layoutu tak, aby vyplnil cely bounding_rect
			ExpandChildFrames = 2, /// viz. atribut expandChildFrames v Report.rnc
			LayoutHorizontalFlag = 4, /// rect ma layout ve smeru x
			LayoutVerticalFlag = 8 /// rect ma layout ve smeru y, pokud je kombinace LayoutX a LayoutY nesmyslna predpoklada se LayoutX == 0 LayoutY == 1
			//--BackgroundItem = 16
		};
		enum Unit {UnitInvalid = 0, UnitMM, UnitPercent};
	public:
		unsigned flags;
		Unit horizontalUnit, verticalUnit;
	public:
		/*--
		bool isAnchored()  const
		{
			return flags & (LeftFixed | TopFixed | BottomFixed | RightFixed);
		}
		--*/
		bool isRubber(Layout ly)  const
		{
			if(ly == LayoutHorizontal)
				return (width() == 0 && horizontalUnit == UnitMM);
			return (height() == 0 && verticalUnit == UnitMM);
		}
		Unit unit(Layout ly)  const
		{
			if(ly == LayoutHorizontal)
				return horizontalUnit;
			return verticalUnit;
		}

		qreal sizeInLayout(Layout ly) const
		{
			return Size(size()).sizeInLayout(ly);
		}
		Rect& setSizeInLayout(qreal sz, Layout ly)
		{
			if(ly == LayoutHorizontal)
				setWidth(sz);
			else
				setHeight(sz);
			return *this;
		}
		Rect& cutSizeInLayout(const Rect &rect, Layout ly)
		{
			if(ly == LayoutHorizontal) {
				if(rect.right() > right())
					setLeft(right());
				else
					setLeft(rect.right());
			}
			else {
				if(rect.bottom() > bottom())
					setTop(bottom());
				else
					setTop(rect.bottom());
			}
			return *this;
		}

		QString toString() const {
			return QString("rect(%1, %2, size[%3%4, %5%6]) [%7]")
					.arg(Point(topLeft()).toString(), Point(bottomRight()).toString())
					.arg(size().width()).arg(unitToString(horizontalUnit))
					.arg(size().height()).arg(unitToString(verticalUnit))
					.arg(flagsToString(flags));
		}

		static QString unitToString(Unit u) {
			QString ret;
			switch(u) {
			case UnitMM:
				ret = "mm"; break;
			case UnitPercent:
				ret = "%"; break;
			default:
				ret = "invalid";
			}
			return ret;
		}

		static QString flagsToString(unsigned flags) {
			QString ret;
			if(flags & FillLayout) ret += 'F';
			if(flags & ExpandChildFrames) ret += 'X';
			if(flags & LayoutHorizontalFlag) ret += 'H';
			if(flags & LayoutVerticalFlag) ret += 'V';
			return ret;
		}
	private:
		void init() {
			flags = 0;
			horizontalUnit = verticalUnit = UnitMM;
		}
	public:
		Rect() : graphics::Rect() {init();}
		Rect(const QPointF &topLeft, const QSizeF &size) : graphics::Rect(topLeft, size) {init();}
		Rect(qreal x, qreal y, qreal width, qreal height) : graphics::Rect(x, y, width, height) {init();}
		Rect(const QRectF &r) : graphics::Rect(r) {init();}
		Rect(const graphics::Rect &r) : graphics::Rect(r) {init();}
	};
public:
	struct ChildSize {

		ChildSize(qreal s = 0, Rect::Unit u = Rect::UnitMM) : size(s), unit(u) {}

		double fillLayoutRatio() const
		{
			return (unit == Rect::UnitPercent)? size / 100.: -1;
		}

		double fromParentSize(double parent_size) const
		{
			double ret = parent_size;
			if(unit == Rect::UnitMM) {
				if(size > 0)
					ret = size;
			}
			else if(unit == Rect::UnitPercent) {
				if(size > 0)
					ret = size / 100 * ret;
			}
			return ret;
		}

		qreal size;
		Rect::Unit unit;
	};
public:
	struct Image
	{
		QPicture picture;
		//QPixmap pixmap;
		QImage image;

		//bool isPixmap() const {return !pixmap.isNull();}
		bool isPicture() const {return !picture.isNull();}
		bool isImage() const {return !image.isNull();}
		Size size() const {
			if(isImage()) return QSizeF(image.size());
			//if(isPixmap()) return pixmap.size();
			if(isPicture()) return QSizeF(picture.boundingRect().size());
			return Size();
		}
		bool isNull() const {return picture.isNull() && image.isNull();}
		//const QImage& toQImage() const {return image;}

		Image() {}
		//Image(const QPixmap &px) : pixmap(px) {}
		Image(const QPicture &pc) : picture(pc) {}
		Image(const QImage &im) : image(im) {}
	};
public:
	virtual ReportItemFrame* toFrame() {return nullptr;}
protected:
	ReportItemBand *parentBand();

	QVariant value(const QString &data_src, const QString &domain = "row", const QVariantList &params = QVariantList(), const QVariant &default_value = ReportItem::INFO_IF_NOT_FOUND_DEFAULT_VALUE, bool sql_match = true);
	/// poukud ma node jen jedno dite vrati to jeho hodnotu vcetne typu, pokud je deti vic, udela to z nich jeden string

	//! Pokud byl predchozi result PrintNotFit a soucasny opet PrintNotFit, znamena to, ze se item uz nikdy nevejde,
	//! zavedenim tohoto fieldu zabranim nekonecnemu odstrankovavani.
	PrintResult checkPrintResult(PrintResult res);
public:
	ReportProcessor* processor(bool throw_exc = qf::core::Exception::Throw);
	//! Vraci atribut elementu itemu.
	//! Pokud hodnota \a attr_name je ve tvaru 'script:funcname', zavola se scriptDriver processoru, jinak se vrati atribut.
	//--QString elementAttribute(const QString &attr_name, const QString &default_val = QString());

	ReportItem* parent() const {
		return static_cast<ReportItem*>(this->Super::parent());
	}
	//! Print item in form, that understandable by ReportPainter.
	virtual PrintResult printMetaPaint(QPaintDevice *paint_device, ReportItemMetaPaint *out, const Rect &bounding_rect);
	//! Print item in HTML element form.
	virtual PrintResult printHtml(HTMLElement &out);
	/// vrati definovanou velikost pro item a layout
	virtual ChildSize childSize(Layout parent_layout) {Q_UNUSED(parent_layout); return ChildSize();}

	ReportItemFrame* parentFrame() const
	{
		if(parent())
			return parent()->toFrame();
		return nullptr;
	}
	static const bool IncludingParaTexts = true;
	/// sometimes is necessarry to continue printing of overflowed text and print brorders for it again,
	/// then \a including_para_texts == false
	virtual void resetIndexToPrintRecursively(bool including_para_texts) {Q_UNUSED(including_para_texts);}
	//virtual bool isBreak() {return false;}

	virtual bool canBreak() {return !isKeepAll();}

	virtual QString toString(int indent = 2, int indent_offset = 0);

protected:
	style::Text* effectiveTextStyle();

	void createHtmlExportAttributes(HTMLElement &out) const;

	void classBegin() override;
	void componentComplete() override;
public:
	Rect designedRect;
protected:
	bool m_recentPrintNotFinished;
private:
	bool m_visible;
};

/*--

class ReportItemTable : public ReportItemBand
{
	Q_OBJECT
private:
	typedef ReportItemBand Super;
protected:
	QDomDocument fakeBandDocument;
	QDomElement fakeBand;
	void createFakeBand();
	virtual bool childrenSynced() {return children().count() > 0;}
	virtual void syncChildren();
public:
	virtual PrintResult printMetaPaint(QPaintDevice *paint_device, ReportItemMetaPaint *out, const Rect &bounding_rect);
public:
	ReportItemTable(ReportItem *parent);
	virtual ~ReportItemTable() {}
};
--*/

#ifdef REPORT_ITEM_GRAPH

class QFGUI_DECL_EXPORT ReportItemGraph : public ReportItemImage
{
	Q_OBJECT
private:
	typedef ReportItemImage Super;
protected:
	//--virtual void syncChildren();
	virtual PrintResult printMetaPaintChildren(QPaintDevice *paint_device, ReportItemMetaPaint *out, const ReportItem::Rect &bounding_rect);
public:
	ReportItemGraph(ReportItem *parent = nullptr)
		: Super(parent) {}
};
#endif
}}}

#endif // QF_GUI_REPORTS_REPORTITEM_H

