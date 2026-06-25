#include "ganttitem.h"
#include "startslotitem.h"
#include "classitem.h"
#include "ganttruler.h"
#include "ganttscene.h"

#include <qf/gui/framework/mainwindow.h>

#include <qf/core/sql/query.h>
#include <qf/core/sql/connection.h>
#include <qf/core/sql/querybuilder.h>
#include <qf/core/assert.h>
#include <plugins/Event/src/eventplugin.h>

#include <QJsonDocument>

namespace qfs = qf::core::sql;
using qf::gui::framework::getPlugin;
using Event::EventPlugin;

using namespace drawing;

GanttItem::GanttItem(QGraphicsItem *parent)
	: Super(parent), IGanttItem(this)
{
	m_ganttRuler = new GanttRuler(this);
}

int GanttItem::startSlotItemIndex(const StartSlotItem *it) const
{
	return m_startSlotItems.indexOf(it);
}

StartSlotItem *GanttItem::startSlotItemAt(int ix, bool throw_ex)
{
	StartSlotItem *ret = m_startSlotItems.value(ix);
	if(!ret && throw_ex)
		QF_EXCEPTION(QString("Invalid item index %1, item count %2!").arg(ix).arg(startSlotItemCount()));
	return ret;
}

StartSlotItem *GanttItem::takeStartSlotItemAt(int ix)
{
	StartSlotItem *ret = m_startSlotItems.takeAt(ix);
	ret->setParentItem(nullptr);
	return ret;
}

void GanttItem::insertStartSlotItem(int ix, StartSlotItem *it)
{
	QF_ASSERT(it != nullptr, "Item == NULL", return);
	m_startSlotItems.insert(ix, it);
	it->setParentItem(this);
}

StartSlotItem *GanttItem::addStartSlotItem()
{
	auto *it = new StartSlotItem(this);
	insertStartSlotItem(startSlotItemCount(), it);
	return it;
}

void GanttItem::load(int stage_id)
{
	qfLogFuncFrame();
	Event::StageData stage_data = getPlugin<EventPlugin>()->stageData(stage_id);
	DrawingConfig dc(stage_data.drawingConfig());
	QVariantList start_slot_list = dc.startSlots();

	qfs::Query q(qfs::Connection::forName());
	qf::core::sql::QueryBuilder qb1;
	qb1.select("competitors.classId")
			.select("COUNT(runs.competitorId) AS runsCount")
			.select("MIN(runs.startTimeMs) AS minStartTime")
			.select("MAX(runs.startTimeMs) AS maxStartTime")
			.from("competitors")
			.joinRestricted("competitors.id", "runs.competitorId", "runs.isRunning AND runs.stageId=" QF_IARG(stage_id))
			.groupBy("competitors.classId")
			.as("classruns");
	qf::core::sql::QueryBuilder qb;
	qb.select2("classdefs", "*")
			.select2("classes", "name AS className")
			.select2("courses", "name AS courseName")
			.select2("codes", "code AS firstCode")
			.select2("classruns", "runsCount, minStartTime, maxStartTime")
			.from("classdefs")
			.join("classdefs.classId", "classes.id")
			.join("classdefs.courseId", "courses.id")
			.joinRestricted("classdefs.courseId", "coursecodes.courseId", "coursecodes.position=1")
			.join("coursecodes.codeId", "codes.id")
			.joinQuery("classdefs.classId", qb1, "classId")
			.where("classdefs.stageId=" QF_IARG(stage_id))
			.orderBy("startSlotIndex, startTimeMin, firstCode, classdefs.courseId");
	int curr_course_id = -1;
	int curr_slot_ix = -1;
	bool curr_slot_is_drawn = false;
	int curr_slot_end_min = 0;
	StartSlotItem *slot_item = nullptr;
	QString qs = qb.toString();
	qfDebug() << qs;
	q.exec(qs);
	while(q.next()) {
		ClassData cd(q);
		// classdefs.startTimeMin is authoritative, it can be edited in the classes
		// table between the draw tool sessions, slot offsets stored in drawingConfig
		// can be stale, so the layout is always positioned by the class start times
		int start_time_min = cd.startTimeMin();
		int slot_ix = cd.startSlotIndex();
		if(slot_ix >= 0) {
			if(slot_ix > curr_slot_ix) {
				slot_item = addStartSlotItem();
				curr_slot_ix = slot_ix;
				StartSlotData sd(start_slot_list.value(slot_ix).toMap());
				if(!cd.value(QStringLiteral("startTimeMin")).isNull())
					sd.setStartOffset(start_time_min);
				slot_item->setData(sd);
				curr_slot_end_min = sd.startOffset();
				curr_slot_is_drawn = true;
				curr_course_id = -1;
			}
		}
		else {
			// class was never laid out by the draw tool, place it to its start time,
			// chain it to the previous class only if it continues the same course
			// without a gap (or has no start time set yet)
			bool chains_to_curr_slot = !curr_slot_is_drawn
					&& slot_item != nullptr
					&& cd.courseId() == curr_course_id
					&& (start_time_min == 0 || start_time_min == curr_slot_end_min);
			if(!chains_to_curr_slot) {
				slot_item = addStartSlotItem();
				StartSlotData sd;
				sd.setStartOffset(start_time_min);
				slot_item->setData(sd);
				curr_slot_end_min = start_time_min;
				curr_slot_is_drawn = false;
			}
			curr_course_id = cd.courseId();
		}
		if (slot_item) {
			auto *class_it = slot_item->addClassItem();
			class_it->setData(cd);
			curr_slot_end_min += class_it->durationMin();
		}
		else {
			QF_EXCEPTION("internal error");
		}
	}
	updateGeometry();
	checkClassClash();
}

void GanttItem::save(int stage_id)
{
	qfLogFuncFrame();
	{
		Event::StageData stage = getPlugin<EventPlugin>()->stageData(stage_id);
		DrawingConfig dc(stage.drawingConfig());
		QVariantList start_slots;
		for (int i = 0; i < startSlotItemCount(); ++i) {
			StartSlotItem *slot_it = startSlotItemAt(i);
			StartSlotData sd = slot_it->data();
			start_slots << sd;
		}
		dc.setStartSlots(start_slots);
		auto dc_str = qf::core::Utils::qvariantToJson(dc);

		QString qs = "UPDATE stages SET drawingConfig=:drawingConfig WHERE id=:id";
		qfs::Query q(qfs::Connection::forName());
		q.prepare(qs, qf::core::Exception::Throw);
		q.bindValue(":drawingConfig", dc_str);
		q.bindValue(":id", stage_id);
		q.exec(qf::core::Exception::Throw);
		getPlugin<EventPlugin>()->clearStageDataCache();
	}
	{
		QString qs = "UPDATE classdefs SET"
					 "  startSlotIndex=:startSlotIndex,"
					 "  startTimeMin=:startTimeMin,"
					 "  startIntervalMin=:startIntervalMin"
					 " WHERE classdefs.id=:id AND stageId=:stageId";
		qfs::Query q(qfs::Connection::forName());
		q.prepare(qs, qf::core::Exception::Throw);
		for (int i = 0; i < startSlotItemCount(); ++i) {
			StartSlotItem *slot_it = startSlotItemAt(i);
			for (int j = 0; j < slot_it->classItemCount(); ++j) {
				ClassItem *class_it = slot_it->classItemAt(j);
				auto dt = class_it->data();
				qfDebug() << dt.id() << dt.className() << "slot:" << dt.startSlotIndex() << "start time:" << dt.startTimeMin();
				q.bindValue(":startSlotIndex", dt.startSlotIndex());
				q.bindValue(":startTimeMin", dt.startTimeMin());
				q.bindValue(":startIntervalMin", dt.startIntervalMin());
				//q.bindValue(":isDrawnIn", dt.isDrawnIn());
				q.bindValue(":id", dt.id());
				q.bindValue(":stageId", stage_id);
				q.exec(qf::core::Exception::Throw);
			}
		}
	}
}

void GanttItem::updateGeometry()
{
	qfLogFuncFrame();
	int pos_y = 0;
	double w = 0;
	for (int i = 0; i < startSlotItemCount(); ++i) {
		StartSlotItem *it = startSlotItemAt(i);
		it->setSlotNumber(i + 1);
		it->updateGeometry();
		it->setPos(QPoint(0, pos_y));
		qfDebug() << i << it->rect().height() << it;
		pos_y += it->rect().height();
		w = qMax(w, it->rect().right());
	}
	for (int i = 0; i < startSlotItemCount(); ++i) {
		StartSlotItem *it = startSlotItemAt(i);
		it->setClassAreaWidth(w);
	}
	QRectF r;
	r.setWidth(w);
	r.setHeight(pos_y);
	{
		QRectF rr = r;
		rr.setHeight(3 * ganttScene()->displayUnit());
		rr.moveTop(-rr.height());
		m_ganttRuler->setRect(rr);
		r |= rr;
	}
	setRect(r);
}

void GanttItem::checkClassClash()
{
	QList<ClassClash> clashes;
	for (int i = 0; i < startSlotItemCount(); ++i) {
		StartSlotItem *slot_it = startSlotItemAt(i);
		if(slot_it->data().isIgnoreClassClashCheck())
			continue;
		for (int j = 0; j < slot_it->classItemCount(); ++j) {
			ClassItem *class_it = slot_it->classItemAt(j);
			const auto clashing_class_list = class_it->findClashes(m_clashTypesToCheck);
			for(ClassItem *other : clashing_class_list) {
				// every pair is found from both sides, record it once
				if(quintptr(class_it) < quintptr(other))
					clashes << ClassClash{class_it, other, class_it->clashWith(other)};
			}
		}
	}
	// the visualization marks just one selected clash, the widget picks it
	// from this list and calls highlightClash()
	ganttScene()->emitClashesChanged(clashes);
}

void GanttItem::highlightClash(const ClassClash &clash)
{
	for (int i = 0; i < startSlotItemCount(); ++i) {
		StartSlotItem *slot_it = startSlotItemAt(i);
		for (int j = 0; j < slot_it->classItemCount(); ++j) {
			ClassItem *class_it = slot_it->classItemAt(j);
			class_it->setClashingClasses({});
		}
	}
	if(clash.class1 && clash.class2) {
		clash.class1->setClashingClasses({clash.class2});
		clash.class2->setClashingClasses({clash.class1});
	}
}

void GanttItem::setClashTypesToCheck(const QSet<ClassItem::ClashType> &clash_types)
{
	m_clashTypesToCheck = clash_types;
	checkClassClash();
}

void GanttItem::moveClassItem(int from_slot_ix, int from_class_ix, int to_slot_ix, int to_class_ix)
{
	if(from_slot_ix == to_slot_ix && (from_class_ix == to_class_ix || to_class_ix == from_class_ix + 1))
		return;
	if(from_slot_ix == to_slot_ix && from_class_ix < to_class_ix)
		to_class_ix--;
	auto *slot1 = startSlotItemAt(from_slot_ix);
	auto *class_it = slot1->takeClassItemAt(from_class_ix);
	if(!class_it)
		return;
	auto *slot2 = startSlotItemAt(to_slot_ix);
	slot2->insertClassItem(to_class_ix, class_it);
	ganttScene()->setDirty(true);
	updateGeometry();
	checkClassClash();
}

void GanttItem::moveStartSlotItem(int from_slot_ix, int to_slot_ix)
{
	if(from_slot_ix == to_slot_ix)
		return;
	auto *slot = takeStartSlotItemAt(from_slot_ix);
	if(slot) {
		if(from_slot_ix < to_slot_ix)
			to_slot_ix--;
		insertStartSlotItem(to_slot_ix, slot);
		ganttScene()->setDirty(true);
		updateGeometry();
		checkClassClash();
	}
}





