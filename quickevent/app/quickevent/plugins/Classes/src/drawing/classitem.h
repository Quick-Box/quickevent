#ifndef DRAWING_CLASSITEM_H
#define DRAWING_CLASSITEM_H

#include "iganttitem.h"

#include <qf/core/utils.h>

#include <QGraphicsRectItem>
#include <QApplication>

namespace qf {
namespace core {
namespace sql {
class Query;
}
}
}
namespace drawing {

class GanttScene;
class StartSlotItem;
class GanttItem;

class ClassData : public QVariantMap
{
public:
	static constexpr int INVALID_START_TIME_SEC = -999999;
private:
	typedef QVariantMap Super;

	QF_VARIANTMAP_FIELD(int, i, setI, d)
	QF_VARIANTMAP_FIELD(QString, c, setC, lassName)
	QF_VARIANTMAP_FIELD(int, c, setC, lassId)
	QF_VARIANTMAP_FIELD(QString, c, setC, ourseName)
	QF_VARIANTMAP_FIELD(int, c, setC, ourseId)
	QF_VARIANTMAP_FIELD2(int, s, setS, tartSlotIndex, -1)
	QF_VARIANTMAP_FIELD2(int, c, setC, lassIndex, -1) ///< not SQL value, internal usage
	QF_VARIANTMAP_FIELD(int, s, setS, tartTimeMin)
	QF_VARIANTMAP_FIELD(int, s, setS, tartIntervalMin)
	QF_VARIANTMAP_FIELD(int, v, setV, acantsBefore)
	QF_VARIANTMAP_FIELD(int, v, setV, acantEvery)
	QF_VARIANTMAP_FIELD(int, v, setV, acantsAfter)
	QF_VARIANTMAP_FIELD(int, f, setF, irstCode)
	QF_VARIANTMAP_FIELD(int, r, setR, unsCount)
	QF_VARIANTMAP_FIELD(int, m, setM, apCount)
public:
	ClassData(const QVariantMap &data = QVariantMap()) : QVariantMap(data) {}
	ClassData(const qf::core::sql::Query &q);
};

class ClassItem : public QGraphicsRectItem, public IGanttItem
{
	Q_DECLARE_TR_FUNCTIONS(drawing::ClassItem)
private:
	typedef QGraphicsRectItem Super;
public:
	enum class ClashType {None, CourseOverlap, RunnersOverlap};
public:
	ClassItem(QGraphicsItem * parent = nullptr);

	void updateGeometry();
	void updateToolTip();
	QList<ClassItem*> findClashes(const QSet<ClashType> &clash_types);
	ClashType clashWith(ClassItem *other) const;
	QList<ClassItem *> clashingClasses() const;
	void setClashingClasses(const QList<ClassItem *> &clashing_classes);

	const ClassData& data() const;
	void setData(const ClassData &data);

	int durationMin() const;

	/// zero duration classes (no start interval) keep zero layout width,
	/// but are painted and hit-tested as a narrow flag so they stay visible
	QRectF boundingRect() const Q_DECL_OVERRIDE;
	QPainterPath shape() const Q_DECL_OVERRIDE;

	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) Q_DECL_OVERRIDE;

	void mousePressEvent(QGraphicsSceneMouseEvent *event) Q_DECL_OVERRIDE;
	void mouseMoveEvent(QGraphicsSceneMouseEvent *event) Q_DECL_OVERRIDE;
	void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) Q_DECL_OVERRIDE;

	void dragEnterEvent(QGraphicsSceneDragDropEvent *event) Q_DECL_OVERRIDE;
	void dragMoveEvent(QGraphicsSceneDragDropEvent *event) Q_DECL_OVERRIDE;
	void dragLeaveEvent(QGraphicsSceneDragDropEvent *event) Q_DECL_OVERRIDE;
	void dropEvent(QGraphicsSceneDragDropEvent *event) Q_DECL_OVERRIDE;

	void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) Q_DECL_OVERRIDE;
protected:
	int runsAndVacantCount() const;
	int markerWidth() const;
	QColor color() const;
	const StartSlotItem* startSlotItem() const;
	StartSlotItem* startSlotItem();
	//QPair<int, int> ganttIndex() const;
private:
	ClassData m_data;
	QGraphicsTextItem *m_classText;
	QGraphicsTextItem *m_courseText;
	QGraphicsTextItem *m_classdefsText;
	QVariant m_dropInsertsBefore;
	QList<ClassItem*> m_clashingClasses;
};

struct ClassClash
{
	ClassItem *class1 = nullptr;
	ClassItem *class2 = nullptr;
	ClassItem::ClashType type = ClassItem::ClashType::None;
};

}

#endif // DRAWING_CLASSITEM_H
