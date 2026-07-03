#include "lateentrydialog.h"
#include "ui_lateentrydialog.h"

#include "qxeventservice.h"
#include "runchange.h"

#include <plugins/Competitors/src/competitordocument.h>

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

LateEntryDialog::LateEntryDialog(int change_id, const LateEntry &late_entry, const QString &status, const QString &status_message, QWidget *parent)
	: Super(parent)
	, ui(new Ui::LateEntryDialog)
	, m_changeId(change_id)
	, m_lateEntryId(late_entry.id)
	, m_status(lateEntryStatusFromString(status))
{
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

	if (auto *run_id = std::get_if<RunId>(&late_entry.id)) {
		auto id = run_id->id;
		ui->edRunId->setValue(id);
		loadOrigValues(id);
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
	m_origValues.si_id = doc.value("siId").toInt();

	ui->edFirstNameOrig->setText(m_origValues.first_name);
	ui->edLastNameOrig->setText(m_origValues.last_name);
	ui->edRegistrationOrig->setText(m_origValues.registration);
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

	// NIY
	// auto *svc = service();
	// auto *nm = svc->networkManager();

	// auto path = QStringLiteral("/api/event/%1/changes").arg(svc->eventId());
	// QUrlQuery query;
	// query.addQueryItem("from_id", QString::number(m_changeId));
	// query.addQueryItem("limit", QString::number(1));
	// svc->getHttpJson(path, query, this, [this](auto data, auto error) {
	// 	if (!error.isEmpty()) {
	// 		setMessage(error, true);
	// 		return;
	// 	}
	// 	auto rec = data.toList().value(0).toMap();
	// });


	// QNetworkRequest request;
	// auto url = svc->exchangeServerUrl();
	// // qfInfo() << "url " << url.toString();
	// url.setPath("/api/event/current/changes/lock-change");

	// QUrlQuery query;
	// query.addQueryItem("change_id", QString::number(m_changeId));
	// auto connection_id = QxClientService::currentConnectionId();
	// query.addQueryItem("lock_number", QString::number(connection_id));
	// url.setQuery(query);
	// qfInfo() << "GET " << url.toString();

	// request.setUrl(url);
	// request.setRawHeader(QxClientService::QX_API_TOKEN, svc->apiToken());
	// auto *reply = nm->get(request);
	// connect(reply, &QNetworkReply::finished, this, [this, reply, connection_id]() {
	// 	auto data = reply->readAll();
	// 	if (reply->error() == QNetworkReply::NetworkError::NoError) {
	// 		m_lockNumber = data.toInt();
	// 		ui->edLockNumber->setValue(m_lockNumber);
	// 		if (m_lockNumber == connection_id) {
	// 			ui->btAccept->setDisabled(false);
	// 			ui->btReject->setDisabled(false);

	// 			qf::core::sql::Query q;
	// 			q.execThrow(QStringLiteral("UPDATE qxchanges SET lock_number=%1, status='Locked' WHERE id=%2")
	// 						.arg(connection_id)
	// 						.arg(m_changeId)
	// 						);
	// 		}
	// 		else {
	// 			setMessage(tr("Change is locked already by other client: %1, current client id:.%2").arg(m_lockNumber).arg(connection_id), false);
	// 		}
	// 	}
	// 	else {
	// 		setMessage(tr("Lock change error: %1\n%2").arg(reply->errorString()).arg(QString::fromUtf8(data)), true);
	// 	}
	// 	reply->deleteLater();
	// });
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
			else if (auto run_id = runId(); run_id.has_value()) {
				qf::core::sql::Record rec {
					{ "siId", ui->edSiCard->value() },
				};
				qf::gui::framework::Application::instance()->qxSql()->updateRecord("runs", run_id.value(), rec, this);
			}
		}
		doc.save();
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
