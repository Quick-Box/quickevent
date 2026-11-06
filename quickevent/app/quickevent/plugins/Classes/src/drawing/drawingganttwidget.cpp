#include "drawingganttwidget.h"
#include "ui_drawingganttwidget.h"

#include "classitem.h"
#include "ganttitem.h"
#include "ganttscene.h"

#include <qf/gui/style.h>
#include <qf/gui/toolbar.h>
#include <qf/gui/menubar.h>
#include <qf/gui/action.h>
#include <qf/gui/dialogs/dialog.h>

#include <quickevent/core/codedef.h>

#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <qf/core/sql/query.h>
#include <qf/core/sql/connection.h>

namespace qfs = qf::core::sql;

using namespace drawing;

DrawingGanttWidget::DrawingGanttWidget(QWidget *parent) :
	Super(parent),
	ui(new Ui::DrawingGanttWidget)
{
	setTitle(tr("Draw tool"));
	setPersistentSettingsId("DrawingToolWidget");
	ui->setupUi(this);

	connect(ui->actSave, &QAction::triggered, this, &DrawingGanttWidget::onActSaveTriggered);
	connect(ui->actFind, &QAction::triggered, this, &DrawingGanttWidget::onActFindTriggered);

	ui->actSave->setIcon(qf::gui::Style::icon("save"));
	ui->actFind->setIcon(qf::gui::Style::icon("find"));

	ui->actSave->setToolTip(tr("Write start times and start slots of all classes back to the Classes table\naccording to the current layout."));
	ui->actFind->setToolTip(tr("Find class by name"));

	m_ganttScene = new GanttScene(this);
	ui->ganttView->setScene(m_ganttScene);

	connect(m_ganttScene, &GanttScene::clashesChanged, this, &DrawingGanttWidget::onClashesChanged);
	connect(ui->lstClashes, &QListWidget::itemClicked, this, &DrawingGanttWidget::onClashItemActivated);
	connect(ui->lstClashes, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *current, QListWidgetItem *) {
		if(current)
			onClashItemActivated(current);
	});
	ui->lstClashes->setToolTip(tr("Click a conflict to highlight it in the layout"));
	ui->splitter->setStretchFactor(0, 1);
	ui->splitter->setStretchFactor(1, 0);
}

DrawingGanttWidget::~DrawingGanttWidget()
{
	delete ui;
}

void DrawingGanttWidget::settleDownInDialog(qf::gui::dialogs::Dialog *dlg)
{
	qf::gui::ToolBar *tb = dlg->toolBar("main", true);
	tb->addAction(ui->actSave);
	tb->addSeparator();
	tb->addWidget(new QLabel(tr("Find")));
	m_edFind = new QLineEdit();
	m_edFind->setMaximumWidth(QFontMetrics(font()).horizontalAdvance('X') * 8);
	m_edFind->setPlaceholderText(tr("Class"));
	m_edFind->setToolTip(tr("Find class by name"));
	connect(m_edFind, &QLineEdit::textEdited, this, &DrawingGanttWidget::onActFindTriggered);
	tb->addWidget(m_edFind);
	tb->addAction(ui->actFind);
	tb->addSeparator();
	tb->addWidget(new QLabel(tr("Show classes for")));
	m_cbStart = new QComboBox();
	m_cbStart->setToolTip(tr("Select the start number for class draw preparation."));
	tb->addWidget(m_cbStart);
	connect(m_cbStart, qOverload<int>(&QComboBox::currentIndexChanged), this, &DrawingGanttWidget::reloadSelectedStart);
	tb->addSeparator();
	m_cbLockIntervals = new QCheckBox(tr("Lock intervals"));
	m_cbLockIntervals->setToolTip(tr("Prevent changing start intervals with the mouse wheel or interval arrows."));
	connect(m_cbLockIntervals, &QCheckBox::toggled, m_ganttScene, &GanttScene::setIntervalsLocked);
	tb->addWidget(m_cbLockIntervals);
	tb->addSeparator();

	m_cbCheckRunners = new QCheckBox(tr("Runners clash"));
	m_cbCheckRunners->setToolTip(tr("Highlight classes overlapping in time which share the first control\nand whose start intervals would let two runners punch it at the same moment."));
	m_cbCheckCourses = new QCheckBox(tr("Courses clash"));
	m_cbCheckCourses->setToolTip(tr("Highlight classes overlapping in time which run on the same course."));

	m_cbCheckRunners->setChecked(true);
	tb->addWidget(m_cbCheckRunners);
#if QT_VERSION < QT_VERSION_CHECK(6, 8, 0)
		connect(m_cbCheckRunners, &QCheckBox::stateChanged, this, &DrawingGanttWidget::onUpdateClassCheck);
#else
		connect(m_cbCheckRunners, &QCheckBox::checkStateChanged, this, &DrawingGanttWidget::onUpdateClassCheck);
#endif
	m_cbCheckCourses->setChecked(true);
	tb->addWidget(m_cbCheckCourses);
#if QT_VERSION < QT_VERSION_CHECK(6, 8, 0)
		connect(m_cbCheckCourses, &QCheckBox::stateChanged, this, &DrawingGanttWidget::onUpdateClassCheck);
#else
		connect(m_cbCheckCourses, &QCheckBox::checkStateChanged, this, &DrawingGanttWidget::onUpdateClassCheck);
#endif
	{
		// QToolBar has no addStretch(), so an expanding empty widget is the
		// idiomatic spacer that pushes the conflict checkbox to the right end,
		// above the conflicts sidebar
		auto *spacer = new QWidget();
		spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
		tb->addWidget(spacer);
		m_cbShowClashes = new QCheckBox(tr("Show all conflicts"));
		m_cbShowClashes->setChecked(true);
		m_cbShowClashes->setToolTip(tr("Show the list of start time conflicts"));
		connect(m_cbShowClashes, &QCheckBox::toggled, ui->lstClashes, &QWidget::setVisible);
		tb->addWidget(m_cbShowClashes);
	}
	if (m_stageId >= 0)
		load(m_stageId);
	onUpdateClassCheck();
}

bool DrawingGanttWidget::acceptDialogDone(int result)
{
	Q_UNUSED(result)
	if(m_ganttScene->isDirty()) {
		auto answer = QMessageBox::question(this, tr("Unsaved changes"),
											tr("The start times layout has unsaved changes.\n"
											   "Do you want to save them before closing?"),
											QMessageBox::Discard | QMessageBox::Cancel | QMessageBox::Save,
											QMessageBox::Save);
		if(answer == QMessageBox::Cancel)
			return false;
		if(answer == QMessageBox::Save)
			m_ganttScene->save();
	}
	return true;
}

void DrawingGanttWidget::load(int stage_id)
{
	m_stageId = stage_id;
	setTitle(tr("E%1 Draw tool").arg(stage_id));
	if (!m_cbStart) {
		m_ganttScene->load(stage_id);
		return;
	}
	m_cbStart->blockSignals(true);
	m_cbStart->clear();
	qfs::Query q(qfs::Connection::forName());
	q.exec(QStringLiteral("SELECT DISTINCT codes.code FROM codes, coursecodes, classdefs"
							" WHERE coursecodes.codeId = codes.id AND classdefs.courseId = coursecodes.courseId"
							" AND classdefs.stageId = %1"
							" ORDER BY codes.code").arg(stage_id));
	while (q.next()) {
		const auto code = q.value(0).toInt();
		if(auto n = quickevent::core::CodeDef::codeToStartNumber(code); n.has_value()) {
			m_cbStart->addItem(tr("Start %1").arg(n.value()), code);
		}
	}
	if (m_cbStart->count() == 0) {
		m_cbStart->addItem(tr("Start"), 0);
	}

	if (m_cbStart->count() == 1) {
		m_cbStart->setEnabled(false);
	}
	m_cbStart->setCurrentIndex(0);
	m_cbStart->blockSignals(false);
	m_ganttScene->load(stage_id, m_cbStart->currentData().toString());
}

void DrawingGanttWidget::reloadSelectedStart()
{
	if (m_stageId < 0 || !m_cbStart || m_cbStart->currentIndex() < 0)
		return;
	if (m_ganttScene->isDirty()) {
		auto answer = QMessageBox::question(this, tr("Unsaved changes"),
			tr("The current start layout has unsaved changes. Save them before switching starts?"),
			QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
		if (answer == QMessageBox::Cancel) {
			m_cbStart->blockSignals(true);
			int old_ix = m_cbStart->findData(m_ganttScene->startCode());
			if (old_ix >= 0) m_cbStart->setCurrentIndex(old_ix);
			m_cbStart->blockSignals(false);
			return;
		}
		if (answer == QMessageBox::Save)
			m_ganttScene->save();
	}
	m_ganttScene->load(m_stageId, m_cbStart->currentData().toString());
	onUpdateClassCheck();
}


void drawing::DrawingGanttWidget::onActSaveTriggered()
{
	if(QMessageBox::information(this, tr("Save classes start times"),
								tr("All the user edited classes start times and start intervals will be overridden.\n"
								   "Do you want to save your changes?"),
								QMessageBox::Save | QMessageBox::Cancel,
								QMessageBox::Save))
	{

		m_ganttScene->save();
	}
}

void DrawingGanttWidget::onActFindTriggered()
{
	QString txt = m_edFind->text().trimmed().toUpper();
	for(QGraphicsItem *it : m_ganttScene->items()) {
		if(auto *cit = dynamic_cast<ClassItem *>(it)) {
			if(cit->data().className().toUpper().contains(txt)) {
				m_ganttScene->clearSelection();
				cit->setSelected(true);
				return;
			}
		}
	}
}

void DrawingGanttWidget::onClashesChanged(const QList<ClassClash> &clashes)
{
	// keep the selected conflict selected if it still exists, else select the first one,
	// so that exactly one conflict is always highlighted in the visualization
	ClassClash prev_clash;
	int prev_row = ui->lstClashes->currentRow();
	if(prev_row >= 0 && prev_row < m_clashes.count())
		prev_clash = m_clashes[prev_row];
	m_clashes = clashes;
	ui->lstClashes->blockSignals(true);
	ui->lstClashes->clear();
	for(const ClassClash &c : clashes) {
		const ClassData &dt1 = c.class1->data();
		const ClassData &dt2 = c.class2->data();
		QString text;
		if(c.type == ClassItem::ClashType::CourseOverlap)
			text = tr("%1 × %2 — same course %3").arg(dt1.className(), dt2.className()).arg(dt1.courseId());
		else
			text = tr("%1 × %2 — first control %3").arg(dt1.className(), dt2.className()).arg(dt1.firstCode());
		ui->lstClashes->addItem(text);
	}
	if(clashes.isEmpty()) {
		auto *item = new QListWidgetItem(tr("No conflicts"));
		item->setFlags(Qt::NoItemFlags);
		ui->lstClashes->addItem(item);
	}
	int select_ix = clashes.isEmpty()? -1: 0;
	for(int i = 0; i < clashes.count(); ++i) {
		const ClassClash &c = clashes[i];
		if(c.class1 == prev_clash.class1 && c.class2 == prev_clash.class2 && c.type == prev_clash.type) {
			select_ix = i;
			break;
		}
	}
	if(select_ix >= 0)
		ui->lstClashes->setCurrentRow(select_ix);
	ui->lstClashes->blockSignals(false);
	applyClashHighlight(select_ix);
	if(m_cbShowClashes)
		m_cbShowClashes->setText(clashes.isEmpty()? tr("Show all conflicts"): tr("Show all conflicts (%1)").arg(clashes.count()));
}

void DrawingGanttWidget::applyClashHighlight(int clash_ix)
{
	GanttItem *git = m_ganttScene->ganttItem();
	if(!git)
		return;
	if(clash_ix >= 0 && clash_ix < m_clashes.count())
		git->highlightClash(m_clashes[clash_ix]);
	else
		git->highlightClash(ClassClash());
}

void DrawingGanttWidget::onClashItemActivated(QListWidgetItem *item)
{
	int ix = ui->lstClashes->row(item);
	if(ix < 0 || ix >= m_clashes.count())
		return;
	applyClashHighlight(ix);
	const ClassClash &c = m_clashes[ix];
	m_ganttScene->clearSelection();
	int margin = 2 * m_ganttScene->displayUnit();
	QRectF r = c.class1->sceneBoundingRect() | c.class2->sceneBoundingRect();
	ui->ganttView->ensureVisible(r, margin, margin);
}

void DrawingGanttWidget::onUpdateClassCheck()
{
	QSet<ClassItem::ClashType> checks;
	if (m_cbCheckRunners && m_cbCheckRunners->isChecked()) {
		checks << ClassItem::ClashType::RunnersOverlap;
	}
	if (m_cbCheckCourses && m_cbCheckCourses->isChecked()) {
		checks << ClassItem::ClashType::CourseOverlap;
	}
	m_ganttScene->ganttItem()->setClashTypesToCheck(checks);
}

