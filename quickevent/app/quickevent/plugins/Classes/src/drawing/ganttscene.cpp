#include "ganttitem.h"
#include "ganttscene.h"

#include <qf/gui/framework/mainwindow.h>

#include <qf/core/assert.h>

#include <QFontMetrics>

#include <plugins/Event/src/eventplugin.h>

using namespace drawing;
using qf::gui::framework::getPlugin;
using Event::EventPlugin;

GanttScene::GanttScene(QObject * parent)
	: Super(parent)
{
	setDisplayUnit(QFontMetrics(QFont()).lineSpacing() / 2);

}

void GanttScene::load(int stage_id)
{
	clear();
	Event::StageData stage_data = getPlugin<EventPlugin>()->stageData(stage_id);
	m_useAllMaps = stage_data.isUseAllMaps();
	m_ganttItem = new GanttItem();
	addItem(m_ganttItem);
	m_ganttItem->load(stage_id);
	m_stageId = stage_id;
	setDirty(false);
}

void GanttScene::save()
{
	if(m_ganttItem) {
		m_ganttItem->save(m_stageId);
		setDirty(false);
	}
}

int GanttScene::duToMin(int n) const
{
	return pxToMin(n * displayUnit());
}

int GanttScene::pxToMin(int px) const
{
	static int px_per_min = minToPx(1);
	return px / px_per_min;
}

int GanttScene::minToPx(int min) const
{
	// shoul be always natural number;
	static int px_per_min = 3 * displayUnit() / 2;
	return min * px_per_min;
}

