#include "txtimporter.h"

#include <qf/gui/framework/mainwindow.h>
#include <qf/gui/dialogs/filedialog.h>
#include <qf/gui/dialogs/messagebox.h>

#include <plugins/Competitors/src/competitordocument.h>
#include <plugins/Event/src/eventplugin.h>
#include <qf/core/assert.h>
#include <qf/core/exception.h>
#include <qf/core/sql/query.h>
#include <qf/core/sql/transaction.h>
#include <qf/core/utils/csvreader.h>

#include <QTextStream>

namespace qfd = qf::gui::dialogs;
using qf::gui::framework::getPlugin;
using Event::EventPlugin;

TxtImporter::TxtImporter(QObject *parent)
	: QObject(parent)
{
}

void TxtImporter::importCompetitorsCSOS()
{
	qf::gui::framework::MainWindow *fwk = qf::gui::framework::MainWindow::frameWork();
	qf::gui::dialogs::MessageBox mbx(fwk);
	mbx.setIcon(QMessageBox::Information);
	mbx.setText(tr("Import windows-1250 coded fixed column size text files in CSOS format."));
	mbx.setInformativeText(tr("Each row should have following columns: "
							  "<ol>"
							  "<li>7 chars: Registration</li>"
							  "<li>1 space</li>"
							  "<li>10 chars: Class</li>"
							  "<li>1 space</li>"
							  "<li>10 chars: SI</li>"
							  "<li>1 space</li>"
							  "<li>25 chars: Name</li>"
							  "<li>1 space</li>"
							  "<li>2 chars: Licence</li>"
							  "<li>1 space</li>"
							  "<li>rest of line: Note</li>"
							  "</ol>"));
	mbx.setDoNotShowAgainPersistentKey("importCompetitorsCSOS");
	int res = mbx.exec();
	//qfInfo() << "RES:" << res;
	if(res != QMessageBox::Ok)
		return;
	QString fn = qfd::FileDialog::getOpenFileName(fwk, tr("Open file"), QString(), tr("CSOS files (*.txt)"));
	if(fn.isEmpty())
		return;
	try {
		QFile f(fn);
		if(!f.open(QFile::ReadOnly))
			QF_EXCEPTION(tr("Cannot open file '%1' for reading.").arg(fn));
		QList<QVariantList> csv_rows;
		QTextStream stream(&f);
#if QT_VERSION_MAJOR >= 6
		stream.setEncoding(QStringConverter::System);
#else
		stream.setCodec("windows-1250");
#endif
		QString line;
		while (stream.readLineInto(&line)) {
			if(line.isEmpty())
				continue;
			if(line.startsWith('#'))
				continue;
			// reg 7, ' ', class 10, ' ', SI 10, ' ', 25 name, ' ', 2 lic, ' ', note
			QString reg = line.mid(0, 7).trimmed();
			QString class_name = line.mid(8, 10).trimmed();
			int si = line.mid(19, 10).toInt();
			QString last_name = line.mid(30, 25).trimmed();
			QString lic = line.mid(56, 2).trimmed();
			QString note = line.mid(59).trimmed();
			csv_rows << (QVariantList() << reg << class_name << si << last_name << QString() << lic << note);
		}
		qfLogScope("importCompetitorsCSOS");
		qf::core::sql::Transaction transaction;
		importParsedCsv(csv_rows);
		transaction.commit();
		emit getPlugin<EventPlugin>()->reloadDataRequest();
	}
	catch (qf::core::Exception &e) {
		qf::gui::dialogs::MessageBox::showException(fwk, e);
	}
}

void TxtImporter::importCompetitorsCSV()
{
	qf::gui::framework::MainWindow *fwk = qf::gui::framework::MainWindow::frameWork();
	qf::gui::dialogs::MessageBox mbx(fwk);
	mbx.setIcon(QMessageBox::Information);
	mbx.setText(tr("Import UTF8 text file with comma separated values with first row as header.<br/>Separator is comma(,)"));
	mbx.setInformativeText(tr("Each row should have following columns: "
							  "<ol>"
							  "<li>Registration</li>"
							  "<li>Class</li>"
							  "<li>SI</li>"
							  "<li>LastName</li>"
							  "<li>FirstName</li>"
							  "<li>Licence</li>"
							  "<li>Note</li>"
							  "</ol>"));
	mbx.setDoNotShowAgainPersistentKey("importCompetitorsCSV");
	int res = mbx.exec();
	if(res != QMessageBox::Ok)
		return;
	QString fn = qfd::FileDialog::getOpenFileName(fwk, tr("Open file"), QString(), tr("CSV files (*.csv *.txt)"));
	if(fn.isEmpty())
		return;
	try {
		QFile f(fn);
		if(!f.open(QFile::ReadOnly))
			QF_EXCEPTION(tr("Cannot open file '%1' for reading.").arg(fn));
		QList<QVariantList> csv_rows;
		QTextStream ts(&f);
#if QT_VERSION_MAJOR < 6
		ts.setCodec("utf-8");
#endif
		qf::core::utils::CSVReader reader(&ts);
		reader.setSeparator(',');
		reader.setLineComment('#');
		/// skip header
		if (!ts.atEnd())
			reader.readCSVLine();
		while (!ts.atEnd()) {
			QStringList sl = reader.readCSVLineSplitted();
			QString reg = sl.value(ColRegistration).trimmed();
			QString class_name = sl.value(ColClassName).trimmed();
			int si = sl.value(ColSiId).trimmed().toInt();
			QString first_name = sl.value(ColFirstName).trimmed();
			QString last_name = sl.value(ColLastName).trimmed();
			QString lic = sl.value(ColLicence).trimmed();
			QString note = sl.value(ColNote).trimmed();
			csv_rows << (QVariantList() << reg << class_name << si << last_name << first_name << lic << note);
		}
		qfLogScope("importCompetitorsCSV");
		qf::core::sql::Transaction transaction;
		importParsedCsv(csv_rows);
		transaction.commit();
		emit getPlugin<EventPlugin>()->reloadDataRequest();
	}
	catch (qf::core::Exception &e) {
		qf::gui::dialogs::MessageBox::showException(fwk, e);
	}
}

void TxtImporter::importRankingCsv()
{
	qfLogFuncFrame();
	qf::gui::framework::MainWindow *fwk = qf::gui::framework::MainWindow::frameWork();
	QString fn = qfd::FileDialog::getOpenFileName(fwk, tr("Open file"), QString(), tr("Oris ranking CSV files (*.txt *.csv)"));
	if(fn.isEmpty())
		return;
	try {
		QFile f(fn);
		if(!f.open(QFile::ReadOnly))
			QF_EXCEPTION(tr("Cannot open file '%1' for reading.").arg(fn));
		QTextStream ts(&f);
		qf::core::utils::CSVReader reader(&ts);
		reader.setSeparator(';');
		//reader.setLineComment('#');
		enum {ColPos = 0, ColLastName, ColFirstName, ColRegistration, ColPoints, ColCoef};

		qfLogScope("importRankingCsv");
		qf::core::sql::Transaction transaction;
		qf::core::sql::Query q;
		q.prepare("UPDATE competitors SET ranking=:ranking WHERE registration=:registration", qf::core::Exception::Throw);

		int n = 0;
		while (!ts.atEnd()) {
			QStringList line = reader.readCSVLineSplitted();
			if(line.count() <= 1)
				QF_EXCEPTION(tr("Fields separation error, invalid CSV format, Error reading CSV line: [%1]").arg(line.join(';').mid(0, 100)));
			if(n++ == 0) // skip column names
				continue;
			QString registration = line.value(ColRegistration);
			int pos = line.value(ColPos).toInt();
			if(pos == 0 || registration.isEmpty()) {
				QF_EXCEPTION(tr("Error reading CSV line: [%1]").arg(line.join(';')));
			}
			qfDebug() << registration << "->" << pos;
			q.bindValue(":ranking", pos);
			q.bindValue(":registration", registration);
			q.exec(qf::core::Exception::Throw);
		}
		transaction.commit();
		qfInfo() << fn << n << "lines imported";
	}
	catch (const qf::core::Exception &e) {
		qf::gui::dialogs::MessageBox::showException(fwk, e);
	}
}

void TxtImporter::importParsedCsv(const QList<QVariantList> &csv)
{
	QMap<QString, int> classes_map; // classes.name->classes.id
	qf::core::sql::Query q;
	q.exec("SELECT id, name FROM classes", qf::core::Exception::Throw);
	while(q.next()) {
		classes_map[q.value(1).toString()] = q.value(0).toInt();
	}
	for(const QVariantList &row : csv) {
		Competitors::CompetitorDocument doc;
		doc.setEmitDbEventsOnSave(false);
		doc.loadForInsert();
		int siid = row.value(ColSiId).toInt();
		//qfInfo() << "SI:" << siid, competitor_obj.ClassDesc, ' ', competitor_obj.LastName, ' ', competitor_obj.FirstName, "classId:", parseInt(competitor_obj.ClassID));
		QString note = row.value(ColNote).toString();
		QString first_name = row.value(ColFirstName).toString();
		QString last_name = row.value(ColLastName).toString();
		if(first_name.isEmpty() && !last_name.isEmpty()) {
			int ix = last_name.indexOf(' ');
			if(ix > 0) {
				first_name = last_name.mid(ix + 1).trimmed();
				last_name = last_name.mid(0, ix).trimmed();
			}
		}
		QString reg = row.value(ColRegistration).toString();
		QString class_name = row.value(ColClassName).toString();
		int class_id = classes_map.value(class_name);
		if(class_id == 0) {
			QF_EXCEPTION(tr("Undefined class name: '%1'").arg(class_name));
		}
		//fwk->showProgress("Importing: " + reg_no + ' ' + last_name + ' ' + first_name, items_processed, items_count);
		//	qfWarning() << tr("%1 %2 %3 SI: %4 is duplicit!").arg(reg_no).arg(last_name).arg(first_name).arg(siid);
		doc.setValue("classId", class_id);
		if(siid > 0) {
			doc.setSiid(siid);
		}
		doc.setValue("firstName", first_name);
		doc.setValue("lastName", last_name);
		doc.setValue("registration", reg);
		doc.setValue("licence", row.value(ColLicence).toString());
		doc.setValue("note", note);
		doc.save();
	}
	getPlugin<EventPlugin>()->emitReloadDataRequest();
	getPlugin<EventPlugin>()->emitDbEvent(Event::EventPlugin::DBEVENT_COMPETITOR_COUNTS_CHANGED);
}

int TxtImporter::getStartTimeMSec(QString str, int start00_msec)
{
	bool ok;
	double dbl_time = str.toDouble(&ok);
	int st_time = 0;
	if(ok) {
		st_time = static_cast<int>(dbl_time) * 60 + ((static_cast<int>(dbl_time * 100)) % 100);
		st_time *= 1000;
	} else { // time hh:mm:ss
		QTime t = QTime::fromString(str,(str.size() > 7) ?"hh:mm:ss" : "h:mm:ss");
		st_time = t.hour() * 3600 + t.minute() * 60 + t.second();
		st_time *= 1000;
		st_time -= start00_msec;
	}
	return st_time;
}


void TxtImporter::importRunsCzeCSV()
{
	qf::gui::framework::MainWindow *fwk = qf::gui::framework::MainWindow::frameWork();
	qf::gui::dialogs::MessageBox mbx(fwk);
	mbx.setIcon(QMessageBox::Information);
	mbx.setText(tr("Import UTF8 text file with comma separated values with first row as header.<br/>Separator is semicolon(;).<br/>Updates only existing runners (key is Czech registration)."));
	mbx.setInformativeText(tr("Each row should have following columns: "
							  "<ol>"
							  "<li>Registration <i>- key</i></li>"
							  "<li>SI</li>"
							  "<li>Class</li>"
							  "<li>Bib</li>"
							  "<li>Start time <i>(in format: <b>mmm.ss</b> from zero time or <b>hh:mm:ss</b>)</i></li>"
							  "</ol> Only first column is mandatory, others can be empty."));
	mbx.setDoNotShowAgainPersistentKey("importRunsCzeCSV");
	int res = mbx.exec();
	if(res != QMessageBox::Ok)
		return;
	QString fn = qfd::FileDialog::getOpenFileName(fwk, tr("Open file"), QString(), tr("CSV files (*.csv *.txt)"));
	if(fn.isEmpty())
		return;

	QMap<QString, int> classes_map; // classes.name->classes.id
	qf::core::sql::Query q;
	q.exec("SELECT id, name FROM classes", qf::core::Exception::Throw);
	while(q.next()) {
		classes_map[q.value(1).toString()] = q.value(0).toInt();
	}

	int stage_id = getPlugin<EventPlugin>()->currentStageId();
	auto start00_day_msec = getPlugin<EventPlugin>()->stageStartTime(stage_id).msecsSinceStartOfDay();

	try {
		QFile f(fn);
		if(!f.open(QFile::ReadOnly))
			QF_EXCEPTION(tr("Cannot open file '%1' for reading.").arg(fn));
		QTextStream ts(&f);
		qf::core::utils::CSVReader reader(&ts);
		reader.setSeparator(';');
		enum {ColRegistration = 0, ColSI, ColClass, ColBib, ColStarttime};

		qfLogScope("importRunsCzeCSV");
		qf::core::sql::Transaction transaction;
		qf::core::sql::Query q1a,q1b,q2,q3,q4;
		q.prepare("SELECT id FROM competitors WHERE registration=:registration", qf::core::Exception::Throw);
		q1a.prepare("UPDATE competitors SET siId=:si WHERE id=:id", qf::core::Exception::Throw);
		q1b.prepare("UPDATE runs SET siId=:si WHERE competitorId=:id", qf::core::Exception::Throw);
		q2.prepare("UPDATE competitors SET classId=:class WHERE id=:id", qf::core::Exception::Throw);
		q3.prepare("UPDATE competitors SET startNumber=:bib WHERE id=:id", qf::core::Exception::Throw);
		q4.prepare("UPDATE runs SET startTimeMs=:starttime WHERE competitorId=:id", qf::core::Exception::Throw);

		int n = 0;
		while (!ts.atEnd()) {
			QStringList line = reader.readCSVLineSplitted();
			if(line.count() <= 1)
				QF_EXCEPTION(tr("Fields separation error, invalid CSV format, Error reading CSV line: [%1]").arg(line.join(';').mid(0, 100)));
			if(n++ == 0) // skip column names
				continue;
			QString registration = line.value(ColRegistration).trimmed();
			if(registration.isEmpty()) {
				QF_EXCEPTION(tr("Error reading CSV line: [%1]").arg(line.join(';')));
			}
			q.bindValue(":registration", registration);
			q.exec(qf::core::Exception::Throw);
			if(q.next()) {
				// if registration found in db - start update data
				int competitor_id = q.value(0).toInt();

				int si = line.value(ColSI).toInt();
				QString class_name = line.value(ColClass).trimmed();
				int bib = line.value(ColBib).toInt();
				QString starttime = line.value(ColStarttime).trimmed();

				qfDebug() << registration << "-> (" << si << "," << class_name << "," << bib << "," << starttime << ")";
				if (si != 0) {
					q1a.bindValue(":si", si);
					q1a.bindValue(":id", competitor_id);
					q1a.exec(qf::core::Exception::Throw);
					q1b.bindValue(":si", si);
					q1b.bindValue(":id", competitor_id);
					q1b.exec(qf::core::Exception::Throw);
				}
				if (!class_name.isEmpty()) {
					int class_id = classes_map.value(class_name);
					if(class_id == 0)
						QF_EXCEPTION(tr("Undefined class name: '%1'").arg(class_name));

					q2.bindValue(":class", class_id);
					q2.bindValue(":id", competitor_id);
					q2.exec(qf::core::Exception::Throw);
				}
				if (bib != 0) {
					q3.bindValue(":bib", bib);
					q3.bindValue(":id", competitor_id);
					q3.exec(qf::core::Exception::Throw);
				}
				if (!starttime.isEmpty()) {
					int st_time = getStartTimeMSec(starttime, start00_day_msec);
					q4.bindValue(":starttime", st_time);
					q4.bindValue(":id", competitor_id);
					q4.exec(qf::core::Exception::Throw);
				}
			}
			else
				qfWarning() << registration << "not found in database.";
		}
		transaction.commit();
		qfInfo() << fn << n << "lines imported";
	}
	catch (const qf::core::Exception &e) {
		qf::gui::dialogs::MessageBox::showException(fwk, e);
	}
}

void TxtImporter::importRunsIdCSV()
{
	qf::gui::framework::MainWindow *fwk = qf::gui::framework::MainWindow::frameWork();
	qf::gui::dialogs::MessageBox mbx(fwk);
	mbx.setIcon(QMessageBox::Information);
	mbx.setText(tr("Import UTF8 text file with comma separated values with first row as header.<br/>Separator is semicolon(;).<br/>Updates only existing runners (key is <b>id</b> in module(table) <b>runs</b>)."));
	mbx.setInformativeText(tr("Each row should have following columns: "
							  "<ol>"
							  "<li>Runs Id <i>- key</i></li>"
							  "<li>SI</li>"
							  "<li>Class</li>"
							  "<li>Bib</li>"
							  "<li>Start time <i>(in format: <b>mmm.ss</b> from zero time or <b>hh:mm:ss</b>)</i></li>"
							  "</ol> Only first column is mandatory, others can be empty."));
	mbx.setDoNotShowAgainPersistentKey("importRunsIdCSV");
	int res = mbx.exec();
	if(res != QMessageBox::Ok)
		return;
	QString fn = qfd::FileDialog::getOpenFileName(fwk, tr("Open file"), QString(), tr("CSV files (*.csv *.txt)"));
	if(fn.isEmpty())
		return;

	QMap<QString, int> classes_map; // classes.name->classes.id
	qf::core::sql::Query q;
	q.exec("SELECT id, name FROM classes", qf::core::Exception::Throw);
	while(q.next()) {
		classes_map[q.value(1).toString()] = q.value(0).toInt();
	}

	int stage_id = getPlugin<EventPlugin>()->currentStageId();
	auto start00_day_msec = getPlugin<EventPlugin>()->stageStartTime(stage_id).msecsSinceStartOfDay();

	try {
		QFile f(fn);
		if(!f.open(QFile::ReadOnly))
			QF_EXCEPTION(tr("Cannot open file '%1' for reading.").arg(fn));
		QTextStream ts(&f);
		qf::core::utils::CSVReader reader(&ts);
		reader.setSeparator(';');
		enum {ColRunsId = 0, ColSI, ColClass, ColBib, ColStarttime};

		qfLogScope("importRunsIdCSV");
		qf::core::sql::Transaction transaction;
		qf::core::sql::Query q1a,q1b,q2,q3,q4;
		q.prepare("SELECT id, competitorId FROM runs WHERE id=:id", qf::core::Exception::Throw);
		q1a.prepare("UPDATE competitors SET siId=:si WHERE id=:id", qf::core::Exception::Throw);
		q1b.prepare("UPDATE runs SET siId=:si WHERE id=:id", qf::core::Exception::Throw);
		q2.prepare("UPDATE competitors SET classId=:class WHERE id=:id", qf::core::Exception::Throw);
		q3.prepare("UPDATE competitors SET startNumber=:bib WHERE id=:id", qf::core::Exception::Throw);
		q4.prepare("UPDATE runs SET startTimeMs=:starttime WHERE id=:id", qf::core::Exception::Throw);

		int n = 0;
		while (!ts.atEnd()) {
			QStringList line = reader.readCSVLineSplitted();
			if(line.count() <= 1)
				QF_EXCEPTION(tr("Fields separation error, invalid CSV format, Error reading CSV line: [%1]").arg(line.join(';').mid(0, 100)));
			if(n++ == 0) // skip column names
				continue;
			QString runs_id = line.value(ColRunsId).trimmed();
			if(runs_id.isEmpty()) {
				QF_EXCEPTION(tr("Error reading CSV line: [%1]").arg(line.join(';')));
			}
			q.bindValue(":id", runs_id);
			q.exec(qf::core::Exception::Throw);
			if(q.next()) {
				// if runsId found in db - start update data
				int competitor_id = q.value(1).toInt();

				int si = line.value(ColSI).toInt();
				QString class_name = line.value(ColClass).trimmed();
				int bib = line.value(ColBib).toInt();
				QString starttime = line.value(ColStarttime).trimmed();

				qfDebug() << runs_id << "-> (" << si << "," << class_name << "," << bib << "," << starttime << ")";
				if (si != 0) {
					q1a.bindValue(":si", si);
					q1a.bindValue(":id", competitor_id);
					q1a.exec(qf::core::Exception::Throw);
					q1b.bindValue(":si", si);
					q1b.bindValue(":id", runs_id);
					q1b.exec(qf::core::Exception::Throw);
				}
				if (!class_name.isEmpty()) {
					int class_id = classes_map.value(class_name);
					if(class_id == 0)
						QF_EXCEPTION(tr("Undefined class name: '%1'").arg(class_name));

					q2.bindValue(":class", class_id);
					q2.bindValue(":id", competitor_id);
					q2.exec(qf::core::Exception::Throw);
				}
				if (bib != 0) {
					q3.bindValue(":bib", bib);
					q3.bindValue(":id", competitor_id);
					q3.exec(qf::core::Exception::Throw);
				}
				if (!starttime.isEmpty()) {
					int st_time = getStartTimeMSec(starttime, start00_day_msec);
					q4.bindValue(":starttime", st_time);
					q4.bindValue(":id", runs_id);
					q4.exec(qf::core::Exception::Throw);
				}
			}
			else
				qfWarning() << runs_id << "not found in database.";
		}
		transaction.commit();
		qfInfo() << fn << n << "lines imported";
	}
	catch (const qf::core::Exception &e) {
		qf::gui::dialogs::MessageBox::showException(fwk, e);
	}
}

void TxtImporter::importRunsIofCSV()
{
	qf::gui::framework::MainWindow *fwk = qf::gui::framework::MainWindow::frameWork();
	qf::gui::dialogs::MessageBox mbx(fwk);
	mbx.setIcon(QMessageBox::Information);
	mbx.setText(tr("Import UTF8 text file with comma separated values with first row as header.<br/>Separator is semicolon(;).<br/>Updates only existing runners (key is IOF ID)."));
	mbx.setInformativeText(tr("Each row should have following columns: "
							  "<ol>"
							  "<li>IOF ID <i>- key</i></li>"
							  "<li>SI</li>"
							  "<li>Class</li>"
							  "<li>Bib</li>"
							  "<li>Start time <i>(in format: <b>mmm.ss</b> from zero time or <b>hh:mm:ss</b>)</i></li>"
							  "</ol> Only first column is mandatory, others can be empty."));
	mbx.setDoNotShowAgainPersistentKey("importRunsIofCSV");
	int res = mbx.exec();
	if(res != QMessageBox::Ok)
		return;
	QString fn = qfd::FileDialog::getOpenFileName(fwk, tr("Open file"), QString(), tr("CSV files (*.csv *.txt)"));
	if(fn.isEmpty())
		return;

	QMap<QString, int> classes_map; // classes.name->classes.id
	qf::core::sql::Query q;
	q.exec("SELECT id, name FROM classes", qf::core::Exception::Throw);
	while(q.next()) {
		classes_map[q.value(1).toString()] = q.value(0).toInt();
	}

	int stage_id = getPlugin<EventPlugin>()->currentStageId();
	auto start00_day_msec = getPlugin<EventPlugin>()->stageStartTime(stage_id).msecsSinceStartOfDay();

	try {
		QFile f(fn);
		if(!f.open(QFile::ReadOnly))
			QF_EXCEPTION(tr("Cannot open file '%1' for reading.").arg(fn));
		QTextStream ts(&f);
		qf::core::utils::CSVReader reader(&ts);
		reader.setSeparator(';');
		enum {ColIofId = 0, ColSI, ColClass, ColBib, ColStarttime};

		qfLogScope("importRunsCzeCSV");
		qf::core::sql::Transaction transaction;
		qf::core::sql::Query q1a,q1b,q2,q3,q4;
		q.prepare("SELECT id FROM competitors WHERE iofId=:iofId", qf::core::Exception::Throw);
		q1a.prepare("UPDATE competitors SET siId=:si WHERE iofId=:iofId", qf::core::Exception::Throw);
		q1b.prepare("UPDATE runs SET siId=:si WHERE competitorId=:id", qf::core::Exception::Throw);
		q2.prepare("UPDATE competitors SET classId=:class WHERE iofId=:iofId", qf::core::Exception::Throw);
		q3.prepare("UPDATE competitors SET startNumber=:bib WHERE iofId=:iofId", qf::core::Exception::Throw);
		q4.prepare("UPDATE runs SET startTimeMs=:starttime WHERE competitorId=:id", qf::core::Exception::Throw);

		int n = 0;
		while (!ts.atEnd()) {
			QStringList line = reader.readCSVLineSplitted();
			if(line.count() <= 1)
				QF_EXCEPTION(tr("Fields separation error, invalid CSV format, Error reading CSV line: [%1]").arg(line.join(';').mid(0, 100)));
			if(n++ == 0) // skip column names
				continue;
			int iof_id = line.value(ColIofId).toInt();
			if(iof_id == 0) {
				QF_EXCEPTION(tr("Error reading CSV line: [%1]").arg(line.join(';')));
			}
			q.bindValue(":iofId", iof_id);
			q.exec(qf::core::Exception::Throw);
			if(q.next()) {
				// if registration found in db - start update data
				int competitor_id = q.value(0).toInt();

				int si = line.value(ColSI).toInt();
				QString class_name = line.value(ColClass).trimmed();
				int bib = line.value(ColBib).toInt();
				QString starttime = line.value(ColStarttime).trimmed();

				qfDebug() << iof_id << "-> (" << si << "," << class_name << "," << bib << "," << starttime << ")";
				if (si != 0) {
					q1a.bindValue(":si", si);
					q1a.bindValue(":iofId", iof_id);
					q1a.exec(qf::core::Exception::Throw);
					q1b.bindValue(":si", si);
					q1b.bindValue(":id", competitor_id);
					q1b.exec(qf::core::Exception::Throw);
				}
				if (!class_name.isEmpty()) {
					int class_id = classes_map.value(class_name);
					if(class_id == 0)
						QF_EXCEPTION(tr("Undefined class name: '%1'").arg(class_name));

					q2.bindValue(":class", class_id);
					q2.bindValue(":iofId", iof_id);
					q2.exec(qf::core::Exception::Throw);
				}
				if (bib != 0) {
					q3.bindValue(":bib", bib);
					q3.bindValue(":iofId", iof_id);
					q3.exec(qf::core::Exception::Throw);
				}
				if (!starttime.isEmpty()) {
					int st_time = getStartTimeMSec(starttime, start00_day_msec);
					q4.bindValue(":starttime", st_time);
					q4.bindValue(":id", competitor_id);
					q4.exec(qf::core::Exception::Throw);
				}
			}
			else
				qfWarning() << iof_id << "not found in database.";
		}
		transaction.commit();
		qfInfo() << fn << n << "lines imported";
	}
	catch (const qf::core::Exception &e) {
		qf::gui::dialogs::MessageBox::showException(fwk, e);
	}
}
