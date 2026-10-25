#include "runstableitemdelegate.h"
#include "runstablemodel.h"

#include <qassert.h>
#include <quickevent/core/og/timems.h>

#include <qf/gui/tableview.h>
#include <qf/gui/framework/mainwindow.h>

#include <qf/gui/model/sqltablemodel.h>
#include <qf/core/sql/connection.h>
#include <qf/core/sql/query.h>
#include <qf/core/log.h>
#include <qf/core/assert.h>
#include <plugins/Event/src/eventplugin.h>

#include <QPainter>
#include <QLineEdit>

using qf::gui::framework::getPlugin;
using Event::EventPlugin;

RunsTableItemDelegate::RunsTableItemDelegate(qf::gui::TableView * parent)
	: Super(parent)
{
}

QWidget *RunsTableItemDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	if(index.column() == RunsTableModel::col_runs_startGateTime || index.column() == RunsTableModel::col_runs_finishGateTime) {
		return new QLineEdit(parent);
	}
	return Super::createEditor(parent, option, index);
}

void RunsTableItemDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
	if(index.column() == RunsTableModel::col_runs_startGateTime
	|| index.column() == RunsTableModel::col_runs_corridorTime
	|| index.column() == RunsTableModel::col_runs_finishGateTime) {
		auto *le = qobject_cast<QLineEdit*>(editor);
		if(le) {
			QDateTime dt = index.data(Qt::EditRole).toDateTime();
			if(dt.isValid()) {
				auto *runs_model = qobject_cast<RunsTableModel*>(view()->tableModel());
				Q_ASSERT(runs_model);
				QDateTime stage_start = getPlugin<EventPlugin>()->stageStartDateTime(runs_model->stageId());
				int msec = static_cast<int>(stage_start.msecsTo(dt));
				le->setText(quickevent::core::og::TimeMs(msec).toString());
			} else {
				le->clear();
			}
		}
		return;
	}
	Super::setEditorData(editor, index);
}

void RunsTableItemDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
	if(index.column() == RunsTableModel::col_runs_startGateTime
	|| index.column() == RunsTableModel::col_runs_corridorTime
	|| index.column() == RunsTableModel::col_runs_finishGateTime) {
		auto *le = qobject_cast<QLineEdit*>(editor);
		if(le) {
			const QString text = le->text().trimmed();
			if(text.isEmpty()) {
				model->setData(index, QVariant());
			} else {
				auto ms = quickevent::core::og::TimeMs::fromString(text);
				if(ms.isValid()) {
					auto *runs_model = qobject_cast<RunsTableModel*>(view()->tableModel());
					Q_ASSERT(runs_model);
					auto dt = getPlugin<EventPlugin>()->stageStartDateTime(runs_model->stageId())
						.addMSecs(ms.msec());
					model->setData(index, dt);
				}
			}
		}
		return;
	}
	Super::setModelData(editor, model, index);
}

void RunsTableItemDelegate::setHighlightedClassId(int class_id, int stage_id)
{
	//qfWarning() << stage_id << class_id;
	m_stageId = stage_id;
	m_highlightedClassId = class_id;
	reloadHighlightedClassId();
}

void RunsTableItemDelegate::reloadHighlightedClassId()
{
	bool is_relays = getPlugin<EventPlugin>()->eventConfig().isRelays();
	m_classDef.load(m_highlightedClassId, m_stageId, is_relays);
	//qfInfo() << "interval:" << m_classInterval << "first:" << m_classStartFirst << "last:" << m_classStartLast;
}

void RunsTableItemDelegate::paintBackground(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	Super::paintBackground(painter, option, index);

	qf::gui::TableView *v = view();
	if(!v)
		return;

	auto *m = v->model();
	auto *tm = qobject_cast<RunsTableModel*>(v->tableModel());
	if(!(m && tm))
		return;
	bool is_relays = getPlugin<EventPlugin>()->eventConfig().isRelays();
	if(m_highlightedClassId > 0) {
		if(is_relays) {
			// check that start time in classes is the same
			if(index.column() == RunsTableModel::Columns::col_runs_startTimeMs) {
				QVariant stime_v = m->data(index.sibling(index.row(), RunsTableModel::Columns::col_runs_startTimeMs), Qt::EditRole);
				auto stime = stime_v.value<quickevent::core::og::TimeMs>();
				int start_ms = stime.msec();
				if(!stime.isValid())
					return;
				int leg = m->data(index.sibling(index.row(), RunsTableModel::Columns::col_runs_leg), Qt::EditRole).toInt();
				if(leg == 1 && m_classDef.classStartFirst != start_ms) {
					//qfInfo() << m_highlightedClassId << m_classStartFirst << start_ms;
					QColor c = Qt::red;
					c.setAlphaF(0.5);
					painter->fillRect(option.rect, c);
				}
			}
		}
		else if(isStartTimeHighlightVisible() && m_classDef.classInterval > 0) {
			//qfInfo() << "col:" << index.column() << m_highlightedClassId << "interval:" << m_classInterval << isStartTimeHighlightVisible();
			QVariant stime_v = m->data(index.sibling(index.row(), RunsTableModel::Columns::col_runs_startTimeMs), Qt::EditRole);
			auto stime = stime_v.value<quickevent::core::og::TimeMs>();
			int start_ms = stime.msec();
			if(!stime.isValid())
				return;
			int prev_start_ms = m_classDef.classStartFirst;
			int next_start_ms = m_classDef.classStartLast;
			//int table_row = v->toTableModelRowNo(index.row());
			QString club = m->data(index.sibling(index.row(), RunsTableModel::Columns::col_registration), Qt::EditRole).toString().mid(0, 3).trimmed();
			QString prev_club;
			// prev/next are meant in start time order, rows are reversed when sorted descending
			bool is_desc = isStartTimeSortDescending();
			int last_row = tm->rowCount() - 1;
			int prev_row = is_desc ? index.row() + 1 : index.row() - 1;
			int next_row = is_desc ? index.row() - 1 : index.row() + 1;
			bool is_first = is_desc ? index.row() == last_row : index.row() == 0;
			bool is_last = is_desc ? index.row() == 0 : index.row() == last_row;
			if(!is_first) {
				prev_start_ms = m->data(index.sibling(prev_row, RunsTableModel::Columns::col_runs_startTimeMs), Qt::EditRole).value<quickevent::core::og::TimeMs>().msec();
				prev_club = m->data(index.sibling(prev_row, RunsTableModel::Columns::col_registration), Qt::EditRole).toString().mid(0, 3).trimmed();
			}
			if(!is_last) {
				next_start_ms = m->data(index.sibling(next_row, RunsTableModel::Columns::col_runs_startTimeMs), Qt::EditRole).value<quickevent::core::og::TimeMs>().msec();
			}

			bool bad_start_time = (start_ms > m_classDef.classStartFirst && start_ms == prev_start_ms) || ((start_ms - prev_start_ms) % m_classDef.classInterval) != 0;
			bool vacant_before = !bad_start_time && (((start_ms - prev_start_ms) > m_classDef.classInterval) || (is_first && (start_ms > m_classDef.classStartFirst)));
			bool vacant_after = !bad_start_time
					&& (((next_start_ms - start_ms) > m_classDef.classInterval)
						|| (is_last && (start_ms < m_classDef.classStartLast)));
			bool bad_club = !vacant_before && (club == prev_club);
			//auto cd = tm->columnDefinition(index.column());
			if(index.column() == RunsTableModel::Columns::col_runs_startTimeMs) {
				//qfWarning() << ix.row() << club << prev_club;
				//qfInfo() << prev_start_ms << start_ms << (start_ms - prev_start_ms) << m_classInterval;
				if(bad_start_time) {
					QColor c = Qt::red;
					c.setAlphaF(0.5);
					painter->fillRect(option.rect, c);
				}
				/*
				else if(vacant_before) {
					QColor c = QColor("lime");
					painter->fillRect(option.rect, c);
				}
				*/
				else if(bad_club) {
					//qfInfo() << (start_ms - prev_start_ms) << m_classInterval;
					auto c = QColor(Qt::magenta);
					painter->fillRect(option.rect, c);
				}
			}
			static const QColor vacant_color("forestgreen");
			static constexpr int vacant_h_ratio = 20;
			if(is_desc ? vacant_after : vacant_before) {
				QRect r = option.rect;
				int h = r.height() / vacant_h_ratio + 1;
				r.setHeight(h);
				painter->fillRect(r, vacant_color);
			}
			if(is_desc ? vacant_before : vacant_after) {
				QRect r = option.rect;
				int h = r.height() / vacant_h_ratio + 1;
				r.setY(r.bottom() - h);
				//r.setHeight(h);
				painter->fillRect(r, vacant_color);
			}
		}
	}
}
