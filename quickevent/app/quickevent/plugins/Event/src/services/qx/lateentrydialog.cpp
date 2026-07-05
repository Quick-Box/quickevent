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

namespace {

LateEntryStatus lateEntryStatusFromString(const QString &status)
{
	if (status.compare(QStringLiteral("Pending"), Qt::CaseInsensitive) == 0) {
		return LateEntryStatus::Pending;
	}
	if (status.compare(QStringLiteral("Accepted"), Qt::CaseInsensitive) == 0) {
		return LateEntryStatus::Accepted;
	}
	if (status.compare(QStringLiteral("Rejected"), Qt::CaseInsensitive) == 0) {
		return LateEntryStatus::Rejected;
	}
	qfWarning() << "Unknown late entry status:" << status;
	return LateEntryStatus::Rejected;
}

} // namespace

LateEntryDialog::LateEntryDialog(int change_id, int stage_id, const LateEntry &late_entry, const QString &status, const QString &status_message, QWidget *parent)
	: Super(parent)
	, ui(new Ui::LateEntryDialog)
	, m_changeId(change_id)
	, m_stageId(stage_id)
	, m_lateEntryId(late_entry.id)
	, m_status(lateEntryStatusFromString(status))
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
	ui->edStartTime->setText(TimeMs(late_entry.start_time_ms.value_or(0)).toString());
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
	ui->btAccept->setEnabled(m_status == LateEntryStatus::Pending && (ui->chkForce->isChecked() || m_lockNumber > 0));
	ui->btReject->setEnabled(m_status == LateEntryStatus::Pending && (ui->chkForce->isChecked() || m_lockNumber > 0));
}

void LateEntryDialog::resolveChangesAndClose(bool is_accepted)
{
	using namespace qf::gui::model;

	if (is_accepted) {
		auto class_id = classId();
		bool is_insert = class_id.has_value();
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

		qf::core::sql::Record rec;
		if (ui->grpSiCard->isChecked()) {
			rec["siId"] = ui->edSiCard->value();
		}
		if (ui->grpStartTime->isChecked()) {
			rec["starttimems"] = TimeMs::fromString(ui->edStartTime->text()).msec();
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

void LateEntryDialog::checkStartTimeIsValid()
{
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
