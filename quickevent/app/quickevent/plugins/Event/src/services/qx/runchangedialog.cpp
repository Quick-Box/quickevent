#include "runchangedialog.h"
#include "ui_runchangedialog.h"

#include "qxeventservice.h"
#include "runchange.h"

#include <plugins/Competitors/src/competitordocument.h>

#include <qf/core/sql/query.h>
#include <qf/core/log.h>

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QMessageBox>

namespace Event::services::qx {

RunChangeDialog::RunChangeDialog(int change_id, int run_id, int lock_number, const RunChange &run_change, QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::RunChangeDialog)
	, m_changeId(change_id)
	, m_runId(run_id)
{
	ui->setupUi(this);

	ui->btAccept->setDisabled(true);
	ui->btReject->setDisabled(true);

	connect(ui->chkForce, &QCheckBox::checkStateChanged, this, [this]() {
		ui->btAccept->setDisabled(!ui->chkForce->isChecked());
		ui->btReject->setDisabled(!ui->chkForce->isChecked());
	});
	connect(ui->btAccept, &QPushButton::clicked, this, [this]() {
		resolveChanges(true);
	});
	connect(ui->btReject, &QPushButton::clicked, this, [this]() {
		resolveChanges(false);
	});

	ui->edClassName->setText(run_change.class_name.value_or(QString()));
	ui->edNote->setText(run_change.note);

	ui->edRunId->setValue(run_id);
	ui->edChangeId->setValue(change_id);
	ui->edLockNumber->setValue(lock_number);

	ui->grpFirstName->setChecked(run_change.first_name.has_value());
	ui->edFirstName->setText(run_change.first_name.has_value()? run_change.first_name.value(): QString());

	ui->grpLastName->setChecked(run_change.last_name.has_value());
	ui->edLastName->setText(run_change.last_name.has_value()? run_change.last_name.value(): QString());

	ui->grpRegistration->setChecked(run_change.registration.has_value());
	ui->edRegistration->setText(run_change.registration.has_value()? run_change.registration.value(): QString());

	ui->grpSiCard->setChecked(run_change.si_id.has_value());
	ui->edSiCard->setValue(run_change.si_id.has_value()? run_change.si_id.value(): 0);

	if (run_id > 0) {
		loadOrigValues();
	}
	else {
		loadClassId();
	}
	lockChange();
}

RunChangeDialog::~RunChangeDialog()
{
	delete ui;
}

QxEventService *RunChangeDialog::service()
{
	auto *svc = qobject_cast<QxEventService*>(Service::serviceByName(QxEventService::serviceId()));
	Q_ASSERT(svc);
	return svc;
}

void RunChangeDialog::setMessage(const QString &msg, bool error)
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

void RunChangeDialog::loadOrigValues()
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

void RunChangeDialog::loadClassId()
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

void RunChangeDialog::lockChange()
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

void RunChangeDialog::resolveChanges(bool is_accepted)
{
	if (is_accepted) {
		applyLocalChanges(is_accepted);
	}
	auto *svc = service();
	auto *nm = svc->networkManager();
	QNetworkRequest request;
	auto url = svc->exchangeServerUrl();
	// qfInfo() << "url " << url.toString();
	url.setPath("/api/event/current/changes/resolve-change");

	QUrlQuery query;
	query.addQueryItem("change_id", QString::number(m_changeId));
	query.addQueryItem("lock_number", QString::number(m_lockNumber));
	query.addQueryItem("accepted", is_accepted? "true": "false");
	query.addQueryItem("status_message", ui->edStatusMessage->text());
	url.setQuery(query);

	request.setUrl(url);
	request.setRawHeader(QxEventService::QX_API_TOKEN, svc->apiToken());
	auto *reply = nm->get(request);
	connect(reply, &QNetworkReply::finished, this, [this, reply]() {
		if (reply->error() == QNetworkReply::NetworkError::NoError) {
			accept();
		}
		else {
			QMessageBox::warning(this,
								 QCoreApplication::applicationName(),
								 tr("Update change error: %1").arg(reply->errorString()));
		}
		reply->deleteLater();
	});
}

void RunChangeDialog::applyLocalChanges(bool is_accepted)
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
			qf::core::sql::Query q;
			q.execThrow(QStringLiteral("UPDATE runs SET siId=%1 WHERE id=%2")
						.arg(ui->edSiCard->value())
						.arg(m_runId)
						);
		}
	}
	doc.save();
	{
		qf::core::sql::Query q;
		q.execThrow(QStringLiteral("UPDATE qxchanges SET status='%1' WHERE id=%2")
					.arg(is_accepted? "Accepted": "Rejected")
					.arg(m_changeId)
					);
	}
	{
		auto dc_str = qf::core::Utils::qvariantToJson(m_origValues.toVariantMap());
		QString qs = "UPDATE qxchanges SET orig_data=:orig_data WHERE id=:id";
		qf::core::sql::Query q;
		q.prepare(qs, qf::core::Exception::Throw);
		q.bindValue(":orig_data", dc_str);
		q.bindValue(":id", m_changeId);
		q.exec(qf::core::Exception::Throw);
	}
}

bool RunChangeDialog::checkHttpError(QNetworkReply *reply)
{
	if (reply->error() != QNetworkReply::NetworkError::NoError) {
		setMessage(tr("Http error: %1\n%2")
				   .arg(reply->request().url().toString())
				   .arg(reply->errorString()), true);
		return false;
	}
	return true;
}

} // namespace Event::services::qx


