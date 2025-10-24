#include "addlegdialogwidget.h"
#include "ui_addlegdialogwidget.h"

#include <qf/gui/framework/mainwindow.h>
#include <qf/gui/dialogs/messagebox.h>

#include <qf/core/log.h>
#include <qf/core/exception.h>
#include <qf/core/assert.h>
#include <qf/core/sql/query.h>
#include <qf/gui/model/sqltablemodel.h>
#include <plugins/Event/src/eventplugin.h>
// #include <plugins/Competitors/src/competitorsplugin.h>
#include <plugins/Competitors/src/competitordocument.h>

#include <QTimer>

using qf::gui::framework::getPlugin;
// using Competitors::CompetitorsPlugin;
using Event::EventPlugin;

AddLegDialogWidget::AddLegDialogWidget(QWidget *parent)
	: Super(parent)
	, ui(new Ui::AddLegDialogWidget)
{
	qfLogFuncFrame() << objectName();
	ui->setupUi(this);
	setPersistentSettingsId(objectName());
	ui->tblCompetitors->setPersistentSettingsId(ui->tblCompetitors->objectName());
	ui->tblRegistrations->setPersistentSettingsId(ui->tblRegistrations->objectName());

	m_defaultStatusText = ui->lblStatus->text();

	auto *competitors_model = new qf::gui::model::SqlTableModel(this);
	//competitors_model->addColumn("relays.club", tr("Club"));
	competitors_model->addColumn("relayName", tr("Name"));
	competitors_model->addColumn("runs.leg", tr("Leg"));
	competitors_model->addColumn("competitorName", tr("Name"));
	competitors_model->addColumn("registration", tr("Reg"));
	competitors_model->addColumn("licence", tr("Lic"));
	competitors_model->addColumn("competitors.siId", tr("SI"));
	qf::core::sql::QueryBuilder qb;
	qb.select2("runs", "id, relayId, leg")
			.select2("competitors", "id, registration, licence, siId")
			.select2("classes", "name")
			.select2("relays", "classId")
			.select("COALESCE(relays.club, '') || ' ' || COALESCE(relays.name, '') AS relayName")
			.select("COALESCE(lastName, '') || ' ' || COALESCE(firstName, '') AS competitorName")
			.from("competitors")
			.join("competitors.id", "runs.competitorId")
			.join("runs.relayId", "relays.id")
			.join("relays.classId", "classes.id")
			.orderBy("competitorName");//.limit(10);
//	qfInfo() << qb.toString();
	competitors_model->setQueryBuilder(qb);
	ui->tblCompetitors->setTableModel(competitors_model);
	ui->tblCompetitors->setReadOnly(true);
	competitors_model->reload();

	auto *reg_model = getPlugin<EventPlugin>()->registrationsModel();
	ui->tblRegistrations->setTableModel(reg_model);
	ui->tblRegistrations->setReadOnly(true);
	connect(reg_model, &qf::gui::model::SqlTableModel::reloaded, this, [this]() {
			ui->tblRegistrations->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);
		});

	connect(ui->edFilter, &QLineEdit::textChanged, this, &AddLegDialogWidget::onFilterTextChanged);
	connect(ui->tblCompetitors, &qf::gui::TableView::doubleClicked, this, &AddLegDialogWidget::onCompetitorSelected);
	connect(ui->tblRegistrations, &qf::gui::TableView::doubleClicked, this, &AddLegDialogWidget::onRegistrationSelected);
	connect(ui->btUseUnregRunner, &QPushButton::clicked, this, &AddLegDialogWidget::onUnregistredRunnerAdded);
}

AddLegDialogWidget::~AddLegDialogWidget()
{
	delete ui;
}

void AddLegDialogWidget::onFilterTextChanged()
{
	QString txt = ui->edFilter->text().trimmed();
	if(txt.length() < 3)
		txt.clear();	// clear filter when less than 3 chars (not return, need to clear filter)
	ui->tblCompetitors->filterByString(txt);
	ui->tblRegistrations->filterByString(txt);
	ui->edFilter->setFocus();
}

void AddLegDialogWidget::onCompetitorSelected()
{
	qf::core::utils::TableRow row = ui->tblCompetitors->selectedRow();
	//int curr_leg = row.value("runs.leg").toInt();
	int competitor_id = row.value("competitors.id").toInt();
	int siid = row.value("competitors.siid").toInt();
	int curr_run_id = row.value("runs.id").toInt();
	int curr_relay_id = row.value("relayId").toInt();
	if(curr_relay_id > 0 && curr_relay_id != relayId()) {
		if(false == qf::gui::dialogs::MessageBox::askYesNo(this, tr("Competitor has different relay assigned already. Move it to current one?")))
			return;
		if(row.value("relays.classId").toInt() != classId()) {
			qf::core::sql::Query q;
			q.exec("UPDATE competitors SET "
				   "classId=" + QString::number(classId())
				   + " WHERE id=" + QString::number(competitor_id), qf::core::Exception::Throw);
		}
	}
	int free_leg = findFreeLeg();
	qf::core::sql::Query q;
	if(curr_run_id == 0 || curr_relay_id == relayId()) {
		q.exec("INSERT INTO runs (competitorId, relayId, leg, siid) VALUES ("
			   + QString::number(competitor_id) + ", "
			   + QString::number(relayId()) + ", "
			   + QString::number(free_leg) + ", "
			   + QString::number(siid) + " "
			   + ") ", qf::core::Exception::Throw);
	}
	else {
		q.exec("UPDATE runs SET "
			   "relayId=" + QString::number(relayId())
			   + ", leg=" + QString::number(free_leg)
			   + ", isRunning=(1=1)" // TRUE is not accepted by SQLite
			   + " WHERE id=" + QString::number(curr_run_id), qf::core::Exception::Throw);
	}
	updateLegAddedStatus(tr("Runner %1 was assigned to leg %2")
						 .arg(row.value("competitorName").toString())
						 .arg(free_leg));
	emit legAdded();
}

void AddLegDialogWidget::onRegistrationSelected()
{
	qf::core::utils::TableRow row = ui->tblRegistrations->selectedRow();
	Competitors::CompetitorDocument doc;
	doc.loadForInsert();
	doc.setValue("firstName", row.value("firstName"));
	doc.setValue("lastName", row.value("lastName"));
	doc.setValue("registration", row.value("registration"));
	doc.setValue("licence", row.value("licence"));
	doc.setValue("siid", row.value("siid"));
	doc.setValue("classId", classId());
	doc.save();
	int run_id = doc.runsIds().value(0);
	QF_ASSERT(run_id > 0, "Bad insert", return);
	int free_leg = findFreeLeg();
	qf::core::sql::Query q;
	q.exec("UPDATE runs SET relayId=" + QString::number(relayId()) + ", leg=" + QString::number(free_leg)
		   + " WHERE id=" + QString::number(run_id), qf::core::Exception::Throw);

	updateLegAddedStatus(tr("Runner %1 was assigned to leg %2")
						 .arg(row.value("competitorName").toString())
						 .arg(free_leg));
	emit legAdded();
}

void AddLegDialogWidget::updateLegAddedStatus(const QString &msg)
{
	if(!m_updateStatusTimer) {
		m_updateStatusTimer = new QTimer(this);
		m_updateStatusTimer->setSingleShot(true);
		m_updateStatusTimer->setInterval(3000);
		connect(m_updateStatusTimer, &QTimer::timeout, this, [this]() {
			ui->lblStatus->setText(m_defaultStatusText);
			ui->lblStatus->setStyleSheet(QString());
		});
	}
	ui->lblStatus->setText(msg);
	ui->lblStatus->setStyleSheet(QStringLiteral("color: white; background: green"));
	m_updateStatusTimer->start();
}

int AddLegDialogWidget::findFreeLeg() const
{
	qf::core::sql::Query q;
	q.exec("SELECT leg FROM runs WHERE leg IS NOT NULL AND relayId=" + QString::number(relayId()) + " ORDER BY leg", qf::core::Exception::Throw);
	int free_leg = 1;
	while(q.next()) {
		int leg = q.value(0).toInt();
		if(leg != free_leg) {
			break;
		}
		free_leg++;
	}
	return free_leg;
}

void AddLegDialogWidget::onUnregistredRunnerAdded()
{
	QString firstName = ui->edFirstName->text();
	QString lastName = ui->edLastName->text();
	if (lastName.isEmpty() || firstName.isEmpty())
		return;

	Competitors::CompetitorDocument doc;
	doc.loadForInsert();
	doc.setValue("firstName", firstName);
	doc.setValue("lastName", lastName);
	doc.setValue("siid", ui->edSiId->value());
	doc.setValue("classId", classId());
	doc.save();
	QString name = lastName + " " + firstName;
	int run_id = doc.runsIds().value(0);
	QF_ASSERT(run_id > 0, "Bad insert", return);
	int free_leg = findFreeLeg();
	qf::core::sql::Query q;
	q.exec("UPDATE runs SET relayId=" + QString::number(relayId()) + ", leg=" + QString::number(free_leg)
		   + " WHERE id=" + QString::number(run_id), qf::core::Exception::Throw);

	updateLegAddedStatus(tr("Runner %1 was assigned to leg %2")
						 .arg(name)
						 .arg(free_leg));
	emit legAdded();

	ui->edFirstName->setText("");
	ui->edLastName->setText("");
	ui->edSiId->setValue(0);
}
