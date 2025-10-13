#include "chooseoriseventdialog.h"
#include "ui_chooseoriseventdialog.h"

#include "orisimporter.h"

#include <qf/core/utils.h>
#include <qf/core/log.h>

#include <QDate>
#include <QUrl>
#include <QJsonObject>
#include <QJsonDocument>
#include <QCompleter>
#include <QLineEdit>

ChooseOrisEventDialog::ChooseOrisEventDialog(OrisImporter *importer, QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::ChooseOrisEventDialog)
	, m_importer(importer)
{
	ui->setupUi(this);

	connect(ui->cbxOrisEvent, &QComboBox::currentIndexChanged, this, &ChooseOrisEventDialog::onOrisEvent_currentIndexChanged);
}

ChooseOrisEventDialog::~ChooseOrisEventDialog()
{
	delete ui;
}

int ChooseOrisEventDialog::exec()
{
	load();
	return Super::exec();
}

int ChooseOrisEventDialog::eventId()
{
	return ui->edEventId->value();
}

void ChooseOrisEventDialog::load()
{
	ui->cbxOrisEvent->lineEdit()->setPlaceholderText(tr("Loading event list from Oris ..."));
	ui->cbxOrisEvent->clear();
	//QCoreApplication::processEvents();
	QDate d = QDate::currentDate();
	d = d.addMonths(-1);
	QUrl url("https://" + OrisImporter::orisDomainName() + "/API/?format=json&method=getEventList&all=1&datefrom=" + d.toString(Qt::ISODate));
	m_importer->getJsonAndProcess(url, this, [this](const QJsonDocument &jsd) {
		//qfWarning() << QString::fromUtf8(jsd.toJson());
		OrisImporter::saveJsonBackup("EventList", jsd);
		QJsonObject jso = jsd.object().value(QStringLiteral("Data")).toObject();
		QMap<QString, int> events_by_descr; //event_descr->event_id
		for(auto it = jso.constBegin(); it != jso.constEnd(); ++it) {
			QJsonObject event = it.value().toObject();
			int event_id = event.value(QStringLiteral("ID")).toString().toInt();
			QJsonObject org1 = event.value(QStringLiteral("Org1")).toObject();
			QJsonObject sport = event.value(QStringLiteral("Sport")).toObject();
			int sport_id = sport.value(QStringLiteral("ID")).toString().toInt();
			QString sport_abbr = (sport_id == 1)? tr("OB"):
								 (sport_id == 2)? tr("LOB"):
								 (sport_id == 3)? tr("MTBO"):
								 (sport_id == 4)? tr("TRAIL"):
								 tr("???");
			QString event_description = event.value(QStringLiteral("Date")).toString()
										+ " " + QString::number(event_id)
										+ " " + sport_abbr
										+ " " + org1.value(QStringLiteral("Abbr")).toString()
										+ " " + event.value(QStringLiteral("Name")).toString();
			//qfInfo() << event_description;
			events_by_descr[event_description] = event_id;
		}
		QMapIterator<QString, int> it(events_by_descr);
		while(it.hasNext()) {
			it.next();
			ui->cbxOrisEvent->addItem(it.key(), it.value());
		}
		QF_SAFE_DELETE(m_completer);
		m_completer = new QCompleter(ui->cbxOrisEvent->model(), this);
		m_completer->setFilterMode(Qt::MatchContains);
		m_completer->setCaseSensitivity(Qt::CaseInsensitive);
		ui->cbxOrisEvent->setCompleter(m_completer);

		ui->cbxOrisEvent->lineEdit()->setPlaceholderText(tr("Search in events ..."));
		//ui->cbxOrisEvent->lineEdit()->selectAll();
		ui->cbxOrisEvent->lineEdit()->clear();
		//ui->edEventId->setValue(0);
	});
}

void ChooseOrisEventDialog::onOrisEvent_currentIndexChanged(int ix)
{
	int event_id = ui->cbxOrisEvent->itemData(ix).toInt();
	ui->edEventId->setValue(event_id);
}
