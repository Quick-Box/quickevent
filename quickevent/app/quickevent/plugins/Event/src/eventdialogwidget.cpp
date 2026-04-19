#include "eventdialogwidget.h"
#include "ui_eventdialogwidget.h"

#include "eventconfig.h"

#include <qf/core/collator.h>

EventDialogWidget::EventDialogWidget(QWidget *parent) :
	Super(parent),
	ui(new Ui::EventDialogWidget)
{
	setPersistentSettingsId("EventDialogWidget");
	ui->setupUi(this);

	connect(ui->ed_iofRace, &QAbstractButton::toggled, ui->frameIofRace, &QWidget::setVisible);
	ui->frameIofRace->hide();

	connect(ui->ed_orisRace, &QAbstractButton::toggled, ui->frameOrisRace, &QWidget::setVisible);
	ui->frameOrisRace->hide();

	ui->cbxDisciplineId->addItem(tr("Long distance"), static_cast<int>(Event::EventConfig::Discipline::LongDistance));
	ui->cbxDisciplineId->addItem(tr("Middle distance"), static_cast<int>(Event::EventConfig::Discipline::ShortDistance));
	ui->cbxDisciplineId->addItem(tr("Ultralong distance"), static_cast<int>(Event::EventConfig::Discipline::UltralongDistance));
	ui->cbxDisciplineId->addItem(tr("Sprint"), static_cast<int>(Event::EventConfig::Discipline::Sprint));
	ui->cbxDisciplineId->addItem(tr("Relays"), static_cast<int>(Event::EventConfig::Discipline::Relays));
	ui->cbxDisciplineId->addItem(tr("Teams"), static_cast<int>(Event::EventConfig::Discipline::Teams));
	ui->cbxDisciplineId->addItem(tr("Free order"), static_cast<int>(Event::EventConfig::Discipline::FreeOrder));
	ui->cbxDisciplineId->addItem(tr("Night"), static_cast<int>(Event::EventConfig::Discipline::NightRace));
	ui->cbxDisciplineId->addItem(tr("Sprint relays"), static_cast<int>(Event::EventConfig::Discipline::SprintRelays));
	ui->cbxDisciplineId->addItem(tr("Knock-out sprint"), static_cast<int>(Event::EventConfig::Discipline::KnocOutSprint));
	ui->cbxDisciplineId->addItem(tr("TempO"), static_cast<int>(Event::EventConfig::Discipline::TempO));
	ui->cbxDisciplineId->addItem(tr("Multi stages"), static_cast<int>(Event::EventConfig::Discipline::MultiStages));
	ui->cbxDisciplineId->addItem(tr("Indoor"), static_cast<int>(Event::EventConfig::Discipline::Indoor));
	ui->cbxDisciplineId->addItem(tr("Mass start"), static_cast<int>(Event::EventConfig::Discipline::MassStart));


	ui->ed_oneTenthSecResults->setDisabled(true);

	QRegularExpression rx("[a-z][a-z0-9_]*"); // PostgreSQL schema must start with small letter and it may contain small letters, digits and underscores only.
	QValidator *validator = new QRegularExpressionValidator(rx, this);
	ui->ed_eventId->setValidator(validator);
}

EventDialogWidget::~EventDialogWidget()
{
	delete ui;
}

void EventDialogWidget::setEventId(const QString &event_id)
{
	QByteArray la = qf::core::Collator::toAscii7(QLocale::Czech, event_id, true);
	ui->ed_eventId->setText(QString::fromUtf8(la));
}

QString EventDialogWidget::eventId() const
{
	QString event_id = ui->ed_eventId->text();
	QByteArray la = qf::core::Collator::toAscii7(QLocale::Czech, event_id, true);
	return QString::fromUtf8(la);
}

void EventDialogWidget::setEventIdEditable(bool b)
{
	ui->ed_eventId->setReadOnly(!b);
}

void EventDialogWidget::loadParams(const QVariantMap &params)
{
	ui->ed_stageCount->setValue(params.value("stageCount").toInt());
	//ui->ed_currentStage->setValue(params.value("currentStageId").toInt());
	ui->ed_name->setText(params.value("name").toString());
	QDate date = params.value("date").toDate();
	if(!date.isValid())
		date = QDate::currentDate();
	ui->ed_date->setDate(date);
	QTime time = params.value("time").toTime();
	if(time.isValid())
		ui->ed_time->setTime(time);
	ui->ed_description->setText(params.value("description").toString());
	ui->ed_place->setText(params.value("place").toString());
	ui->ed_mainReferee->setText(params.value("mainReferee").toString());
	ui->ed_director->setText(params.value("director").toString());
	ui->ed_handicapLength->setValue(params.value("handicapLength").toInt());
	ui->cbxSportId->setCurrentIndex(params.value("sportId").toInt() - 1);
	if(ui->cbxSportId->currentIndex() < 0) {
		ui->cbxSportId->setCurrentIndex(0);
	}
	if (auto ix = ui->cbxDisciplineId->findData(params.value("disciplineId").toInt()); ix < 0) {
		ui->cbxDisciplineId->setCurrentIndex(0);
	} else {
		ui->cbxDisciplineId->setCurrentIndex(ix);
	}
	ui->ed_orisImportId->setText(params.value("importId").toString());
	ui->ed_orisRace->setChecked(!ui->ed_orisImportId->text().isEmpty());
	ui->ed_orisEventKey->setText(params.value("orisEventKey").toString());
	ui->ed_cardChecCheckTimeSec->setValue(params.value("cardChechCheckTimeSec").toInt());
	ui->ed_oneTenthSecResults->setCurrentIndex(params.value("oneTenthSecResults").toInt());
	ui->ed_iofRace->setChecked(params.value("iofRace").toInt() != 0);
	ui->ed_xmlRaceNumber->setValue(params.value("iofXmlRaceNumber").toInt());
}

QVariantMap EventDialogWidget::saveParams()
{
	QVariantMap ret;
	ret["stageCount"] = ui->ed_stageCount->value();
	//ret["currentStageId"] = ui->ed_currentStage->value();
	ret["name"] = ui->ed_name->text();
	ret["date"] = ui->ed_date->date();
	ret["time"] = ui->ed_time->time();
	ret["description"] = ui->ed_description->text();
	ret["place"] = ui->ed_place->text();
	ret["mainReferee"] = ui->ed_mainReferee->text();
	ret["director"] = ui->ed_director->text();
	ret["handicapLength"] = ui->ed_handicapLength->value();
	ret["sportId"] = (ui->cbxSportId->currentIndex() <= 0) ? 1 : ui->cbxSportId->currentIndex() + 1;
	ret["disciplineId"] = (ui->cbxDisciplineId->currentIndex() <= 0) ? static_cast<int>(Event::EventConfig::Discipline::LongDistance) : ui->cbxDisciplineId->currentData();
	ret["importId"] = ui->ed_orisImportId->text().toInt();
	ret["orisEventKey"] = ui->ed_orisEventKey->text();
	ret["cardChechCheckTimeSec"] = ui->ed_cardChecCheckTimeSec->value();
	ret["oneTenthSecResults"] = ui->ed_oneTenthSecResults->currentIndex();
	ret["iofRace"] = (int)ui->ed_iofRace->isChecked();
	ret["iofXmlRaceNumber"] = ui->ed_xmlRaceNumber->value();
	return ret;
}
