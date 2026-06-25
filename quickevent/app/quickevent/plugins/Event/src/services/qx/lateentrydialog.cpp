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

namespace Event::services::qx {

LateEntryDialog::LateEntryDialog(int change_id, int lock_number, const LateEntry &late_entry, QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::LateEntryDialog)
	, m_changeId(change_id)
	, m_runId(late_entry.run_id)
{
	ui->setupUi(this);

	ui->btAccept->setDisabled(true);
	ui->btReject->setDisabled(true);

	connect(ui->btAccept, &QPushButton::clicked, this, [this]() {
		resolveChanges(true);
	});
	connect(ui->btReject, &QPushButton::clicked, this, [this]() {
		resolveChanges(false);
	});

	ui->edClassName->setText(late_entry.record.class_name.value_or(QString()));
	ui->edNote->setText(late_entry.record.note);

	ui->edRunId->setValue(m_runId);
	ui->edChangeId->setValue(change_id);
	ui->edLockNumber->setValue(lock_number);

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

	ui->grpFirstName->setChecked(late_entry.record.first_name.has_value());
	ui->edFirstName->setText(late_entry.record.first_name.has_value()? late_entry.record.first_name.value(): QString());
	update_background(ui->edFirstName);

	ui->grpLastName->setChecked(late_entry.record.last_name.has_value());
	ui->edLastName->setText(late_entry.record.last_name.has_value()? late_entry.record.last_name.value(): QString());
	update_background(ui->edLastName);

	ui->grpRegistration->setChecked(late_entry.record.registration.has_value());
	ui->edRegistration->setText(late_entry.record.registration.has_value()? late_entry.record.registration.value(): QString());
	update_background(ui->edRegistration);

	ui->grpSiCard->setChecked(late_entry.record.si_id.has_value());
	ui->edSiCard->setValue(late_entry.record.si_id.has_value()? late_entry.record.si_id.value(): 0);
	update_background(ui->edSiCard);

	if (m_runId > 0) {
		loadOrigValues();
	}
	else {
		loadClassId();
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

void LateEntryDialog::loadOrigValues()
{
	Q_ASSERT(m_runId > 0);

	qf::core::sql::Query q;
	q.execThrow(QStringLiteral("SELECT classes.name AS class_name, competitors.id AS competitor_id"
							   " FROM runs"
							   " JOIN competitors ON competitors.id=runs.competitorId AND runs.id=%1"
							   " LEFT JOIN classes ON competitors.classId=classes.id")
				.arg(m_runId)
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

void LateEntryDialog::loadClassId()
{
	auto class_name = ui->edClassName->text();
	if (class_name.isEmpty()) {
		return;
	}
	qf::core::sql::Query q;
	q.execThrow(QStringLiteral("SELECT id"
							   " FROM classes"
							   " WHERE name='%1'")
				.arg(class_name)
				);
	if (q.next()) {
		m_classId = q.value("id").toInt();
	}
}

void LateEntryDialog::lockChange()
{
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

void LateEntryDialog::resolveChanges(bool is_accepted)
{
	using namespace qf::gui::model;

	bool is_insert = m_runId == 0;
	Competitors::CompetitorDocument doc;
	doc.load(m_competitorId, is_insert? DataDocument::ModeInsert: DataDocument::ModeEdit);
	if (is_insert) {
		doc.setValue("classId", m_classId);
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
		else {
			qf::core::sql::Record rec {
				{ "siId", ui->edSiCard->value() },
			};
			qf::gui::framework::Application::instance()->qxSql()->updateRecord("runs", m_runId, rec, this);
		}
	}
	doc.save();

	qf::core::sql::Record rec {
		{"status", is_accepted? "Accepted": "Rejected"},
		{"orig_data", qf::core::Utils::qvariantToJson(m_origValues.toVariantMap())},
	};
	qf::gui::framework::Application::instance()->qxSql()->updateRecord("qxchanges", m_changeId, rec, this);
}

} // namespace Event::services::qx


