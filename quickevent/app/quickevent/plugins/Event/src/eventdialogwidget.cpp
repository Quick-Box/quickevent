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

	using S = Event::EventConfig::Sport;
	for (S sport : {S::OB, S::LOB, S::MTBO, S::TRAIL})
		ui->cbxSportId->addItem(sportName(static_cast<int>(sport)), static_cast<int>(sport));

	using D = Event::EventConfig::Discipline;
	for (D disc : {D::LongDistance, D::ShortDistance, D::UltralongDistance, D::Sprint,
	               D::Relays, D::Teams, D::FreeOrder, D::NightRace, D::SprintRelays,
	               D::KnocOutSprint, D::TempO, D::MultiStages, D::Indoor, D::MassStart}) {
		ui->cbxDisciplineId->addItem(disciplineName(static_cast<int>(disc)), static_cast<int>(disc));
	}


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
	if (auto ix = ui->cbxSportId->findData(params.value("sportId").toInt()); ix < 0) {
		ui->cbxSportId->setCurrentIndex(0);
	} else {
		ui->cbxSportId->setCurrentIndex(ix);
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
	ret["sportId"] = ui->cbxSportId->currentData().isNull() ? static_cast<int>(Event::EventConfig::Sport::OB) : ui->cbxSportId->currentData().toInt();
	ret["disciplineId"] = (ui->cbxDisciplineId->currentIndex() <= 0) ? static_cast<int>(Event::EventConfig::Discipline::LongDistance) : ui->cbxDisciplineId->currentData();
	ret["importId"] = ui->ed_orisImportId->text().toInt();
	ret["orisEventKey"] = ui->ed_orisEventKey->text();
	ret["cardChechCheckTimeSec"] = ui->ed_cardChecCheckTimeSec->value();
	ret["oneTenthSecResults"] = ui->ed_oneTenthSecResults->currentIndex();
	ret["iofRace"] = (int)ui->ed_iofRace->isChecked();
	ret["iofXmlRaceNumber"] = ui->ed_xmlRaceNumber->value();
	return ret;
}

QString EventDialogWidget::disciplineName(int disc_id)
{
	using D = Event::EventConfig::Discipline;
	switch (static_cast<D>(disc_id)) {
	case D::LongDistance:      return tr("Long distance");
	case D::ShortDistance:     return tr("Middle distance");
	case D::UltralongDistance: return tr("Ultralong distance");
	case D::Sprint:            return tr("Sprint");
	case D::Relays:            return tr("Relays");
	case D::Teams:             return tr("Teams");
	case D::FreeOrder:         return tr("Free order");
	case D::NightRace:         return tr("Night");
	case D::SprintRelays:      return tr("Sprint relays");
	case D::KnocOutSprint:     return tr("Knock-out sprint");
	case D::TempO:             return tr("TempO");
	case D::MultiStages:       return tr("Multi stages");
	case D::MassStart:         return tr("Mass start");
	case D::Indoor:            return tr("Indoor");
	}
	return {};
}

QString EventDialogWidget::sportName(int sport_id)
{
	using S = Event::EventConfig::Sport;
	switch (static_cast<S>(sport_id)) {
	case S::OB:    return QStringLiteral("OB");
	case S::LOB:   return QStringLiteral("LOB");
	case S::MTBO:  return QStringLiteral("MTBO");
	case S::TRAIL: return QStringLiteral("TRAIL");
	}
	return {};
}
