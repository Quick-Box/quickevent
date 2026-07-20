#include "lateentrydialog.h"
#include "ui_lateentrydialog.h"

#include "qxeventservice.h"
#include "runchange.h"

#include <plugins/Competitors/src/competitordocument.h>

#include <quickevent/core/og/timems.h>

#include <qf/gui/framework/application.h>
#include <qf/core/sql/qxsql.h>
#include <qf/core/sql/query.h>
#include <qf/core/log.h>

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QLineEdit>
#include <QMessageBox>
#include <QSpinBox>

#include <variant>

using namespace quickevent::core::og;

namespace Event::services::qx {

LateEntryDialog::LateEntryDialog(int change_id, int stage_id, const LateEntry &late_entry, const QString &status, const QString &status_message, QWidget *parent)
	: Super(parent)
	, ui(new Ui::LateEntryDialog)
	, m_changeId(change_id)
	, m_stageId(stage_id)
	, m_lateEntryId(late_entry.id)
	, m_status(qxChangeStatusFromString(status))
{
	Q_ASSERT(change_id > 0);

	ui->setupUi(this);

	ui->edStatusMessage->setPlainText(status_message);

	ui->btAccept->setDisabled(true);
	ui->btReject->setDisabled(true);

	connect(ui->chkForce, &QCheckBox::checkStateChanged, this, [this]() {
		updateButtonsEnabled();
	});
	connect(ui->btAccept, &QPushButton::clicked, this, [this]() {
		resolveChangesAndClose(true);
		done(QDialog::Accepted);
	});
	connect(ui->btAcceptAndEdit, &QPushButton::clicked, this, [this]() {
		resolveChangesAndClose(true);
		emit editCompetitor(m_competitorId);
		done(QDialog::Accepted);
	});
	connect(ui->btReject, &QPushButton::clicked, this, [this]() {
		resolveChangesAndClose(false);
		done(QDialog::Accepted);
	});
	connect(ui->btSendMessage, &QPushButton::clicked, this, &LateEntryDialog::updateQxChangeMessage);

	ui->edNote->setText(late_entry.note.value_or(QString()));

	ui->edChangeId->setValue(change_id);

	auto update_background = [](QWidget *widget) {
		bool is_set = false;
		if (auto *le = qobject_cast<QLineEdit*>(widget)) {
			is_set = !le->text().isEmpty();
		}
		else if (auto *spin_box = qobject_cast<QSpinBox*>(widget)) {
			is_set = spin_box->value() > 0;
		}
		if (is_set) {
			widget->setStyleSheet("background: palegreen");
		}
	};

	ui->grpFirstName->setChecked(late_entry.first_name.has_value());
	ui->edFirstName->setText(late_entry.first_name.value_or(QString()));
	update_background(ui->edFirstName);

	ui->grpLastName->setChecked(late_entry.last_name.has_value());
	ui->edLastName->setText(late_entry.last_name.value_or(QString()));
	update_background(ui->edLastName);

	ui->grpRegistration->setChecked(late_entry.registration.has_value());
	ui->edRegistration->setText(late_entry.registration.value_or(QString()));
	update_background(ui->edRegistration);

	ui->grpSiCard->setChecked(late_entry.si_id.has_value());
	ui->edSiCard->setValue(late_entry.si_id.value_or(0));
	update_background(ui->edSiCard);

	ui->grpStartTime->setChecked(late_entry.start_time_ms.has_value());
	ui->edStartTime->setText(late_entry.start_time_ms.has_value()? TimeMs(late_entry.start_time_ms.value()).toString(): QString{});
	update_background(ui->edStartTime);

	connect(ui->edStartTime, &QLineEdit::textEdited, this, &LateEntryDialog::checkStartTimeIsValid);

	if (auto *run_id = std::get_if<RunId>(&late_entry.id)) {
		auto id = run_id->id;
		ui->edRunId->setValue(id);
		loadOrigValues(id);
		checkStartTimeIsValid();
	}
	else if (auto *id = std::get_if<ClassId>(&late_entry.id)) {
		auto class_id = id->id;
		loadClassName(class_id);
	}

	if (late_entry.paid.value_or(false)) {
		ui->lblPaid->setText(tr("Paid"));
		ui->lblPaid->setStyleSheet("background:green;color:white");
	} else {
		ui->lblPaid->setText(tr("Not Paid"));
		ui->lblPaid->setStyleSheet("background:red;color:white");
	}

	checkDuplicitRegistration();
	checkDuplicitName();
	lockChange();
}

LateEntryDialog::~LateEntryDialog()
{
	delete ui;
}

QxEventService *LateEntryDialog::service()
{
	auto *svc = qobject_cast<QxEventService*>(Service::serviceByName(QxEventService::serviceId()));
	Q_ASSERT(svc);
	return svc;
}

void LateEntryDialog::setMessage(const QString &msg, bool error)
{
	if (msg.isEmpty()) {
		ui->lblError->setStyleSheet({});
	}
	else if (error) {
		ui->lblError->setStyleSheet("background: salmon");
	}
	else {
		ui->lblError->setStyleSheet({});
	}
	ui->lblError->setText(msg);
}

void LateEntryDialog::loadOrigValues(int run_id)
{
	Q_ASSERT(run_id > 0);

	qf::core::sql::Query q;
	q.execThrow(QStringLiteral("SELECT classes.name AS class_name, competitors.id AS competitor_id"
							   " FROM runs"
							   " JOIN competitors ON competitors.id=runs.competitorId AND runs.id=%1"
							   " LEFT JOIN classes ON competitors.classId=classes.id")
				.arg(run_id)
				);
	if (q.next()) {
		ui->edClassName->setText(q.value("class_name").toString());
		m_competitorId = q.value("competitor_id").toInt();
	}

	Competitors::CompetitorDocument doc;
	doc.load(m_competitorId, qf::gui::model::DataDocument::ModeView);

	m_origValues.first_name = doc.value("firstName").toString();
	m_origValues.last_name = doc.value("lastName").toString();
	m_origValues.registration = doc.value("registration").toString();
	{
		qf::core::sql::Query q;
		q.execThrow(QStringLiteral("SELECT startTimeMs"
								   " FROM runs"
								   " WHERE id=%1")
					.arg(run_id)
					);
		if (q.next()) {
			m_origValues.start_time_ms = q.value("startTimeMs").toInt();
		}
	}
	m_origValues.si_id = doc.value("siId").toInt();

	ui->edFirstNameOrig->setText(m_origValues.first_name);
	ui->edLastNameOrig->setText(m_origValues.last_name);
	ui->edRegistrationOrig->setText(m_origValues.registration);
	ui->edStartTimeOrig->setText(TimeMs(m_origValues.start_time_ms).toString());
	ui->edSiCardOrig->setValue(m_origValues.si_id);
}

void LateEntryDialog::loadClassName(int class_id)
{
	qf::core::sql::Query q;
	q.execThrow(QStringLiteral("SELECT name FROM classes WHERE id=%1")
				.arg(class_id) );
	if (q.next()) {
		ui->edClassName->setText(q.value(0).toString());
	}
}

void LateEntryDialog::unlockChange() const
{
	if (m_lockNumber > 0 || ui->chkForce->isChecked()) {
		qf::core::sql::Query q;
		q.execThrow(QStringLiteral("UPDATE qxchanges SET lock_number=NULL WHERE id=%1")
					.arg(m_changeId) );
	}
}

void LateEntryDialog::lockChange()
{
	auto lock_number = QxEventService::currentConnectionId();
	qf::core::sql::Query q;
	q.execThrow(QStringLiteral("UPDATE qxchanges SET lock_number=%1 WHERE id=%2  AND (lock_number IS NULL OR lock_number=%1)")
				.arg(lock_number)
				.arg(m_changeId)
				);
	if (q.numRowsAffected() == 0) {
		m_lockNumber = 0;
		setMessage(tr("Change is already locked by other user."), true);
	}
	else {
		m_lockNumber = lock_number;
	}
	updateButtonsEnabled();
}

void LateEntryDialog::updateButtonsEnabled()
{
	ui->btAccept->setEnabled(m_status == QxChangeStatus::Pending && (ui->chkForce->isChecked() || m_lockNumber > 0));
	ui->btReject->setEnabled(m_status == QxChangeStatus::Pending && (ui->chkForce->isChecked() || m_lockNumber > 0));
}

void LateEntryDialog::resolveChangesAndClose(bool is_accepted)
{
	using namespace qf::gui::model;

	if (is_accepted) {
		auto class_id = classId();
		bool is_insert = class_id.has_value() && !m_setIsRunning;
		Competitors::CompetitorDocument doc;
		doc.load(m_competitorId, is_insert? DataDocument::ModeInsert: DataDocument::ModeEdit);
		if (is_insert) {
			doc.setValue("classId", class_id.value());
		}
		if (ui->grpFirstName->isChecked()) {
			doc.setValue("firstName", ui->edFirstName->text());
		}
		if (ui->grpLastName->isChecked()) {
			doc.setValue("lastName", ui->edLastName->text());
		}
		if (ui->grpRegistration->isChecked()) {
			doc.setValue("registration", ui->edRegistration->text());
		}
		if (ui->grpSiCard->isChecked()) {
			if (is_insert) {
				doc.setValue("siId", ui->edSiCard->value());
			}
		}
		doc.save();
		m_competitorId = doc.dataId().toInt();
		qf::core::sql::Record rec;
		if (ui->grpSiCard->isChecked()) {
			rec["siId"] = ui->edSiCard->value();
		}
		if (ui->grpStartTime->isChecked()) {
			rec["starttimems"] = TimeMs::fromString(ui->edStartTime->text()).msec();
		}
		if (m_setIsRunning) {
			rec["isRunning"] = true;
		}
		if (!rec.isEmpty()) {
			auto competitor_id = doc.value("id").toInt();
			Q_ASSERT(competitor_id > 0);
			int run_id = 0;
			{
				qf::core::sql::Query q;
				q.execThrow(QStringLiteral("SELECT id FROM runs WHERE competitorId=%1 AND stageId=%2")
							.arg(competitor_id)
							.arg(m_stageId)
							);
				if (q.next()) {
					run_id = q.value(0).toInt();
				}
			}
			Q_ASSERT(run_id > 0);
			qf::gui::framework::Application::instance()->qxSql()->updateRecord("runs", run_id, rec, this);
		}
	}

	qf::core::sql::Record rec {
		{"status", is_accepted? "Accepted": "Rejected"},
		{"status_message", ui->edStatusMessage->toPlainText()},
		{"orig_data", qf::core::Utils::qvariantToJson(is_accepted? m_origValues.toVariantMap(): QVariantMap{})},
	};
	qf::gui::framework::Application::instance()->qxSql()->updateRecord("qxchanges", m_changeId, rec, this);
}

void LateEntryDialog::updateQxChangeMessage()
{
	qf::core::sql::Record rec {
		{ "status_message", ui->edStatusMessage->toPlainText() },
	};
	qf::gui::framework::Application::instance()->qxSql()->updateRecord("qxchanges", m_changeId, rec, this);
}

void LateEntryDialog::checkDuplicitRegistration()
{
	auto class_id = classId();
	if (!ui->grpRegistration->isChecked() || !class_id.has_value()) {
		return;
	}
	auto reg = ui->edRegistration->text().trimmed().toUpper();
	qf::core::sql::Query q;
	q.execThrow(QStringLiteral("SELECT id, firstName, lastName FROM competitors WHERE registration='%1' AND classId=%2")
				.arg(reg).arg(class_id.value()) );
	if (q.next()) {
		setMessage(tr("Competitor %1 %2 %3 is registered in this class already, run will be updated.")
				   .arg(q.value("firstName").toString())
				   .arg(q.value("lastName").toString())
				   .arg(reg)
				   , true);
		auto competitor_id = q.value("id").toInt();
		Q_ASSERT(competitor_id > 0);
		changeEventEntryToStageEntry(competitor_id);
	}
}

void LateEntryDialog::checkDuplicitName()
{
	auto class_id = classId();
	if (!ui->grpFirstName->isChecked() || !ui->grpLastName->isChecked() || !class_id.has_value()) {
		return;
	}
	auto first_name = ui->edFirstName->text().trimmed();
	auto last_name = ui->edLastName->text().trimmed();
	qf::core::sql::Query q;
	q.execThrow(QStringLiteral("SELECT id, registration FROM competitors WHERE firstName='%1' AND lastName='%2' AND classId=%3")
				.arg(first_name)
				.arg(last_name)
				.arg(class_id.value()));
	if (q.next()) {
		setMessage(tr("Competitor %1 %2 %3 is registered in this class already, run will be updated.")
				   .arg(first_name)
				   .arg(last_name)
				   .arg(q.value("registration").toString())
				   , true);
		auto competitor_id = q.value("id").toInt();
		Q_ASSERT(competitor_id > 0);
		changeEventEntryToStageEntry(competitor_id);
	}
}

void LateEntryDialog::changeEventEntryToStageEntry(int competitor_id)
{
	m_setIsRunning = true;
	m_competitorId = competitor_id;

	ui->grpFirstName->setChecked(false);
	ui->grpLastName->setChecked(false);
	ui->grpRegistration->setChecked(false);
}

void LateEntryDialog::checkStartTimeIsValid()
{
	if (!ui->grpStartTime->isChecked()) {
		ui->edStartTime->setStyleSheet({});
		return;
	}
	if (auto run_id = runId(); run_id.has_value()) {
		auto stime = TimeMs::fromString(ui->edStartTime->text());
		if (stime.isValid()) {
			auto possible_stimes = Competitors::CompetitorDocument::possibleStartTimesMs(run_id.value());
			if (possible_stimes.contains(stime.msec())) {
				ui->edStartTime->setStyleSheet({});
			} else {
				ui->edStartTime->setStyleSheet("background: red");
			}
		}
	}
}

void LateEntryDialog::done(int result)
{
	unlockChange();
	Super::done(result);
}

std::optional<int> LateEntryDialog::runId() const
{
	if (auto *id = std::get_if<RunId>(&m_lateEntryId)) {
		return id->id;
	}
	return std::nullopt;
}

std::optional<int> LateEntryDialog::classId() const
{
	if (auto *id = std::get_if<ClassId>(&m_lateEntryId)) {
		return id->id;
	}
	return std::nullopt;
}

} // namespace Event::services::qx
