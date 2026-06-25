#ifndef DRAWING_STARTSLOTITEM_H
#define DRAWING_STARTSLOTITEM_H

#include "iganttitem.h"

#include <qf/core/utils.h>
#include <qf/core/exception.h>

#include <QGraphicsRectItem>

namespace drawing {

class ClassItem;
class GanttItem;
class StartSlotHeader;

class StartSlotData : public QVariantMap
{
public:
	explicit StartSlotData(const QVariantMap &m = QVariantMap()) : QVariantMap(m) {}

	QF_VARIANTMAP_FIELD(int, s, setS, tartOffset)
	//QF_VARIANTMAP_FIELD(bool, is, set, Locked)
	QF_VARIANTMAP_FIELD(bool, is, set, IgnoreClassClashCheck)
};

class StartSlotItem : public QGraphicsRectItem, public IGanttItem
{
private:
	typedef QGraphicsRectItem Super;
public:
	StartSlotItem(QGraphicsItem * parent = 0);

	QF_FIELD_IMPL2(int, s, S, lotNumber, 0)

public:
	ClassItem* addClassItem();
	int classItemCount() const;
	int classItemIndex(const ClassItem *it) const;
	void insertClassItem(int ix, ClassItem *it);
	ClassItem* classItemAt(int ix, bool throw_ex = qf::core::Exception::Throw);
	ClassItem* takeClassItemAt(int ix);

	void setStartOffset(int start_offset);
	int startOffset() const;

	void setStartInterval(int interval_min);
	int startInterval() const; ///< interval of the slot's first class, -1 when the slot is empty
	bool isStartIntervalUniform() const; ///< all classes in the slot share the same start interval

	//void setLocked(bool b);
	//bool isLocked() const;

	bool isIgnoreClassClashCheck() const;
	void setIgnoreClassClashCheck(bool b);

	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) Q_DECL_OVERRIDE;

	void updateGeometry();
	void setClassAreaWidth(int px);

	const StartSlotData& data() const;
	void setData(const StartSlotData &data);

	void dragEnterEvent(QGraphicsSceneDragDropEvent *event) Q_DECL_OVERRIDE;
	void dragMoveEvent(QGraphicsSceneDragDropEvent *event) Q_DECL_OVERRIDE;
	void dragLeaveEvent(QGraphicsSceneDragDropEvent *event) Q_DECL_OVERRIDE;
	void dropEvent(QGraphicsSceneDragDropEvent *event) Q_DECL_OVERRIDE;
private:
	StartSlotData m_data;
	QList<ClassItem*> m_classItems;
	StartSlotHeader *m_header;
	bool m_dragIn = false;
};

}

#endif // DRAWING_STARTSLOTITEM_H
