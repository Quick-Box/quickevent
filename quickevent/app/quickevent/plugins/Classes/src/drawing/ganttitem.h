#ifndef DRAWING_GANTTITEM_H
#define DRAWING_GANTTITEM_H

#include "classitem.h"

#include <qf/core/utils.h>
#include <qf/core/exception.h>

#include <QGraphicsRectItem>
#include <QVariantList>

namespace Event {
class EventPlugin;
}
namespace drawing {

class StartSlotItem;
class ClassItem;
class GanttRuler;

class DrawingConfig : public QVariantMap
{
public:
	explicit DrawingConfig(const QVariantMap &m = QVariantMap()) : QVariantMap(m) {}

	QF_VARIANTMAP_FIELD(QVariantList, s, setS, tartSlots)
};

class GanttItem : public QGraphicsRectItem, public IGanttItem
{
private:
	typedef QGraphicsRectItem Super;
public:
	GanttItem(QGraphicsItem * parent = nullptr);

	void load(int stage_id);
	void save(int stage_id);

	void updateGeometry();
	void checkClassClash();
	void highlightClash(const ClassClash &clash);
	void setClashTypesToCheck(const QSet<ClassItem::ClashType> &clash_types);

	void moveClassItem(int from_slot_ix, int from_class_ix, int to_slot_ix, int to_class_ix);
	void moveStartSlotItem(int from_slot_ix, int to_slot_ix);

	int startSlotItemCount() const { return m_startSlotItems.count(); }
	int startSlotItemIndex(const StartSlotItem *it) const;
	StartSlotItem* startSlotItemAt(int ix, bool throw_ex = qf::core::Exception::Throw);
	StartSlotItem* takeStartSlotItemAt(int ix);

	void insertStartSlotItem(int ix, StartSlotItem *it);
	StartSlotItem* addStartSlotItem();
private:
	QList<StartSlotItem*> m_startSlotItems;
	DrawingConfig m_drawingConfig;
	GanttRuler *m_ganttRuler;
	QSet<ClassItem::ClashType> m_clashTypesToCheck;
};

}

#endif // DRAWING_GANTTITEM_H
