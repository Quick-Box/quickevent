#ifndef DRAWING_GANTTSCENE_H
#define DRAWING_GANTTSCENE_H

#include "classitem.h"

#include <QGraphicsScene>

namespace drawing {

class GanttItem;

class GanttScene : public QGraphicsScene
{
	Q_OBJECT
private:
	typedef QGraphicsScene Super;
signals:
	void clashesChanged(const QList<drawing::ClassClash> &clashes);
public:
	GanttScene(QObject * parent = 0);

	void load(int stage_id);
	void save();

	GanttItem *ganttItem() { return m_ganttItem; }

	void emitClashesChanged(const QList<ClassClash> &clashes) { emit clashesChanged(clashes); }

	/**
	 * @brief displayUnit
	 * @return default font line spacing / 2
	 */
	int displayUnit() const {return m_displayUnit;}
	void setDisplayUnit(int display_unit) {m_displayUnit = display_unit;}

	/// height of every slot row (and class item), so that empty slots are not smaller
	int rowHeight() const {return 6 * m_displayUnit + m_displayUnit / 2;}

	int pxToMin(int px) const;
	int minToPx(int min) const;
	int duToMin(int n) const;

	bool isUseAllMaps() const {return m_useAllMaps;}

	/// layout was changed and not saved to the database yet
	bool isDirty() const {return m_dirty;}
	void setDirty(bool b) {m_dirty = b;}
private:
	int m_stageId = -1;
	int m_displayUnit;
	GanttItem *m_ganttItem = nullptr;
	bool m_useAllMaps = false;
	bool m_dirty = false;
};

}

#endif
