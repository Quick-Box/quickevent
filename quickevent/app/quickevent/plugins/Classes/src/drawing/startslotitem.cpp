#include "startslotitem.h"
#include "classitem.h"
#include "ganttitem.h"
#include "ganttscene.h"
#include "startslotheader.h"

#include <qf/core/assert.h>

#include <QGraphicsSceneMouseEvent>
#include <QJsonDocument>
#include <QPainter>
#include <QtGlobal>
#include <QMimeData>
#include <algorithm>

using namespace drawing;

StartSlotItem::StartSlotItem(QGraphicsItem *parent)
	: Super(parent), IGanttItem(this)
{
	m_header = new StartSlotHeader(this);
	//setFlag(QGraphicsItem::ItemIsSelectable);
	setAcceptDrops(true);
}

int StartSlotItem::classItemCount() const
{
	return m_classItems.count();
}

int StartSlotItem::classItemIndex(const ClassItem *it) const
{
	return m_classItems.indexOf(it);
}

void StartSlotItem::insertClassItem(int ix, ClassItem *it)
{
	QF_ASSERT(it != nullptr, "Item == NULL", return);
	m_classItems.insert(ix, it);
	it->setParentItem(this);
}

ClassItem *StartSlotItem::classItemAt(int ix, bool throw_ex)
{
	ClassItem *ret = m_classItems.value(ix);
	if(!ret && throw_ex)
		QF_EXCEPTION(QString("Invalid item index %1, item count %2!").arg(ix).arg(classItemCount()));
	return ret;
}

ClassItem *StartSlotItem::takeClassItemAt(int ix)
{
	if(ix >= 0 && ix < m_classItems.count()) {
		ClassItem *ret = m_classItems.takeAt(ix);
		ret->setParentItem(nullptr);
		return ret;
	}
	return nullptr;
}

void StartSlotItem::setStartOffset(int start_offset)
{
	start_offset = std::max(start_offset, 0);
	auto dt = config();
	if(dt.startOffset != start_offset) {
		dt.startOffset = start_offset;
		setConfig(dt);
		ganttScene()->setDirty(true);
		updateGeometry();
		ganttItem()->checkClassClash();
	}
}

int StartSlotItem::startOffset() const
{
	auto dt = config();
	return dt.startOffset;
}

void StartSlotItem::setStartInterval(int interval_min)
{
	if(m_classItems.isEmpty())
		return;
	interval_min = std::max(interval_min, 1);
	if(startInterval() == interval_min)
		return;
	for(ClassItem *it : m_classItems) {
		ClassData dt = it->data();
		dt.setStartIntervalMin(interval_min);
		it->setData(dt);
	}
	ganttScene()->setDirty(true);
	ganttItem()->updateGeometry();
	ganttItem()->checkClassClash();
}

int StartSlotItem::startInterval() const
{
	if(m_classItems.isEmpty())
		return -1;
	return m_classItems.value(0)->data().startIntervalMin();
}

bool StartSlotItem::isStartIntervalUniform() const
{
	for(int i = 1; i < m_classItems.count(); ++i) {
		if(m_classItems[i]->data().startIntervalMin() != m_classItems[0]->data().startIntervalMin())
			return false;
	}
	return true;
}

bool StartSlotItem::isIgnoreClassClashCheck() const
{
	return config().ignoreClassClashCheck;
}

void StartSlotItem::setIgnoreClassClashCheck(bool b)
{
	auto dt = config();
	if(dt.ignoreClassClashCheck == b)
		return;
	dt.ignoreClassClashCheck = b;
	setConfig(dt);
	ganttScene()->setDirty(true);
	updateGeometry();
	ganttItem()->checkClassClash();
}
/*
void StartSlotItem::setLocked(bool b)
{
	auto dt = data();
	dt.setLocked(b);
	setData(dt);
	updateGeometry();
	ganttItem()->checkClassClash();
}

bool StartSlotItem::isLocked() const
{
	return data().isLocked();
}
*/
void StartSlotItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
	if(m_dragIn) {
		QColor c("steelblue");
		c.setAlpha(32);
		QRectF r = rect();
		r.setLeft(0);
		painter->fillRect(r, c);
	}
	Super::paint(painter, option, widget);
}

ClassItem *StartSlotItem::addClassItem()
{
	auto *it = new ClassItem(this);
	insertClassItem(classItemCount(), it);
	return it;
}

const Event::StartSlotConfig& StartSlotItem::config() const
{
	return m_config;
}

void StartSlotItem::setConfig(const Event::StartSlotConfig &data)
{
	m_config = data;

}

void StartSlotItem::updateGeometry()
{
	qfLogFuncFrame();
	const auto &dt = config();
	int pos_x = minToPx(dt.startOffset);
	double h = m_header->minHeight();
	for (int i = 0; i < classItemCount(); ++i) {
		ClassItem *it = classItemAt(i);
		qfDebug() << i << it;
		/*
		if(isLocked()) {
			pos_x = minToPx(it->data().startTimeMin());
		}
		*/
		it->setPos(pos_x, 0);
		it->updateGeometry();
		//it->setZValue(it->isLocked()? 1: 0);
		pos_x += it->rect().width();
		h = qMax(h, it->rect().height());
	}
	QRectF r;
	r.setHeight(h);
	r.setWidth(pos_x);
	//qfInfo() << r.left() << r.top() << r.width() << r.height();
	setRect(r);
	m_header->updateGeometry();
}

void StartSlotItem::setClassAreaWidth(int px)
{
	QRectF r = rect();
	r.setRight(px);
	setRect(r);
}

void StartSlotItem::dragEnterEvent(QGraphicsSceneDragDropEvent *event)
{
	event->setAccepted(true);
	m_dragIn = true;
	update();
}

void StartSlotItem::dragMoveEvent(QGraphicsSceneDragDropEvent *event)
{
	event->setAccepted(true);
}

void StartSlotItem::dragLeaveEvent(QGraphicsSceneDragDropEvent *event)
{
	Q_UNUSED(event);
	m_dragIn = false;
	update();
}

void StartSlotItem::dropEvent(QGraphicsSceneDragDropEvent *event)
{
	qfLogFuncFrame();
	QJsonDocument jsd = QJsonDocument::fromJson(event->mimeData()->text().toUtf8());
	QVariantMap m = jsd.toVariant().toMap();
	Qt::DropAction act = (m.isEmpty())? Qt::IgnoreAction: Qt::MoveAction;
	event->setDropAction(act);
	event->accept();

	int slot1_ix = m.value(QStringLiteral("slotIndex"), -1).toInt();
	int class1_ix = m.value(QStringLiteral("classIndex"), -1).toInt();
	int slot2_ix = ganttItem()->startSlotItemIndex(this);
	int class2_ix = classItemCount();
	qfDebug() << "DROP class:" << slot1_ix << class1_ix;
	ganttItem()->moveClassItem(slot1_ix, class1_ix, slot2_ix, class2_ix);

	m_dragIn = false;
	update();
}
