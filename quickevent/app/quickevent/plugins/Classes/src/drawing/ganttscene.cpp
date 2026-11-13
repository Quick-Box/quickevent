#include "ganttitem.h"
#include "startslotitem.h"
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

void GanttScene::load(int stage_id, const QString &start_code)
{
	clear();
	const auto &stage_data = getPlugin<EventPlugin>()->stageConfig(stage_id);
	m_useAllMaps = stage_data.useAllMaps;
	m_ganttItem = new GanttItem();
	addItem(m_ganttItem);
	m_startCode = start_code;
	m_ganttItem->load(stage_id, m_startCode);
	m_stageId = stage_id;
	setDirty(false);
}

void GanttScene::setIntervalsLocked(bool locked)
{
	if (m_intervalsLocked == locked)
		return;
	m_intervalsLocked = locked;
	if (!m_ganttItem)
		return;
	for (int i = 0; i < m_ganttItem->startSlotItemCount(); ++i)
		m_ganttItem->startSlotItemAt(i)->updateGeometry();
}

void GanttScene::save()
{
	if(m_ganttItem) {
		m_ganttItem->save(m_stageId, m_startCode);
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

