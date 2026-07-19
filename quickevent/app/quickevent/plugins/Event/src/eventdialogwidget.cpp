#include "eventdialogwidget.h"
#include "ui_eventdialogwidget.h"

#include <qf/core/collator.h>

#include <QDateTimeEdit>
#include <QHeaderView>
#include <QTableWidget>



EventDialogWidget::EventDialogWidget(QWidget *parent) :
	Super(parent),
	ui(new Ui::EventDialogWidget)
{
	setPersistentSettingsId("EventDialogWidget");
	ui->setupUi(this);

	ui->stageStartTimesTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	ui->stageStartTimesTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
	ui->stageStartTimesTable->verticalHeader()->hide();
	connect(ui->ed_stageCount, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() {
		updateStageStartTimeEditors(saveParams());
	});

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

void EventDialogWidget::updateStageStartTimeEditors(const EventDialogWidget::Params &params)
{
	QDateTime event_start(params.eventConfig.date, params.eventConfig.time);

	QTableWidget *table = ui->stageStartTimesTable;
	table->setRowCount(params.eventConfig.stageCount);
	for(int row = 0; row < params.eventConfig.stageCount; ++row) {
		auto stage_id = row + 1;
		auto *stage_item = new QTableWidgetItem(QString::number(stage_id));
		stage_item->setTextAlignment(Qt::AlignCenter);
		table->setItem(row, 0, stage_item);

		auto stage_start = params.stageStarts.value(row, event_start.addDays(row));
		auto *editor = new QDateTimeEdit(stage_start, table);
		editor->setCalendarPopup(true);
		editor->setDisplayFormat(QStringLiteral("dd.MM.yyyy HH:mm:ss"));
		table->setCellWidget(row, 1, editor);
	}
	table->resizeRowsToContents();
	const int visible_row_count = qMin(table->rowCount(), 5);
	int height = table->horizontalHeader()->height() + 2 * table->frameWidth();
	for(int row = 0; row < visible_row_count; ++row) {
		height += table->rowHeight(row);
	}
	table->setFixedHeight(height);
}

void EventDialogWidget::loadParams(const EventDialogWidget::Params &params)
{
	ui->ed_name->setText(params.eventConfig.name);
	ui->ed_date->setDate(params.eventConfig.date.isValid() ? params.eventConfig.date : QDate::currentDate());
	if(params.eventConfig.time.isValid())
		ui->ed_time->setTime(params.eventConfig.time);

	ui->ed_stageCount->setValue(params.eventConfig.stageCount);
	updateStageStartTimeEditors(params);
	ui->ed_description->setText(params.eventConfig.description);
	ui->ed_place->setText(params.eventConfig.place);
	ui->ed_mainReferee->setText(params.eventConfig.mainReferee);
	ui->ed_director->setText(params.eventConfig.director);
	ui->ed_handicapLength->setValue(params.eventConfig.handicapLength);
	if(auto ix = ui->cbxSportId->findData(params.eventConfig.sportId); ix < 0)
		ui->cbxSportId->setCurrentIndex(0);
	else
		ui->cbxSportId->setCurrentIndex(ix);
	if(auto ix = ui->cbxDisciplineId->findData(params.eventConfig.disciplineId); ix < 0)
		ui->cbxDisciplineId->setCurrentIndex(0);
	else
		ui->cbxDisciplineId->setCurrentIndex(ix);
	ui->ed_orisImportId->setText(params.eventConfig.importId > 0 ? QString::number(params.eventConfig.importId) : QString());
	ui->ed_orisRace->setChecked(params.eventConfig.importId > 0);
	ui->ed_orisEventKey->setText(params.eventConfig.orisEventKey);
	ui->ed_cardChecCheckTimeSec->setValue(params.eventConfig.cardCheckTimeSec);
	ui->ed_oneTenthSecResults->setCurrentIndex(params.eventConfig.oneTenthSecResults);
	ui->ed_iofRace->setChecked(params.eventConfig.iofRace);
	ui->ed_xmlRaceNumber->setValue(params.eventConfig.iofXmlRaceNumber);
}

EventDialogWidget::Params EventDialogWidget::saveParams()
{
    EventDialogWidget::Params params;
	params.eventConfig.stageCount = ui->ed_stageCount->value();
	for(int row = 0; row < ui->stageStartTimesTable->rowCount(); ++row) {
		if(auto *editor = qobject_cast<QDateTimeEdit *>(ui->stageStartTimesTable->cellWidget(row, 1))) {
			params.stageStarts << editor->dateTime();
		}
	}
	params.eventConfig.name = ui->ed_name->text();
	params.eventConfig.date = ui->ed_date->date();
	params.eventConfig.time = ui->ed_time->time();
	params.eventConfig.description = ui->ed_description->text();
	params.eventConfig.place = ui->ed_place->text();
	params.eventConfig.mainReferee = ui->ed_mainReferee->text();
	params.eventConfig.director = ui->ed_director->text();
	params.eventConfig.handicapLength = ui->ed_handicapLength->value();
	params.eventConfig.sportId = ui->cbxSportId->currentData().isNull()
		? static_cast<int>(Event::EventConfig::Sport::OB)
		: ui->cbxSportId->currentData().toInt();
	params.eventConfig.disciplineId = ui->cbxDisciplineId->currentIndex() <= 0
		? static_cast<int>(Event::EventConfig::Discipline::LongDistance)
		: ui->cbxDisciplineId->currentData().toInt();
	params.eventConfig.importId = ui->ed_orisImportId->text().toInt();
	params.eventConfig.orisEventKey = ui->ed_orisEventKey->text();
	params.eventConfig.cardCheckTimeSec = ui->ed_cardChecCheckTimeSec->value();
	params.eventConfig.oneTenthSecResults = ui->ed_oneTenthSecResults->currentIndex();
	params.eventConfig.iofRace = ui->ed_iofRace->isChecked();
	params.eventConfig.iofXmlRaceNumber = ui->ed_xmlRaceNumber->value();
	return params;
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
