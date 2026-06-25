#include "startslotheader.h"
#include "startslotitem.h"
#include "ganttscene.h"
#include "ganttitem.h"

#include <qf/core/assert.h>

#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneWheelEvent>
#include <QMenu>
#include <QInputDialog>
#include <QPainter>
#include <QApplication>
#include <QDrag>
#include <QMimeData>
#include <QStyleOptionGraphicsItem>
#include <QJsonDocument>

#include <functional>

using namespace drawing;

namespace {

QColor hoverColor()
{
	QColor c("steelblue");
	c.setAlpha(48);
	return c;
}

int wheelSteps(QGraphicsSceneWheelEvent *event)
{
	double n = event->delta() / 120.;
	if(n > -1 && n <= 0)
		n = -1;
	else if(n < 1 && n > 0)
		n = 1;
	return (int)n;
}

void paintItemRecursively(QGraphicsItem *it, QPainter *painter, QStyleOptionGraphicsItem *opt)
{
	if(!it->isVisible())
		return;
	painter->save();
	painter->translate(it->pos());
	it->paint(painter, opt, nullptr);
	for(QGraphicsItem *child : it->childItems())
		paintItemRecursively(child, painter, opt);
	painter->restore();
}

}

class LockItem : public QGraphicsRectItem
{
	Q_DECLARE_TR_FUNCTIONS(drawing::ClassdefsLockItem)
public:
	LockItem(StartSlotHeader *parent = nullptr) : QGraphicsRectItem(parent), m_startSlotItem(parent->startSlotItem())
	{
		int du_px = m_startSlotItem->ganttScene()->displayUnit();
		setRect(0, 0, 3 * du_px, 2 * du_px);
		setAcceptHoverEvents(true);
		setCursor(Qt::PointingHandCursor);
		updateToolTip();
	}

	void updateToolTip()
	{
		if(m_startSlotItem->isIgnoreClassClashCheck())
			setToolTip(tr("Clash check is OFF for this slot.\nClick to check start time clashes of this slot's classes again."));
		else
			setToolTip(tr("Clash check is ON for this slot.\nClick to exclude this slot's classes from start time clash checks."));
	}

	void mousePressEvent(QGraphicsSceneMouseEvent *event) Q_DECL_OVERRIDE
	{
		if(event->button() == Qt::LeftButton) {
			m_startSlotItem->setIgnoreClassClashCheck(!m_startSlotItem->isIgnoreClassClashCheck());
			updateToolTip();
			parentItem()->update();
			event->accept();
		}
	}

	void hoverEnterEvent(QGraphicsSceneHoverEvent *event) Q_DECL_OVERRIDE
	{
		Q_UNUSED(event)
		m_hover = true;
		update();
	}

	void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) Q_DECL_OVERRIDE
	{
		Q_UNUSED(event)
		m_hover = false;
		update();
	}

	void paint(QPainter *painter, const QStyleOptionGraphicsItem * option, QWidget *widget = nullptr) Q_DECL_OVERRIDE
	{
		Q_UNUSED(option)
		Q_UNUSED(widget)
		//QGraphicsRectItem::paint(painter, option, widget);
		QRectF r = rect();
		if(m_hover)
			painter->fillRect(r, hoverColor());
		// draw the 3:2 padlock centered in the (square) click area
		double w = qMin(r.width(), r.height() * 3 / 2) * 3 / 4;
		double h = 2 * w / 3;
		painter->save();
		painter->translate(r.center().x() - w / 2, r.center().y() - h / 2);
		QRectF r1(0, 0, w / 3, h / 2);
		QRectF r2(0, 0, w * 2 / 3, h / 2);
		QColor c;
		bool is_ignore_class_check = m_startSlotItem->isIgnoreClassClashCheck();
		if(is_ignore_class_check) {
			r1.moveLeft(w / 3);
			r2.moveLeft(w / 6);
			c = Qt::blue;
		}
		else {
			r1.moveLeft(w / 2);
			c = Qt::darkGreen;
		}
		r1.moveTop(h / 8);
		r2.moveTop(h / 2);

		QPen p(Qt::SolidLine);
		p.setWidthF(h / 8);
		p.setColor(c);
		p.setCapStyle(Qt::FlatCap);
		painter->setPen(p);
		//painter->fillRect(r1, Qt::yellow);
		painter->drawArc(r1, 0, 180 * 16);
		painter->drawLine(r1.bottomLeft(), QPointF(r1.left(), r1.center().y()));
		painter->drawLine(r1.bottomRight(), QPointF(r1.right(), r1.center().y()));

		double d = p.widthF();
		r2.adjust(d, 0, -d, 0);
		painter->fillRect(r2, c);
		painter->restore();
	}
private:
	StartSlotItem *m_startSlotItem;
	bool m_hover = false;
};

class SpinButton : public QGraphicsRectItem
{
public:
	SpinButton(int step, std::function<void(int)> step_fn, QGraphicsItem *parent)
		: QGraphicsRectItem(parent), m_step(step), m_stepFn(step_fn)
	{
		setAcceptHoverEvents(true);
		setCursor(Qt::PointingHandCursor);
		setPen(Qt::NoPen);
	}

	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) Q_DECL_OVERRIDE
	{
		Q_UNUSED(option)
		Q_UNUSED(widget)
		QRectF r = rect();
		if(m_hover)
			painter->fillRect(r, hoverColor());
		double w2 = r.width() / 4;
		double h2 = r.height() / 6;
		QPointF c = r.center();
		QPolygonF arrow;
		if(m_step < 0)
			arrow << QPointF(c.x() - w2, c.y()) << QPointF(c.x() + w2, c.y() - h2) << QPointF(c.x() + w2, c.y() + h2);
		else
			arrow << QPointF(c.x() + w2, c.y()) << QPointF(c.x() - w2, c.y() - h2) << QPointF(c.x() - w2, c.y() + h2);
		painter->save();
		painter->setRenderHint(QPainter::Antialiasing);
		painter->setPen(Qt::NoPen);
		painter->setBrush(QColor(isEnabled()? Qt::darkGray: Qt::lightGray));
		painter->drawPolygon(arrow);
		painter->restore();
	}

	void hoverEnterEvent(QGraphicsSceneHoverEvent *event) Q_DECL_OVERRIDE
	{
		Q_UNUSED(event)
		m_hover = true;
		update();
	}

	void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) Q_DECL_OVERRIDE
	{
		Q_UNUSED(event)
		m_hover = false;
		update();
	}

	void mousePressEvent(QGraphicsSceneMouseEvent *event) Q_DECL_OVERRIDE
	{
		if(event->button() == Qt::LeftButton) {
			m_stepFn(m_step);
			event->accept();
		}
	}

	void wheelEvent(QGraphicsSceneWheelEvent *event) Q_DECL_OVERRIDE
	{
		m_stepFn(wheelSteps(event));
		event->accept();
	}
private:
	int m_step;
	std::function<void(int)> m_stepFn;
	bool m_hover = false;
};

class SpinValueItem : public QGraphicsRectItem
{
public:
	SpinValueItem(std::function<void(int)> step_fn, QGraphicsItem *parent)
		: QGraphicsRectItem(parent), m_stepFn(step_fn)
	{
		setAcceptHoverEvents(true);
		setPen(Qt::NoPen);
	}

	void setText(const QString &t)
	{
		if(m_text != t) {
			m_text = t;
			update();
		}
	}

	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) Q_DECL_OVERRIDE
	{
		Q_UNUSED(option)
		Q_UNUSED(widget)
		if(m_hover)
			painter->fillRect(rect(), hoverColor());
		if(!isEnabled())
			painter->setPen(Qt::gray);
		painter->drawText(rect(), Qt::AlignCenter, m_text);
	}

	void hoverEnterEvent(QGraphicsSceneHoverEvent *event) Q_DECL_OVERRIDE
	{
		Q_UNUSED(event)
		m_hover = true;
		update();
	}

	void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) Q_DECL_OVERRIDE
	{
		Q_UNUSED(event)
		m_hover = false;
		update();
	}

	void wheelEvent(QGraphicsSceneWheelEvent *event) Q_DECL_OVERRIDE
	{
		m_stepFn(wheelSteps(event));
		event->accept();
	}
private:
	QString m_text;
	std::function<void(int)> m_stepFn;
	bool m_hover = false;
};

/// `< value >` spin box: square buttons around a 2:1 value cell
class SpinnerItem : public QGraphicsRectItem
{
public:
	SpinnerItem(std::function<void(int)> step_fn, QGraphicsItem *parent)
		: QGraphicsRectItem(parent)
	{
		setPen(Qt::NoPen);
		m_btnDec = new SpinButton(-1, step_fn, this);
		m_value = new SpinValueItem(step_fn, this);
		m_btnInc = new SpinButton(+1, step_fn, this);
	}

	void setToolTips(const QString &value_tt, const QString &dec_tt, const QString &inc_tt)
	{
		m_value->setToolTip(value_tt);
		m_btnDec->setToolTip(dec_tt);
		m_btnInc->setToolTip(inc_tt);
	}

	void setGeometry(const QRectF &r)
	{
		setPos(r.topLeft());
		setRect(0, 0, r.width(), r.height());
		// half-square buttons around a square value cell
		double h = r.height();
		m_btnDec->setRect(0, 0, h / 2, h);
		m_value->setRect(0, 0, h, h);
		m_value->setPos(h / 2, 0);
		m_btnInc->setRect(0, 0, h / 2, h);
		m_btnInc->setPos(3 * h / 2, 0);
	}

	void setValue(const QString &s)
	{
		m_value->setText(s);
	}

	void setSpinnerEnabled(bool b)
	{
		if(isEnabled() == b)
			return;
		setEnabled(b); // propagates to the children
		if(b) {
			m_btnDec->setCursor(Qt::PointingHandCursor);
			m_btnInc->setCursor(Qt::PointingHandCursor);
		}
		else {
			m_btnDec->unsetCursor();
			m_btnInc->unsetCursor();
		}
		m_btnDec->update();
		m_value->update();
		m_btnInc->update();
	}
private:
	SpinButton *m_btnDec;
	SpinButton *m_btnInc;
	SpinValueItem *m_value;
};

StartSlotHeader::StartSlotHeader(StartSlotItem *parent)
	: Super(parent), IGanttItem(this)
{
	auto *slot_it = startSlotItem();
	// column 1: slot number above the clash check lock
	m_textSlotNo = new QGraphicsTextItem(this);
	m_textSlotNo->setDefaultTextColor(Qt::gray);
	m_lockItem = new LockItem(this);
	// column 2: start time spinner above the start interval spinner
	auto *start_spinner = new SpinnerItem([slot_it](int steps) {
		slot_it->setStartOffset(slot_it->startOffset() + steps);
	}, this);
	start_spinner->setToolTips(
				tr("Start time of the first class in this slot [min].\nUse mouse wheel to change it, or right-click the slot header for more options."),
				tr("Move the slot start 1 minute earlier"),
				tr("Move the slot start 1 minute later"));
	m_startSpinner = start_spinner;
	auto *interval_spinner = new SpinnerItem([slot_it](int steps) {
		slot_it->setStartInterval(slot_it->startInterval() + steps);
	}, this);
	interval_spinner->setToolTips(
				tr("Start interval of classes in this slot [min].\nUse mouse wheel to change it, the value is set to all classes in the slot."),
				tr("Decrease the start interval by 1 minute"),
				tr("Increase the start interval by 1 minute"));
	m_intervalSpinner = interval_spinner;

	setToolTip(tr("Start slot: drag the header to reorder slots,\nright-click for more options."));
	setCursor(Qt::ArrowCursor);
	setAcceptDrops(true);
}

int StartSlotHeader::minHeight()
{
	return ganttScene()->rowHeight();
}

static constexpr int LABEL_WIDTH_DU = 10;

void StartSlotHeader::updateGeometry()
{
	auto *slot_it = startSlotItem();
	int du_px = ganttScene()->displayUnit();
	int label_width = LABEL_WIDTH_DU * du_px;
	QRectF r = slot_it->rect();
	r.setWidth(label_width);
	setRect(r);
	setPos(-label_width, 0);

	// 2 columns x 2 rows grid filling the whole header height,
	// column 1 cells are squares, spinner cells are 1/2 + 1 + 1/2 square
	double row_h = r.height() / 2;
	double col1_w = row_h;
	m_textSlotNo->setPlainText(QString::number(slot_it->slotNumber()));
	QRectF no_br = m_textSlotNo->boundingRect();
	m_textSlotNo->setPos((col1_w - no_br.width()) / 2, (row_h - no_br.height()) / 2);
	m_lockItem->setRect(0, 0, col1_w, row_h);
	m_lockItem->setPos(0, row_h);
	auto *start_spinner = static_cast<SpinnerItem*>(m_startSpinner);
	start_spinner->setGeometry(QRectF(col1_w, 0, 2 * row_h, row_h));
	start_spinner->setValue(QString::number(slot_it->data().startOffset()));
	auto *interval_spinner = static_cast<SpinnerItem*>(m_intervalSpinner);
	interval_spinner->setGeometry(QRectF(col1_w, row_h, 2 * row_h, row_h));
	if(slot_it->isStartIntervalUniform()) {
		int interval = slot_it->startInterval();
		interval_spinner->setValue(interval < 0? QString(): QString::number(interval));
		interval_spinner->setToolTips(
					tr("Start interval of classes in this slot [min].\nUse mouse wheel to change it, the value is set to all classes in the slot."),
					tr("Decrease the start interval by 1 minute"),
					tr("Increase the start interval by 1 minute"));
		interval_spinner->setSpinnerEnabled(true);
	}
	else {
		interval_spinner->setValue(QStringLiteral("≠")); // not-equal sign
		QStringList class_intervals;
		for(int i = 0; i < slot_it->classItemCount(); ++i) {
			const ClassData &cd = slot_it->classItemAt(i)->data();
			class_intervals << QStringLiteral("%1: %2").arg(cd.className()).arg(cd.startIntervalMin());
		}
		const QString tool_tip = tr("Classes in this slot have different start intervals (%1),\nediting is disabled.").arg(class_intervals.join(QStringLiteral(", ")));
		interval_spinner->setToolTips(tool_tip, tool_tip, tool_tip);
		interval_spinner->setSpinnerEnabled(false);
	}
	interval_spinner->setVisible(slot_it->classItemCount() > 0);
}

void StartSlotHeader::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
	const StartSlotData &dt = startSlotItem()->data();
	if(dt.isIgnoreClassClashCheck()) {
		QColor c("khaki");
		c.setAlpha(128);
		QRectF r = rect();
		r.setLeft(0);
		painter->fillRect(r, c);
	}
	if(m_dragIn) {
		QColor c("steelblue");
		c.setAlpha(32);
		QRectF r = rect();
		r.setLeft(0);
		painter->fillRect(r, c);
	}
	Super::paint(painter, option, widget);
}

void StartSlotHeader::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
	StartSlotData dt = startSlotItem()->data();
	QMenu menu;
	QAction *a_append_start_slot = menu.addAction(tr("Append start slot"));
	QAction *a_set_start = menu.addAction(tr("Set slot start offset"));
	//QAction *a_locked = menu.addAction(tr("Locked"));
	//a_locked->setChecked(dt.isLocked());
	//a_locked->setCheckable(true);
	QAction *a_clash = menu.addAction(tr("Ignore class clash check"));
	a_clash->setCheckable(true);
	a_clash->setChecked(dt.isIgnoreClassClashCheck());
	QAction *a = menu.exec(event->screenPos());
	if(a == a_append_start_slot) {
		auto *gi = ganttItem();
		gi->insertStartSlotItem(gi->startSlotItemIndex(startSlotItem()) + 1, new StartSlotItem(gi));
		ganttScene()->setDirty(true);
		gi->updateGeometry();
	}
	else if(a == a_set_start) {
		QWidget *w = nullptr;
		QObject *o = scene();
		while(o) {
			w = qobject_cast<QWidget*>(o);
			if(w)
				break;
			o = o->parent();
		}
		bool ok;
		int i = QInputDialog::getInt(w, tr("InputDialog"), tr("Start slot offset [min]:"), dt.startOffset(), 0, 1000000, 1, &ok);
		if(ok) {
			startSlotItem()->setStartOffset(i);
		}
	}
	else if(a == a_clash) {
		startSlotItem()->setIgnoreClassClashCheck(a_clash->isChecked());
		static_cast<LockItem*>(m_lockItem)->updateToolTip();
		update();
	}
	//else if(a == a_locked) {
	//	startSlotItem()->setLocked(!startSlotItem()->isLocked());
	//	update();
	//}
}

StartSlotItem *StartSlotHeader::startSlotItem()
{
	auto *ret = dynamic_cast<StartSlotItem*>(parentItem());
	QF_ASSERT_EX(ret != nullptr, "Bad parent!");
	return ret;
}

void StartSlotHeader::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
	Q_UNUSED(event)
	setCursor(Qt::ClosedHandCursor);
}

void StartSlotHeader::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
	if (QLineF(event->screenPos(), event->buttonDownScreenPos(Qt::LeftButton)).length() < QApplication::startDragDistance()) {
		return;
	}
	qfLogFuncFrame();
	auto *drag = new QDrag(event->widget());
	auto *mime = new QMimeData;
	drag->setMimeData(mime);
	{
		QVariantMap m;
		int slot_ix = ganttItem()->startSlotItemIndex(startSlotItem());
		m[QStringLiteral("slotIndex")] = slot_ix;
		QJsonDocument jsd = QJsonDocument::fromVariant(m);
		QString mime_text = QString::fromUtf8(jsd.toJson());
		qfDebug() << "mime:" << mime_text;
		mime->setText(mime_text);

		QPixmap pixmap(rect().size().toSize());
		pixmap.fill(Qt::white);

		QPainter painter(&pixmap);
		//painter.translate(15, 15);
		painter.setRenderHint(QPainter::Antialiasing);
		QStyleOptionGraphicsItem opt;
		paint(&painter, &opt, nullptr);
		for(QGraphicsItem *it : childItems())
			paintItemRecursively(it, &painter, &opt);
		painter.end();
		//pixmap.setMask(pixmap.createHeuristicMask());

		drag->setPixmap(pixmap);
		drag->setHotSpot(QPoint(ganttScene()->displayUnit(), 0));
	}
	drag->exec();
	setCursor(Qt::ArrowCursor);
}

void StartSlotHeader::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
	Q_UNUSED(event)
	setCursor(Qt::ArrowCursor);
}

void StartSlotHeader::dragEnterEvent(QGraphicsSceneDragDropEvent *event)
{
	event->setAccepted(true);
	m_dragIn = true;
	update();
}

void StartSlotHeader::dragMoveEvent(QGraphicsSceneDragDropEvent *event)
{
	event->setAccepted(true);
}

void StartSlotHeader::dragLeaveEvent(QGraphicsSceneDragDropEvent *event)
{
	Q_UNUSED(event);
	m_dragIn = false;
	update();
}

void StartSlotHeader::dropEvent(QGraphicsSceneDragDropEvent *event)
{
	qfLogFuncFrame();
	QJsonDocument jsd = QJsonDocument::fromJson(event->mimeData()->text().toUtf8());
	QVariantMap m = jsd.toVariant().toMap();
	Qt::DropAction act = (m.isEmpty())? Qt::IgnoreAction: Qt::MoveAction;
	event->setDropAction(act);
	event->accept();

	int slot1_ix = m.value(QStringLiteral("slotIndex"), -1).toInt();
	int slot2_ix = ganttItem()->startSlotItemIndex(startSlotItem());
	qfDebug() << "DROP header:" << slot1_ix << slot2_ix;
	ganttItem()->moveStartSlotItem(slot1_ix, slot2_ix);

	m_dragIn = false;
	update();
}
