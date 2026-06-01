#include "codeclassresultswidget.h"
#include "ui_codeclassresultswidget.h"

#include <plugins/Event/src/eventplugin.h>

#include <quickevent/core/codedef.h>
#include <quickevent/gui/og/sqltablemodel.h>
#include <quickevent/core/og/timems.h>
#include <quickevent/core/si/punchrecord.h>

#include <qf/gui/framework/mainwindow.h>

#include <qf/core/sql/query.h>
#include <qf/core/sql/querybuilder.h>
#include <qf/core/assert.h>
#include <qf/core/log.h>

#include <QDropEvent>
#include <QMimeData>
#include <QJsonObject>
#include <QTimer>

using qf::gui::framework::getPlugin;
using Event::EventPlugin;

CodeClassResultsWidget::CodeClassResultsWidget(QWidget *parent)
	: QWidget(parent)
	, ui(new Ui::CodeClassResultsWidget)
{
	ui->setupUi(this);

	ui->tblView->setReadOnly(true);
	//ui->tblView->setPersistentSettingsId("tblView");
	//ui->tblPunches->setRowEditorMode(qfw::TableView::EditRowsMixed);
	//ui->tblPunches->setInlineEditSaveStrategy(qfw::TableView::OnEditedValueCommit);
	auto *m = new quickevent::gui::og::SqlTableModel(this);
	m->addColumn("competitorName", tr("Competitor"));
	m->addColumn("competitors.registration", tr("Reg"));//.setReadOnly(true);
	m->addColumn("timeMs", tr("Time")).setCastType(qMetaTypeId<quickevent::core::og::TimeMs>());
	m->addColumn("disqualified", tr("DISQ")).setReadOnly(true);
	ui->tblView->setTableModel(m);
	m_tableModel = m;
}

CodeClassResultsWidget::~CodeClassResultsWidget()
{
	delete ui;
}

void CodeClassResultsWidget::reloadDeferred()
{
	if(!m_reloadDeferredTimer) {
		m_reloadDeferredTimer = new QTimer(this);
		m_reloadDeferredTimer->setSingleShot(true);
		m_reloadDeferredTimer->setInterval(200);
		connect(m_reloadDeferredTimer, &QTimer::timeout, this, &CodeClassResultsWidget::reload);
	}
	if(m_reloadDeferredTimer->isActive())
		return;
	m_reloadDeferredTimer->start();
}

void CodeClassResultsWidget::reload()
{
	if(!getPlugin<EventPlugin>()->isEventOpen())
		return;
	int stage_id = getPlugin<EventPlugin>()->currentStageId();
	int class_id = this->ui->lstClass->currentData().toInt();
	int code = (m_pinnedToCode == ALL_CODES)? ui->lstCode->currentData().toInt(): m_pinnedToCode;
	if(class_id == 0 || code == 0) {
		m_tableModel->clearRows();
		return;
	}
	qf::core::sql::QueryBuilder qb;
	qb.select2("competitors", "registration")
			.select("COALESCE(competitors.lastName, '') || ' ' || COALESCE(competitors.firstName, '') AS competitorName");
	if(code == RESULTS_PUNCH_CODE) {
		// results
		qb.select2("runs", "timeMs, disqualified")
				.from("runs")
				.where("runs.stageId=" QF_IARG(stage_id))
				.joinRestricted("runs.competitorId", "competitors.id",
								"competitors.classId=" QF_IARG(class_id)
								//" AND NOT runs.disqualified"
								" AND timeMs IS NOT NULL" ,
								qf::core::sql::QueryBuilder::INNER_JOIN)
				.orderBy("runs.timeMs");//.limit(10);
	}
	else {
		qb.select2("punches", "runTimeMs AS timeMs")
				.select2("runs", "disqualified")
				.from("punches")
				.joinRestricted("punches.runId", "runs.id",
								"punches.stageId=" QF_IARG(stage_id)
								" AND punches.code=" QF_IARG(code)
								//" AND NOT runs.disqualified",
								, qf::core::sql::QueryBuilder::INNER_JOIN)
				.joinRestricted("runs.competitorId", "competitors.id", "competitors.classId=" QF_IARG(class_id), qf::core::sql::QueryBuilder::INNER_JOIN)
				.orderBy("punches.runTimeMs");//.limit(10);
	}
	qfDebug() << qb.toString();
	m_tableModel->setQueryBuilder(qb, false);
	m_tableModel->reload();
}

void CodeClassResultsWidget::onPunchReceived(const quickevent::core::si::PunchRecord &punch)
{
	int code = this->ui->lstCode->currentData().toInt();
	if(code > 0 && punch.code() == code) {
		reload();
	}
}

void CodeClassResultsWidget::reset(int class_id, int code, int pin_to_code)
{
	m_pinnedToCode = pin_to_code;
	disconnect(ui->lstClass, QOverload<int>::of(&QComboBox::currentIndexChanged), nullptr, nullptr);
	disconnect(ui->lstCode, QOverload<int>::of(&QComboBox::currentIndexChanged), nullptr, nullptr);
	ui->lstClass->clear();
	{
		qf::core::sql::Query q;
		q.exec("SELECT id, name FROM classes ORDER BY name", qf::core::Exception::Throw);
		while(q.next()) {
			ui->lstClass->addItem(q.value(1).toString(), q.value(0));
		}
	}
	if(pin_to_code == ALL_CODES) {
		ui->lblCode->show();
		ui->lstCode->show();
		connect(ui->lstClass, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [this](int ) {
			int stage_id = getPlugin<EventPlugin>()->currentStageId();
			int class_id = this->ui->lstClass->currentData().toInt();
			QSignalBlocker b1(ui->lstCode);
			ui->lstCode->clear();
			ui->lstCode->addItem(tr("Results"), RESULTS_PUNCH_CODE);
			ui->lstCode->addItem(tr("Finish"), quickevent::core::CodeDef::FINISH_PUNCH_CODE);
			qf::core::sql::QueryBuilder qb;
			qb.select2("codes", "code, radio")
					.from("classdefs")
					.joinRestricted("classdefs.courseId", "coursecodes.courseId",
									"classdefs.stageId=" QF_IARG(stage_id)
									" AND classdefs.classId=" QF_IARG(class_id),
									qf::core::sql::QueryBuilder::INNER_JOIN)
					.join("coursecodes.codeId", "codes.id", qf::core::sql::QueryBuilder::INNER_JOIN)
					//.joinRestricted("coursecodes.codeId", "codes.id", "codes.radio", qf::core::sql::QueryBuilder::INNER_JOIN)
					.orderBy("coursecodes.position");
			//qfInfo() << qb.toString();
			qf::core::sql::Query q;
			q.exec(qb.toString(), qf::core::Exception::Throw);
			while(q.next()) {
				QVariant code = q.value(0);
				bool radio =  q.value(1).toBool();
				QString caption = code.toString();
				if(radio) {
					caption += " " + tr("R", "Radio station");
				}
				ui->lstCode->addItem(caption, code);
			}
		});

		ui->lstClass->setCurrentIndex(ui->lstClass->findData(class_id));
		connect(ui->lstCode, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &CodeClassResultsWidget::reload);
		ui->lstCode->setCurrentIndex(ui->lstCode->findData(code));
		//connect(ui->btReload, &QPushButton::clicked, this, &CodeClassResultsWidget::reload);
	}
	else {
		ui->lblCode->hide();
		ui->lstCode->hide();
		connect(ui->lstClass, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &CodeClassResultsWidget::reload);
		ui->lstClass->setCurrentIndex(ui->lstClass->findData(class_id));
	}
}

void CodeClassResultsWidget::loadSetup(const QJsonObject &jso)
{
	reset(jso.value("classId").toInt(), jso.value("codeId").toInt());
}

QJsonObject CodeClassResultsWidget::saveSetup()
{
	QJsonObject ret;
	ret["classId"] = ui->lstClass->currentData().toInt();
	ret["codeId"] = ui->lstCode->currentData().toInt();
	return ret;
}
#if 0
void CodeClassResultsWidget::dropEvent(QDropEvent *event)
{
	qfInfo() << event->mimeData()->hasText();
	Super::dropEvent(event);
	/*
	if (event->mimeData()->hasFormat("application/x-dnditemdata")) {
		QByteArray itemData = event->mimeData()->data("application/x-dnditemdata");
		QDataStream dataStream(&itemData, QIODevice::ReadOnly);

		QPixmap pixmap;
		QPoint offset;
		dataStream >> pixmap >> offset;

		QLabel *newIcon = new QLabel(this);
		newIcon->setPixmap(pixmap);
		newIcon->move(event->pos() - offset);
		newIcon->show();
		newIcon->setAttribute(Qt::WA_DeleteOnClose);

		if (event->source() == this) {
			event->setDropAction(Qt::MoveAction);
			event->accept();
		} else {
			event->acceptProposedAction();
		}
	} else {
		event->ignore();
	}
	*/
}
#endif
