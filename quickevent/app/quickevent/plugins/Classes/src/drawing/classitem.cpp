#include "classitem.h"
#include "ganttscene.h"
#include "startslotitem.h"
#include "ganttitem.h"
#include "drawingganttwidget.h"

#include "../classdefwidget.h"

#include <qf/core/sql/query.h>
#include <qf/core/assert.h>
#include <qf/gui/dialogs/dialog.h>

#include <QDrag>
#include <QJsonDocument>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>
#include <QCursor>
#include <QGraphicsSceneMouseEvent>
#include <QWidget>
#include <QStyleOptionGraphicsItem>
#include <QMenu>
#include <QDialogButtonBox>

namespace qfd = qf::gui::dialogs;
namespace qfc = qf::core;
namespace qfm = qf::gui::model;

namespace drawing {

ClassData::ClassData(const qf::core::sql::Query &q)
{
	for(auto s : {"id",
		"className", "classId",
		"courseName", "courseId",
		//"startSlotIndex",
		"startTimeMin", "startIntervalMin",
		"vacantsBefore", "vacantsBefore", "vacantEvery", "vacantsAfter",
		"firstCode",
		"runsCount", "mapCount"})
	{
		insert(s, q.value(s));
	}
	{
		QVariant v = q.value("startSlotIndex");
		if(!v.isNull())
			setStartSlotIndex(v.toInt());
	}
	/*
	{
		QVariant v = q.value("minStartTime");
		if(!v.isNull())
			setMinStartTimeSec(v.toInt() / 1000);
	}
	{
		QVariant v = q.value("maxStartTime");
		if(!v.isNull())
			setMaxStartTimeSec(v.toInt() / 1000);
	}
	*/
}

ClassItem::ClassItem(QGraphicsItem *parent)
	: Super(parent), IGanttItem(this)
{
	int du_px = ganttScene()->displayUnit();
	m_classText = new QGraphicsTextItem(this);
	m_courseText = new QGraphicsTextItem(this);
	m_courseText->setPos(0, 2 * du_px);
	m_classdefsText = new QGraphicsTextItem(this);
	m_classdefsText->setPos(0, 4 * du_px);
	QRect r;
	r.setHeight(ganttScene()->rowHeight());
	setRect(r);

	setCursor(Qt::ArrowCursor);
	setAcceptDrops(true);

	setFlag(QGraphicsItem::ItemClipsChildrenToShape, true);
	setFlag(QGraphicsItem::ItemIsSelectable, true);
}

const ClassData &ClassItem::data() const
{
	return m_data;
}

void ClassItem::setData(const ClassData &data)
{
	m_data = data;
}

QColor ClassItem::color() const
{
	auto dt = data();
	int hue = dt.firstCode() % 100;
	QColor c;
	c.setHsvF(hue / 100., 1, 1);
	return c;
}

int ClassItem::markerWidth() const
{
	return qMax(ganttScene()->displayUnit() / 2, 4);
}

QRectF ClassItem::boundingRect() const
{
	QRectF r = Super::boundingRect();
	if(r.width() < markerWidth())
		r.setWidth(markerWidth());
	return r;
}

QPainterPath ClassItem::shape() const
{
	QPainterPath p;
	p.addRect(boundingRect());
	return p;
}

const StartSlotItem *ClassItem::startSlotItem() const
{
	return const_cast<ClassItem*>(this)->startSlotItem(); // NOLINT(cppcoreguidelines-pro-type-const-cast)
}

StartSlotItem *ClassItem::startSlotItem()
{
	auto *ret = dynamic_cast<StartSlotItem*>(parentItem());
	QF_ASSERT_EX(ret != nullptr, "Bad parent!");
	return ret;
}
QList<ClassItem *> ClassItem::clashingClasses() const
{
	return m_clashingClasses;
}

void ClassItem::setClashingClasses(const QList<ClassItem *> &clashing_classes)
{
	m_clashingClasses = clashing_classes;
	updateToolTip();
	update();
}

/*
QPair<int, int> ClassItem::ganttIndex() const
{
	int class_ix = startSlotItem()->classItemIndex(this);
	int slot_ix = ganttItem()->startSlotItemIndex(startSlotItem());
	return QPair<int, int>(slot_ix, class_ix);
}
*/
void ClassItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
	QColor c_runner = color();
	QColor c_free = c_runner;
	c_free.setAlpha(64);
	auto r = rect();
	bool is_zero_width = r.width() < markerWidth();
	if(is_zero_width) {
		// paint zero duration classes as a narrow solid flag, see boundingRect()
		r.setWidth(markerWidth());
	}
	QRectF r1 = r;
	r1.setWidth(minToPx(1));
	r1.setHeight(r.height() / 8);
	painter->save();
	painter->fillRect(r, is_zero_width? c_runner: c_free);
	auto dt = data();
	int interval = dt.startIntervalMin();
	for (int i = 0; !is_zero_width && i < runsAndVacantCount(); ++i) {
		r1.moveLeft(minToPx(i * interval));
		//QColor c = (i == 0)? c_first: c_runner;
		painter->fillRect(r1, c_runner);
		painter->drawRect(r1);
	}
	if(clashingClasses().count()) {
		painter->save();
		qreal w = r.height() / 15;
		QPen p;
		p.setColor(Qt::red);
		p.setWidthF(w);
		painter->setPen(p);
		w /= 2;
		QRectF r2 = r.adjusted(w, w, -w, -w);
		painter->drawRect(r2);
		painter->restore();
	}
	if(isSelected()) {
		painter->save();
		qreal w = r.height() / 15;
		QPen p;
		p.setColor(QColor(Qt::blue).lighter());
		p.setWidthF(w);
		painter->setPen(p);
		w /= 2;
		// keep the selection visible inside the clash border
		qreal inset = clashingClasses().count()? 5 * w: w;
		QRectF r2 = r.adjusted(inset, inset, -inset, -inset);
		painter->drawRect(r2);
		painter->restore();
	}
	if(m_dropInsertsBefore.isValid()) {
		QColor c_insert = c_runner;
		//c_insert.setAlpha(128);
		QRectF r2 = r;
		r2.setWidth(minToPx(2));
		if(!m_dropInsertsBefore.toBool()) {
			r2.moveLeft(r.width() - r2.width());
		}
		painter->fillRect(r2, c_insert);
		QPen p;
		p.setColor(color());
		painter->setPen(p);
		painter->drawRect(r2);
	}
	painter->restore();
	Super::paint(painter, option, widget);
}

int ClassItem::runsAndVacantCount() const
{
	auto dt = data();
	int cnt = dt.runsCount();
	if(ganttScene()->isUseAllMaps()) {
		int map_cnt = dt.mapCount();
		cnt = qMax(cnt, map_cnt);
	}
	else {
		int vacants = (dt.vacantEvery() > 0)? cnt / dt.vacantEvery(): 0;
		cnt = dt.vacantsBefore() + cnt + vacants + dt.vacantsAfter();
	}
	return cnt;
}

int ClassItem::durationMin() const
{
	auto dt = data();
	return runsAndVacantCount() * dt.startIntervalMin();
}

void ClassItem::updateGeometry()
{
	ClassData &dt = m_data;
	//qfLogFuncFrame() << dt.className() << "runners:" << dt.runsCount();
	QRectF r = rect();
	r.setWidth(minToPx(durationMin()));
	setRect(r);
	//setBrush(color());
	bool has_width = durationMin() > 0;
	m_classText->setVisible(has_width);
	m_courseText->setVisible(has_width);
	m_classdefsText->setVisible(has_width);
	m_classText->setHtml(QString("<b>%1</b> %2+%3").arg(dt.className()).arg(dt.runsCount()).arg(runsAndVacantCount() - dt.runsCount()));
	m_courseText->setHtml(QString("<b>%1</b> (%2)").arg(dt.firstCode()).arg(dt.courseId()));
	dt.setStartTimeMin(pxToMin(pos().x()));
	m_classdefsText->setPlainText(QString("<%1-%3>/%2")
								  .arg(dt.startTimeMin())
								  .arg(dt.startIntervalMin())
								  .arg(dt.startTimeMin() + durationMin())
								  //.arg((dt.minStartTimeSec() == ClassData::INVALID_START_TIME_SEC)? QString(): QString::number(dt.minStartTimeSec() / 60))
								  //.arg((dt.maxStartTimeSec() == ClassData::INVALID_START_TIME_SEC)? QString(): QString::number(dt.maxStartTimeSec() / 60))
								  );
	int slot_ix = ganttItem()->startSlotItemIndex(startSlotItem());
	int class_ix = startSlotItem()->classItemIndex(this);
	dt.setStartSlotIndex(slot_ix);
	dt.setClassIndex(class_ix);
	updateToolTip();
}

void ClassItem::updateToolTip()
{
	auto dt = data();
	QString tool_tip;
	// HTML tooltip can cause word wrap
	tool_tip = "<html><body>";
	tool_tip += tr("class: <b>%1</b>, %2 runners + %3 vacants = %4<br/>")
			.arg(dt.className())
			.arg(dt.runsCount())
			.arg(runsAndVacantCount() - dt.runsCount())
			.arg(runsAndVacantCount()) ;
	tool_tip += tr("first code <b>%1</b>, course %2 - %3<br/>").arg(dt.firstCode()).arg(dt.courseId()).arg(dt.courseName());
	tool_tip += tr("vacants before: %1, every: %2, after: %3<br/>").arg(dt.vacantsBefore()).arg(dt.vacantEvery()).arg(dt.vacantsAfter());
	tool_tip += tr("class start: %1, interval: %2, duration: %3, end: %4<br/>").arg(dt.startTimeMin()).arg(dt.startIntervalMin()).arg(durationMin()).arg(dt.startTimeMin() + durationMin());
	tool_tip += tr("map count: %1").arg(dt.mapCount());
	//if(dt.minStartTimeSec() != ClassData::INVALID_START_TIME_SEC || dt.maxStartTimeSec() != ClassData::INVALID_START_TIME_SEC) {
	//	tool_tip += tr("competitors start first: %1, last: %2<br/>").arg(dt.minStartTimeSec() / 60).arg(dt.maxStartTimeSec() / 60);
	//}
	auto clst = clashingClasses();
	if(clst.count()) {
		QStringList sl;
		for(auto *cl : clst) {
			sl << cl->data().className();
		}
		tool_tip += tr(", clash with: %1<br/>").arg(sl.join(", "));
	}
	tool_tip += "<br/><i>" + tr("Drag the class to move it to another slot,<br/>right-click to edit its definition.") + "</i>";
	tool_tip += "</body></html>";
	tool_tip.replace(' ', "&nbsp;");
	setToolTip(tool_tip);
}

QList<ClassItem *> ClassItem::findClashes(const QSet<ClashType> &clash_types)
{
	QList<ClassItem*> ret;
	auto *git = ganttItem();
	for (int i = 0; i < git->startSlotItemCount(); ++i) {
		StartSlotItem *slot_it = git->startSlotItemAt(i);
		if(slot_it->data().isIgnoreClassClashCheck())
			continue;
		for (int j = 0; j < slot_it->classItemCount(); ++j) {
			ClassItem *class_it = slot_it->classItemAt(j);
			if(class_it == this)
				continue;
			if(clash_types.contains(this->clashWith(class_it))) {
				ret << class_it;
			}
		}
	}
	return ret;
}

namespace {
int gcd(int a, int b)
{
	while (a != b) {
		if (a > b)
			a = a - b;
		else
			b = b - a;
	}
	return a;
}
}

ClassItem::ClashType ClassItem::clashWith(ClassItem *other) const
{
	qfLogFuncFrame() << data().className() << "vs" << other->data().className();
	auto dt = data();
	auto odt = other->data();
	int t1 = dt.startTimeMin();
	int t2 = t1 + durationMin();
	int ot1 = odt.startTimeMin();
	int ot2 = ot1 + other->durationMin();
	if(ot1 < t2 && ot2 > t1) {
		// overlap
		if(dt.courseId() == odt.courseId()) {
			qfDebug() << "\t ret: CourseOverlap";
			return ClashType::CourseOverlap;
		}
		if(dt.firstCode() == odt.firstCode()) {
			int si = dt.startIntervalMin();
			int osi = odt.startIntervalMin();
			if(si > 0 && osi > 0) {
				int si_gcd = gcd(si, osi);
				if((t1 % si_gcd) == (ot1 % si_gcd)) {
					qfDebug() << "\t ret: 2RunnersOverlap";
					return ClashType::RunnersOverlap;
				}
			}
		}
	}
	qfDebug() << "\t ret: NONE";
	return ClashType::None;
}

void ClassItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
	if(event->button() == Qt::LeftButton) {
		scene()->clearSelection();
		setSelected(true);
	}
}

void ClassItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
	if (QLineF(event->screenPos(), event->buttonDownScreenPos(Qt::LeftButton)).length() < QApplication::startDragDistance()) {
		return;
	}
	qfLogFuncFrame();
	setCursor(Qt::ClosedHandCursor);
	auto *drag = new QDrag(event->widget());
	auto *mime = new QMimeData;
	drag->setMimeData(mime);
	{
		QVariantMap m;
		auto &dt = data();
		int slot_ix = dt.startSlotIndex();
		int class_ix = dt.classIndex();
		m[QStringLiteral("slotIndex")] = slot_ix;
		m[QStringLiteral("classIndex")] = class_ix;
		QJsonDocument jsd = QJsonDocument::fromVariant(m);
		QString mime_text = QString::fromUtf8(jsd.toJson());
		qfDebug() << "mime:" << mime_text;
		mime->setText(mime_text);

		QPixmap pixmap(rect().size().toSize());
		pixmap.fill(Qt::white);

		QPainter painter(&pixmap);
		//painter.translate(15, 15);
		painter.setRenderHint(QPainter::Antialiasing);
		QStyleOptionGraphicsItem opt;
		paint(&painter, &opt, nullptr);
		{
			m_classText->paint(&painter, &opt, nullptr);
			painter.translate(m_courseText->pos());
			m_courseText->paint(&painter, &opt, nullptr);
			painter.translate(m_classdefsText->pos() - m_courseText->pos());
			m_classdefsText->paint(&painter, &opt, nullptr);
		}
		painter.end();
		//pixmap.setMask(pixmap.createHeuristicMask());

		drag->setPixmap(pixmap);
		drag->setHotSpot(QPoint(5 * ganttScene()->displayUnit(), 0));
	}
	Qt::DropAction act = drag->exec();
	qfDebug() << "drag exit:" << act;
	if(act == Qt::MoveAction) {

	}
	setCursor(Qt::ArrowCursor);
}

void ClassItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
	Q_UNUSED(event)
	setCursor(Qt::ArrowCursor);
}

void ClassItem::dragEnterEvent(QGraphicsSceneDragDropEvent *event)
{
	event->setAccepted(true);
	QPointF pos = mapFromScene(event->scenePos());
	m_dropInsertsBefore = (pos.x() < rect().width() / 2)? true: false;
	update();
}

void ClassItem::dragMoveEvent(QGraphicsSceneDragDropEvent *event)
{
	event->setAccepted(true);
	QPointF pos = mapFromScene(event->scenePos());
	bool before = (pos.x() < rect().width() / 2)? true: false;
	//qfInfo() << pos.x() << (rect().width() / 2) << before << m_dropInsertsBefore;
	if(before != m_dropInsertsBefore) {
		m_dropInsertsBefore = before;
		update();
	}
}

void ClassItem::dragLeaveEvent(QGraphicsSceneDragDropEvent *event)
{
	Q_UNUSED(event)
	m_dropInsertsBefore = QVariant();
	update();
}

void ClassItem::dropEvent(QGraphicsSceneDragDropEvent *event)
{
	qfLogFuncFrame();
	QJsonDocument jsd = QJsonDocument::fromJson(event->mimeData()->text().toUtf8());
	QVariantMap m = jsd.toVariant().toMap();
	Qt::DropAction act = (m.isEmpty())? Qt::IgnoreAction: Qt::MoveAction;
	event->setDropAction(act);
	event->accept();

	int slot1_ix = m.value(QStringLiteral("slotIndex"), -1).toInt();
	int class1_ix = m.value(QStringLiteral("classIndex"), -1).toInt();
	auto dt = data();
	int slot2_ix = dt.startSlotIndex();
	int class2_ix = dt.classIndex();
	if(!m_dropInsertsBefore.toBool())
		class2_ix++;
	qfDebug() << "DROP class:" << slot1_ix << class1_ix;
	ganttItem()->moveClassItem(slot1_ix, class1_ix, slot2_ix, class2_ix);

	m_dropInsertsBefore = QVariant();
	update();
}

void ClassItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
	QMenu menu;
	menu.addAction("Edit class");
	QAction *a = menu.exec(event->screenPos());
	if(a) {
		auto *parent_w = qfc::Utils::findParent<drawing::DrawingGanttWidget*>(scene());
		auto *w = new ClassDefWidget();
		w->setWindowTitle(tr("Edit class"));
		qfd::Dialog dlg(QDialogButtonBox::Save | QDialogButtonBox::Cancel, parent_w);
		dlg.setDefaultButton(QDialogButtonBox::Save);
		dlg.setCentralWidget(w);
		w->load(data().id(), qfm::DataDocument::ModeEdit);
		bool ok = dlg.exec();
		if(ok) {
			//qfInfo() << "OK";
			qf::gui::model::DataDocument *doc = w->dataDocument();
			ClassData dt = data();
			dt.setStartTimeMin(doc->value("startTimeMin").toInt());
			dt.setStartIntervalMin(doc->value("startIntervalMin").toInt());
			dt.setVacantsBefore(doc->value("vacantsBefore").toInt());
			dt.setVacantEvery(doc->value("vacantEvery").toInt());
			dt.setVacantsAfter(doc->value("vacantsAfter").toInt());
			dt.setMapCount(doc->value("mapCount").toInt());
			setData(dt);
			auto *slot_it = startSlotItem();
			if(slot_it->classItemIndex(this) == 0) {
				// the edited start time was written to the database by the dialog,
				// move the whole slot to keep the layout consistent with a reload
				StartSlotData sd = slot_it->data();
				sd.setStartOffset(qMax(dt.startTimeMin(), 0));
				slot_it->setData(sd);
			}
			if(slot_it->classItemCount() > 1) {
				// start times of the classes chained after this one shift with its new size
				ganttScene()->setDirty(true);
			}
			ganttItem()->updateGeometry();
			ganttItem()->checkClassClash();
		}
	}
}

}
