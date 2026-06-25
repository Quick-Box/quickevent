#ifndef DRAWING_STARTSLOTHEADER_H
#define DRAWING_STARTSLOTHEADER_H

#include "iganttitem.h"

#include <QGraphicsRectItem>
#include <QCoreApplication>

namespace drawing {

class StartSlotItem;

class StartSlotHeader : public QGraphicsRectItem, public IGanttItem
{
	Q_DECLARE_TR_FUNCTIONS(drawing::StartSlotHeader)
private:
	typedef QGraphicsRectItem Super;
public:
	StartSlotHeader(StartSlotItem *parent = 0);

	StartSlotItem* startSlotItem();
	int minHeight();
	void updateGeometry();
protected:
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0) Q_DECL_OVERRIDE;

	void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) Q_DECL_OVERRIDE;

	void mousePressEvent(QGraphicsSceneMouseEvent *event) Q_DECL_OVERRIDE;
	void mouseMoveEvent(QGraphicsSceneMouseEvent *event) Q_DECL_OVERRIDE;
	void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) Q_DECL_OVERRIDE;

	void dragEnterEvent(QGraphicsSceneDragDropEvent *event) Q_DECL_OVERRIDE;
	void dragMoveEvent(QGraphicsSceneDragDropEvent *event) Q_DECL_OVERRIDE;
	void dragLeaveEvent(QGraphicsSceneDragDropEvent *event) Q_DECL_OVERRIDE;
	void dropEvent(QGraphicsSceneDragDropEvent *event) Q_DECL_OVERRIDE;
private:
	QGraphicsTextItem *m_textSlotNo;
	QGraphicsRectItem *m_lockItem;
	QGraphicsRectItem *m_startSpinner = nullptr;
	QGraphicsRectItem *m_intervalSpinner = nullptr;
	bool m_dragIn = false;
};

} // namespace drawing

#endif // DRAWING_STARTSLOTHEADER_H
