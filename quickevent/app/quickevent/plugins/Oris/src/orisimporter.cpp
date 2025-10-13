#include "chooseoriseventdialog.h"
#include "orisimporter.h"

#include <plugins/Event/src/eventplugin.h>
#include <plugins/Classes/src/classdocument.h>
#include <plugins/Competitors/src/competitordocument.h>

#include <qf/gui/framework/mainwindow.h>
#include <qf/gui/dialogs/dialog.h>
#include <qf/gui/dialogs/getiteminputdialog.h>
#include <qf/gui/dialogs/filedialog.h>
#include <qf/gui/dialogs/messagebox.h>
#include <qf/gui/framework/plugin.h>
#include <qf/gui/dialogbuttonbox.h>
#include <qf/gui/htmlviewwidget.h>

#include <qf/core/network/networkaccessmanager.h>
#include <qf/core/network/networkreply.h>
#include <qf/core/log.h>
#include <qf/core/sql/query.h>
#include <qf/core/sql/transaction.h>
#include <qf/core/utils/fileutils.h>
#include <qf/core/utils/htmlutils.h>
#include <qf/core/assert.h>

#include <QDate>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QPushButton>
#include <QTime>
#include <QUrl>
#include <QInputDialog>
#include <QMessageBox>
#include <set>

#if QT_VERSION_MAJOR >= 6
// workaround to decode cp-1250 on linux, needed for relays import from Oris
#include <iconv.h>

namespace {
QByteArray convertCp1250ToUtf8(const QByteArray &input) {
	iconv_t cd = iconv_open("UTF-8", "CP1250");
	if (cd == reinterpret_cast<iconv_t>(-1)) {
		qfWarning() << "iconv_open failed";
		return input;
	}

	size_t inBytesLeft = input.size();
	size_t outBytesLeft = input.size() * 2; // allocate more for UTF-8
	QByteArray output;
	output.resize(outBytesLeft);

	char *inBuf = const_cast<char *>(input.data()); // NOLINT(cppcoreguidelines-pro-type-const-cast)
	char *outBuf = output.data();

	size_t result = iconv(cd, &inBuf, &inBytesLeft, &outBuf, &outBytesLeft);
	if (result == (size_t)-1) {
		qWarning() << "iconv conversion failed";
		iconv_close(cd);
		return input;
	}

	iconv_close(cd);
	output.resize(output.size() - outBytesLeft);
	return output;
}
}
#endif

using qf::gui::framework::getPlugin;
using Event::EventPlugin;

void OrisImporter::saveJsonBackup(const QString &fn, const QJsonDocument &jsd)
{
	QString dir = qf::core::utils::FileUtils::osTempDir() + '/' + "quickevent";
	qf::core::utils::FileUtils::ensurePath(dir);
	QFile f(dir + '/' + fn + ".json");
	if(f.open(QFile::WriteOnly)) {
		QByteArray ba = jsd.toJson();
		f.write(ba);
	}
}

namespace {
QJsonDocument load_offline_json(const QString &fn)
{
	QJsonDocument ret;
	QString dir = qf::core::utils::FileUtils::osTempDir() + '/' + "quickevent";
	QFile f(dir + '/' + fn + ".load.json");
	if(f.open(QFile::ReadOnly)) {
		ret = QJsonDocument::fromJson(f.readAll());
	}
	return ret;
}
}

OrisImporter::OrisImporter(QObject *parent)
	: QObject(parent)
{

}

QString OrisImporter::orisDomainName()
{
	static auto s = QStringLiteral("oris.ceskyorientak.cz");
	return s;
}

qf::core::network::NetworkAccessManager *OrisImporter::networkAccessManager()
{
	if(!m_networkAccessManager) {
		m_networkAccessManager = new qf::core::network::NetworkAccessManager(this);
	}
	return m_networkAccessManager;
}

void OrisImporter::getJsonAndProcess(const QUrl &url, QObject *context, std::function<void (const QJsonDocument &)> process_call_back)
{
	auto *manager = networkAccessManager();
	auto *reply = manager->get(url);
	qf::gui::framework::MainWindow *fwk = qf::gui::framework::MainWindow::frameWork();
	connect(context, &QObject::destroyed, reply, &qf::core::network::NetworkReply::deleteLater); // NOLINT(readability-suspicious-call-argument)
	connect(reply, &qf::core::network::NetworkReply::downloadProgress, fwk, &qf::gui::framework::MainWindow::showProgress);
	connect(reply, &qf::core::network::NetworkReply::finished, context, [reply, process_call_back](bool get_ok) {
		qfInfo() << "Get:" << reply->url().toString() << "OK:" << get_ok;
		qf::gui::framework::MainWindow *fwk = qf::gui::framework::MainWindow::frameWork();
		if(get_ok) {
			QJsonParseError err;
			QJsonDocument jsd = QJsonDocument::fromJson(reply->data(), &err);
			if(err.error != QJsonParseError::NoError) {
				qfError() << reply->data();
				qf::gui::dialogs::MessageBox::showError(fwk, tr("JSON document parse error: %1 at: %2 near: %3")
															   .arg(err.errorString())
															   .arg(err.offset)
															   .arg(reply->data().mid(err.offset, 50).constData()));
				return;
			}
			process_call_back(jsd);
		}
		else {
			qf::gui::dialogs::MessageBox::showError(fwk, "http get error on: " + reply->url().toString() + ", " + reply->errorString());
		}
		reply->deleteLater();
	});
}

void OrisImporter::getTextAndProcess(const QUrl &url, QObject *context, std::function<void (const QByteArray &)> process_call_back)
{
	auto *manager = networkAccessManager();
	qf::core::network::NetworkReply *reply = manager->get(url);
	qf::gui::framework::MainWindow *fwk = qf::gui::framework::MainWindow::frameWork();
	connect(context, &QObject::destroyed, reply, &qf::core::network::NetworkReply::deleteLater); // NOLINT(readability-suspicious-call-argument)
	connect(reply, &qf::core::network::NetworkReply::downloadProgress, fwk, &qf::gui::framework::MainWindow::showProgress);
	connect(reply, &qf::core::network::NetworkReply::finished, context, [reply, process_call_back](bool get_ok) {
		qfInfo() << "Get:" << reply->url().toString() << "OK:" << get_ok;
		qf::gui::framework::MainWindow *fwk = qf::gui::framework::MainWindow::frameWork();
		if(get_ok) {
			process_call_back(reply->data());
		}
		else {
			qf::gui::dialogs::MessageBox::showError(fwk, "http get error on: " + reply->url().toString() + ", " + reply->errorString());
		}
		reply->deleteLater();
	});
}

void OrisImporter::syncCurrentEventEntries(std::function<void ()> success_callback)
{
	qf::gui::framework::MainWindow *fwk = qf::gui::framework::MainWindow::frameWork();
	//if(!qf::gui::dialogs::MessageBox::askYesNo(fwk, tr("All runners entries imported from Oris will be synchronized, manual changes will be lost!")))
	int oris_id = getPlugin<EventPlugin>()->eventConfig()->importId();
	if(oris_id == 0) {
		qf::gui::dialogs::MessageBox::showError(fwk, tr("Cannot find Oris import ID."));
		return;
	}
	syncEventEntries(oris_id, success_callback);
}

void OrisImporter::syncRelaysEntries(int event_id, std::function<void ()> success_callback)
{
	/*
	qf::gui::framework::MainWindow *fwk = qf::gui::framework::MainWindow::frameWork();
	if(!getPlugin<EventPlugin>()->eventConfig()->isRelays()) {
		qf::gui::dialogs::MessageBox::showError(fwk, tr("Not relays event."));
		return;
	}
	int oris_id = getPlugin<EventPlugin>()->eventConfig()->importId();
	if(oris_id == 0) {
		qf::gui::dialogs::MessageBox::showError(fwk, tr("Cannot find Oris import ID."));
		return;
	}
	*/
	QUrl url(QString("https://" + OrisImporter::orisDomainName() + "/ExportPrihlasek?id=%1").arg(event_id));
	getTextAndProcess(url, this, [=](const QByteArray &data) {
		qf::gui::framework::MainWindow *fwk = qf::gui::framework::MainWindow::frameWork();
		try {
			qfLogScope("syncRelaysEntries");
			qf::core::sql::Transaction transaction;
			qf::core::sql::Query q;

			q.execThrow("UPDATE competitors SET importId=1 WHERE importId IS NOT NULL");
			q.execThrow("UPDATE runs SET importId=1 WHERE importId IS NOT NULL");
			q.execThrow("UPDATE relays SET importId=1 WHERE importId IS NOT NULL");

			QMap<QString, int> class_ids;
			q.execThrow("SELECT id, name FROM classes");
			while(q.next())
				class_ids[q.value(1).toString()] = q.value(0).toInt();

#if QT_VERSION_MAJOR >= 6
			auto data_utf8 = convertCp1250ToUtf8(data);
			QTextStream ts(data_utf8);
			ts.setEncoding(QStringConverter::Utf8);
#else
			QTextStream ts(data);
			ts.setCodec("cp1250");
#endif
			enum E1 {
				Club = 4,
				RelayPos = 3,
				ClassName = 11,
				Reg = 8,
				Name = 28,
				SI = 9,
				//Competitor = Reg + Name + SI,
			};
			while(!ts.atEnd()) {
				QString ln = ts.readLine();
				QString line(ln);
				int n = 0;

				QString relay_club = line.mid(n, Club).trimmed(); n += Club;
				if(relay_club.isEmpty())
					continue;
				QString relay_name = line.mid(n, RelayPos).trimmed(); n += RelayPos;
				QString class_name = line.mid(n, ClassName).trimmed(); n += ClassName;

				int class_id = class_ids.value(class_name);
				if(class_id == 0) {
					qfError() << "Invalid class name" << class_name;
					continue;
				}

				qfInfo() << relay_club << relay_name << class_name << class_id;

				q.execThrow("SELECT id FROM relays WHERE"
							" name='" + relay_name + "'"
							" AND club='" + relay_club + "'"
							" AND classId=" + QString::number(class_id));
				int relay_id;
				if(q.next()) {
					relay_id = q.value(0).toInt();
					q.execThrow("UPDATE relays SET importId=2 WHERE id=" + QString::number(relay_id));
				}
				else {
					q.execThrow("INSERT INTO relays (classId, club, name, importId) VALUES ("
								+ QString::number(class_id) + ", "
								+ "'" + relay_club + "', "
								+ "'" + relay_name + "', "
								+ "2"
								+ ")");
					relay_id = q.lastInsertId().toInt();
				}

				int leg = 0;
				while(n < line.size()) {
					leg++;
					QString reg = line.mid(n, Reg).trimmed(); n += Reg;
					QString name = line.mid(n, Name).trimmed(); n += Name;
					QString last_name = name.section(' ', 0, 0);
					QString first_name = name.section(' ', 1);
					int si = line.mid(n, SI).trimmed().toInt(); n += SI;

					if(reg.isEmpty() && name.isEmpty())
						continue;

					qfInfo() << '\t' << leg << last_name << first_name << reg << si;

					int competitor_id = 0;
					{
						q.execThrow("SELECT id FROM competitors WHERE"
									" firstName='" + first_name + "'"
									" AND lastName='" + last_name + "'"
									" AND registration='" + reg + "'");
						if(q.next()) {
							competitor_id = q.value(0).toInt();
						}
						if(competitor_id == 0 && !reg.isEmpty()) {
								q.execThrow("SELECT id FROM competitors WHERE"
											" registration='" + reg + "'");
								if(q.next())
									competitor_id = q.value(0).toInt();
						}
						/*
						if(competitor_id == 0) {
							q.execThrow("SELECT id FROM competitors WHERE"
										" firstName='" + first_name + "'"
										" AND lastName='" + last_name + "'"
										" AND registration IS NULL" );
							if(q.next())
								competitor_id = q.value(0).toInt();
						}
						*/
						if(competitor_id == 0) {
							q.execThrow("INSERT INTO competitors (registration) VALUES ('" + reg + "')");
							competitor_id = q.lastInsertId().toInt();
						}
						Q_ASSERT(competitor_id > 0);
						q.execThrow("UPDATE competitors SET"
									" firstName='" + first_name + "',"
									" lastName='" + last_name + "',"
									" registration='" + reg + "',"
									" siid=" + QString::number(si) + ","
									" importId=2"
									" WHERE id=" + QString::number(competitor_id)
									);
					}
					int run_id;
					{
						q.execThrow("SELECT id FROM runs WHERE"
									" relayId=" + QString::number(relay_id) + ""
									" AND leg=" + QString::number(leg));
						if(q.next()) {
							run_id = q.value(0).toInt();
						}
						else {
							q.execThrow("INSERT INTO runs (relayId) VALUES (" + QString::number(relay_id) + ")");
							run_id = q.lastInsertId().toInt();
						}
						q.execThrow("UPDATE runs SET"
									" competitorId=" + QString::number(competitor_id) + ","
									" stageId=1,"
									" relayId=" + QString::number(relay_id) + ","
									" leg=" + QString::number(leg) + ","
									" siid=" + QString::number(si) + ","
									" importId=2"
									" WHERE id=" + QString::number(run_id)
									);
					}
				}
			}
			q.execThrow("DELETE FROM runs WHERE importId=1");
			q.execThrow("DELETE FROM competitors WHERE importId=1");
			q.execThrow("DELETE FROM relays WHERE importId=1");
			transaction.commit();
			qf::gui::dialogs::MessageBox::showInfo(fwk, tr("Import finished successfully."));
			if(success_callback)
				success_callback();
		}
		catch (qf::core::Exception &e) {
			qf::gui::dialogs::MessageBox::showException(fwk, e);
		}
	});
}

void OrisImporter::chooseAndImport()
{
	qfLogFuncFrame();
	qf::gui::framework::MainWindow *fwk = qf::gui::framework::MainWindow::frameWork();
	ChooseOrisEventDialog dlg(this, fwk);
	if(!dlg.exec())
		return;
	int event_id = dlg.eventId();
	if(event_id > 0) {
		importEvent(event_id, [this]() {
			importRegistrations([this]() {
				importClubs([this]() {
					importMissingOneTimeClubs();
				});
			});
		});
	}
}

namespace {
QString jsonObjectToFullName(const QJsonObject &data, const QString &field_name)
{
	QJsonObject jso = data.value(field_name).toObject();
	return jso.value(QStringLiteral("FirstName")).toString() + ' ' + jso.value(QStringLiteral("LastName")).toString();
}
}

void OrisImporter::importEvent(int event_id, std::function<void ()> success_callback)
{
	QUrl url(QString("https://" + OrisImporter::orisDomainName() + "/API/?format=json&method=getEvent&id=%1").arg(event_id));
	getJsonAndProcess(url, this, [this, event_id, success_callback](const QJsonDocument &jsd) {
		qf::gui::framework::MainWindow *fwk = qf::gui::framework::MainWindow::frameWork();
		try {
			saveJsonBackup("Event", jsd);
			QJsonObject data = jsd.object().value(QStringLiteral("Data")).toObject();
			int stage_count = data.value(QStringLiteral("Stages")).toString().toInt();
			if(!stage_count)
				stage_count = 1;
			int sport_id = data.value(QStringLiteral("Sport")).toObject().value(QStringLiteral("ID")).toString().toInt();
			int di = data.value(QStringLiteral("Discipline")).toObject().value(QStringLiteral("ID")).toString().toInt();
			auto discipline_id_opt = Event::EventConfig::disciplineFromInt(di);//.value_or(Event::EventConfig::Discipline::Classic);
			if (!discipline_id_opt.has_value()) {
				qfError() << "OrisImporter: Unknown discipline ID:" << di;
			}
			auto discipline = discipline_id_opt.value_or(Event::EventConfig::Discipline::Classic);
			qfInfo() << "pocet etap:" << stage_count << "sport id:" << sport_id << "discipline id:" << di;
			QVariantMap ecfg;
			ecfg["stageCount"] = stage_count;
			ecfg["name"] = data.value(QStringLiteral("Name")).toString();
			ecfg["description"] = QString();
			ecfg["date"] = QDate::fromString(data.value(QStringLiteral("Date")).toString(), Qt::ISODate);
			ecfg["place"] = data.value(QStringLiteral("Place")).toString();
			ecfg["mainReferee"] = jsonObjectToFullName(data, QStringLiteral("MainReferee"));
			ecfg["director"] = jsonObjectToFullName(data, QStringLiteral("Director"));
			ecfg["sportId"] = sport_id;
			ecfg["disciplineId"] = static_cast<int>(discipline);
			ecfg["importId"] = event_id;
			ecfg["time"] = QTime::fromString(data.value(QStringLiteral("StartTime")).toString(), QStringLiteral("hh:mm"));
			if(!getPlugin<EventPlugin>()->createEvent(QString(), ecfg))
				return;

			bool is_relay = getPlugin<EventPlugin>()->eventConfig()->isRelays();
			//QString event_name = getPlugin<EventPlugin>()->eventName();
			qfLogScope("importEvent");
			qf::core::sql::Transaction transaction;

			int items_processed = 0;
			int items_count = 0;
			QJsonObject classes_o = data.value(QStringLiteral("Classes")).toObject();
			for(auto it = classes_o.constBegin(); it != classes_o.constEnd(); ++it) {
				items_count++;
			}
			Classes::ClassDocument doc;
			for(auto it = classes_o.constBegin(); it != classes_o.constEnd(); ++it) {
				QJsonObject class_o = it.value().toObject();
				int class_id = class_o.value(QStringLiteral("ID")).toString().toInt();
				QString class_name = class_o.value(QStringLiteral("Name")).toString();
				fwk->showProgress("Importing class: " + class_name, items_processed++, items_count);
				qfInfo() << "adding class id:" << class_id << "name:" << class_name;
				doc.loadForInsert();
				doc.setValue("id", class_id);
				doc.setValue("name", class_name);
				doc.save();
				if (is_relay) {
					int legs = class_o.value(QStringLiteral("Legs")).toString().toInt();
					qf::core::sql::Query q;
					q.execThrow("UPDATE classdefs SET"
								" relayLegCount=" + QString::number(legs) +
								" WHERE classId=" + QString::number(class_id) +
								" AND stageId=1"
								);
				}
			}
			transaction.commit();
			fwk->hideProgress();
		}
		catch (qf::core::Exception &e) {
			qf::gui::dialogs::MessageBox::showException(fwk, e);
		}
		syncEventEntries(event_id, success_callback);
	});
}

namespace {
class Run : public QVariantMap
{
	QF_VARIANTMAP_FIELD(int, s, setS, tageId)
	QF_VARIANTMAP_FIELD(int, s, setS, iId)
	QF_VARIANTMAP_FIELD(bool, is, set, Running)
	QF_VARIANTMAP_FIELD(bool, c, setC, ardLent)

public:
	Run(const QVariant &v = QVariant()) : QVariantMap(v.toMap()) {}
};
class Runs : public QVariantList
{
public:
	Runs(const QVariant &v = QVariant()) : QVariantList(v.toList()) {}

	Run runAtStage(int stage_no) const
	{
		return Run(value(stage_no - 1));
	}
	void setRunAtStage(int stage_no, const Run &run)
	{
		//stage_no--;
		while (count() < stage_no)
			append(Run());
		(*this)[stage_no-1] = run;
	}
	QString toString() const
	{
		QJsonDocument doc = QJsonDocument::fromVariant(*this);
		QByteArray ba = doc.toJson(QJsonDocument::Compact);
		return QString::fromUtf8(ba);
	}
	static Runs load(int competitor_id)
	{
		Runs ret;
		qf::core::sql::Query q;
		QStringList fields{QStringLiteral("stageId"), QStringLiteral("isRunning"), QStringLiteral("siId"), QStringLiteral("cardLent")};
		q.execThrow("SELECT " + fields.join(',') + " FROM runs"
					" WHERE competitorId=" QF_IARG(competitor_id)
					" ORDER BY stageId");
		while (q.next()) {
			Run run;
			for(const QString &fldname : fields)
				run[fldname] = q.value(fldname);
			ret << run;
		}
		return ret;
	}
};

const char KEY_IS_DATA_DIRTY[] = "isDataDirty";
const char KEY_RUNS[] = "runs";
const char KEY_ORIG_RUNS[] = "origRuns";
}

void OrisImporter::syncEventEntries(int event_id, std::function<void ()> success_callback)
{
	if(getPlugin<EventPlugin>()->eventConfig()->isRelays()) {
		syncRelaysEntries(event_id, success_callback);
		return;
	}
	QUrl url(QString("https://" + OrisImporter::orisDomainName() + "/API/?format=json&method=getEventEntries&eventid=%1").arg(event_id));
	getJsonAndProcess(url, this, [=](const QJsonDocument &jsd) {
		static const QString json_fn = "EventEntries";
		saveJsonBackup(json_fn, jsd);
		qf::gui::framework::MainWindow *fwk = qf::gui::framework::MainWindow::frameWork();
		try {
			int stage_cnt = getPlugin<EventPlugin>()->stageCount();
			qf::core::sql::Query q;
			QMap<int, QString> classes_map; // classes.id->classes.name
			q.exec("SELECT id, name FROM classes", qf::core::Exception::Throw);
			while(q.next()) {
				classes_map[q.value(0).toInt()] = q.value(1).toString();
			}
			QMap<int, int> imported_competitors; // importId->id
			q.exec("SELECT id, importId FROM competitors WHERE importId>0 ORDER BY importId", qf::core::Exception::Throw);
			while(q.next()) {
				imported_competitors[q.value(1).toInt()] = q.value(0).toInt();
			}
			QJsonDocument jsd2 = load_offline_json(json_fn);
			if(jsd2.isNull())
				jsd2 = jsd;
			QJsonObject data = jsd2.object().value(QStringLiteral("Data")).toObject();
			int items_processed = 0;
			int items_count = 0;
			for(auto it = data.constBegin(); it != data.constEnd(); ++it) {
				items_count++;
			}
			QList<Competitors::CompetitorDocument*> doc_lst;
			doc_lst.reserve(items_count);
			//QSet<int> used_idsi;
			for(auto it = data.constBegin(); it != data.constEnd(); ++it) {
				QJsonObject competitor_o = it.value().toObject();
				auto *doc = new Competitors::CompetitorDocument();
				doc_lst << doc;
				Runs runs;
				Runs orig_runs;
				int import_id = competitor_o.value(QStringLiteral("ID")).toString().toInt();
				int competitor_id = imported_competitors.value(import_id);
				if(competitor_id > 0) {
					doc->load(competitor_id);
					orig_runs = Runs::load(competitor_id);
					runs = orig_runs;
					imported_competitors.remove(import_id);
				}
				else {
					doc->loadForInsert();
				}
				{
					QJsonObject stages = competitor_o.value(QStringLiteral("Stages")).toObject();
					if(stages.isEmpty()) {
						for (int i = 0; i < stage_cnt; ++i) {
							Run run = runs.runAtStage(i+1);
							run.setRunning(true);
							runs.setRunAtStage(i+1, run);
						}
					}
					else {
						for (int i = 0; i < stage_cnt; ++i) {
							Run run = runs.runAtStage(i+1);
							QString key = QString("Stage%1").arg(i+1);
							run.setRunning(stages.value(key).toInt() == 1);
							runs.setRunAtStage(i+1, run);
						}
					}
				}
				QString siid_str = competitor_o.value(QStringLiteral("SI")).toString();
				bool ok;
				int siid = siid_str.toInt(&ok);
				//qfInfo() << "SI:" << siid, competitor_obj.ClassDesc, ' ', competitor_obj.LastName, ' ', competitor_obj.FirstName, "classId:", parseInt(competitor_obj.ClassID));
				QString note = competitor_o.value(QStringLiteral("Note")).toString();
				if(!ok && !siid_str.isEmpty()) {
					note += " SI:" + siid_str;
				}
				QString s = competitor_o.value(QStringLiteral("RentSI")).toString();
				bool rent_si = s.toInt() == 1;
				int orig_siid = doc->value("siId").toInt();
				for (int i = 0; i < stage_cnt; ++i) {
					Run run = runs.runAtStage(i+1);
					Run orig_run = orig_runs.runAtStage(i+1);
					if(orig_run.siId() == orig_siid || orig_run.siId() == 0)
						run.setSiId(siid);
					run.setCardLent(rent_si);
					runs.setRunAtStage(i+1, run);
				}
				QString requested_start = competitor_o.value(QStringLiteral("RequestedStart")).toString();
				if(!requested_start.isEmpty()) {
					note += " req. start: " + requested_start;
				}
				QString first_name = competitor_o.value(QStringLiteral("FirstName")).toString();
				QString last_name = competitor_o.value(QStringLiteral("LastName")).toString();
				if(first_name.isEmpty() && last_name.isEmpty()) {
					QString name = competitor_o.value(QStringLiteral("Name")).toString();
					if(!name.isEmpty()) {
						last_name = name.section(' ', 0, 0);
						first_name = name.section(' ', 1);
					}
				}
				QString reg_no = competitor_o.value(QStringLiteral("RegNo")).toString();
				QString iof_id = competitor_o.value(QStringLiteral("IOFID")).toString();
				fwk->showProgress("Importing: " + reg_no + ' ' + last_name + ' ' + first_name, items_processed, items_count);
				int class_id = competitor_o.value(QStringLiteral("ClassID")).toString().toInt();
				if(!classes_map.contains(class_id)) {
					qfWarning() << "class id:" << class_id << "not found in the class definitions";
					class_id = 0;
				}
				doc->setValue("classId", (class_id == 0)? QVariant(QMetaType(QMetaType::Type::Int)): QVariant(class_id));
				doc->setSiid(siid);
				doc->setValue("firstName", first_name);
				doc->setValue("lastName", last_name);
				doc->setValue("registration", reg_no);
				doc->setValue("iofId", !iof_id.isEmpty() ? iof_id : QVariant(QMetaType(QMetaType::Type::Int)));
				doc->setValue("licence", competitor_o.value(QStringLiteral("Licence")).toString());
				doc->setValue("note", note);
				doc->setValue("importId", import_id);
				doc->setProperty(KEY_RUNS, runs);
				doc->setProperty(KEY_ORIG_RUNS, orig_runs);
				items_processed++;
			}
			for(const auto &[import_id, id] : imported_competitors.asKeyValueRange()) {
				auto *doc = new Competitors::CompetitorDocument();
				doc_lst << doc;
				doc->load(id, Competitors::CompetitorDocument::ModeDelete);
			}
			static const QStringList fields = QStringList()
											  << QStringLiteral("classId")
											  << QStringLiteral("className")
											  << QStringLiteral("lastName")
											  << QStringLiteral("firstName")
											  << QStringLiteral("registration")
											  << QStringLiteral("iofId")
											  << QStringLiteral("siId")
											  << QStringLiteral("licence")
											  << KEY_RUNS
											  << QStringLiteral("note")
											  << QStringLiteral("importId");
			QVariantList new_entries_rows;
			QVariantList edited_entries_rows;
			QVariantList deleted_entries_rows;
			auto variant_to_string = [](const QVariant &v) {
				if(!v.isValid())
					return QStringLiteral("invalid");
				if(v.isNull())
					return QStringLiteral("null");
				return v.toString();
			};
			auto field_string = [classes_map, variant_to_string](Competitors::CompetitorDocument *doc, const QString fldn) {
				QString s;
				if(fldn == QLatin1String("className"))
					s = classes_map.value(doc->value(QStringLiteral("classId")).toInt());
				else if(fldn == QLatin1String(KEY_RUNS))
					s = Runs(doc->property(KEY_RUNS)).toString();
				else
					s = variant_to_string(doc->value(fldn));
				return s;
			};
			auto orig_field_string = [classes_map, variant_to_string](Competitors::CompetitorDocument *doc, const QString fldn) {
				QString s;
				if(fldn == QLatin1String("className"))
					s = classes_map.value(doc->origValue(QStringLiteral("classId")).toInt());
				else if(fldn == QLatin1String(KEY_RUNS))
					s = Runs(doc->property(KEY_ORIG_RUNS)).toString();
				else
					s = variant_to_string(doc->origValue(fldn));// + ":" + doc->origValue(fldn).typeName() + ":" + (doc->origValue(fldn).isNull()? "null": "");
				return s;
			};
			for(Competitors::CompetitorDocument *doc : doc_lst) {
				QVariantList tr = QVariantList() << QStringLiteral("tr");
				if(doc->mode() == Competitors::CompetitorDocument::ModeInsert) {
					doc->setProperty(KEY_IS_DATA_DIRTY, true);
					for(const auto &fldn : fields) {
						auto td = QVariantList() << QStringLiteral("td") << field_string(doc, fldn);
						tr.insert(tr.length(), td);
					}
					new_entries_rows.insert(new_entries_rows.length(), tr);
				}
				else if(doc->mode() == Competitors::CompetitorDocument::ModeEdit) {
					static QVariantMap green_attrs{{QStringLiteral("bgcolor"), QStringLiteral("khaki")}};
					for(const auto &fldn : fields) {
						bool is_dirty = false;
						if(fldn == QLatin1String(KEY_RUNS)) {
							Runs runs(doc->property(KEY_RUNS));
							Runs orig_runs(doc->property(KEY_ORIG_RUNS));
							is_dirty = !(runs == orig_runs);
						}
						else if(fldn == QLatin1String("className")) {
							is_dirty = doc->isDirty(QLatin1String("classId"));
						}
						else {
							is_dirty = doc->isDirty(fldn);
						}
						auto td = QVariantList() << QStringLiteral("td");
						if(is_dirty) {
							td << green_attrs;
							doc->setProperty(KEY_IS_DATA_DIRTY, true);
							td << orig_field_string(doc, fldn) + " -> " + field_string(doc, fldn);
						}
						else {
							td << field_string(doc, fldn);
						}
						tr.insert(tr.length(), td);
					}
					if(doc->property(KEY_IS_DATA_DIRTY).toBool())
						edited_entries_rows.insert(edited_entries_rows.length(), tr);
				}
				else if(doc->mode() == Competitors::CompetitorDocument::ModeDelete) {
					for(const QString &fldn : fields) {
						auto td = QVariantList() << QStringLiteral("td") << field_string(doc, fldn);
						tr.insert(tr.length(), td);
					}
					deleted_entries_rows.insert(deleted_entries_rows.length(), tr);
				}
			}
			QVariantList html_body = QVariantList() << QStringLiteral("body");
			html_body.insert(html_body.length(), QVariantList() << QStringLiteral("body"));
			html_body.insert(html_body.length(), qf::core::utils::HtmlUtils::createHtmlTable(tr("New entries"), fields, new_entries_rows));
			html_body.insert(html_body.length(), qf::core::utils::HtmlUtils::createHtmlTable(tr("Edited entries"), fields, edited_entries_rows));
			html_body.insert(html_body.length(), qf::core::utils::HtmlUtils::createHtmlTable(tr("Deleted entries"), fields, deleted_entries_rows));
			fwk->hideProgress();
			qf::core::utils::HtmlUtils::FromHtmlListOptions opts;
			opts.setDocumentTitle(tr("Oris import report"));
			const QString html = qf::core::utils::HtmlUtils::fromHtmlList(html_body, opts);
#ifdef Q_OS_LINUX_NNNNN
			QFile f("/tmp/1.html");
			if(f.open(QFile::WriteOnly)) {
				f.write(html.toUtf8());
			}
#endif
			qf::gui::dialogs::Dialog dlg(QDialogButtonBox::Save | QDialogButtonBox::Cancel, fwk);
			qf::gui::DialogButtonBox *bbx = dlg.buttonBox();
			auto *bt_no_drops = new QPushButton(tr("Save without drops"));
			auto no_drops = std::make_shared<bool>(false);
			connect(bt_no_drops, &QPushButton::clicked, [no_drops]() {
				*no_drops = true;
			});
			bbx->addButton(bt_no_drops, QDialogButtonBox::AcceptRole);

			auto *bt_export = new QPushButton(tr("Export"));
			connect(bt_export, &QPushButton::clicked, fwk, [fwk, html]() {
				QString fn = qf::gui::dialogs::FileDialog::getSaveFileName(fwk, tr("Export as ..."), "changes.html", tr("HTML files *.html (*.html)"));
				if(fn.isEmpty())
					return;
				if(!fn.endsWith(".html", Qt::CaseInsensitive))
					fn = fn + ".html";
				QFile f(fn);
				if(!f.open(QFile::WriteOnly)) {
					qf::gui::dialogs::MessageBox::showError(fwk, tr("Cannot open file '%1' for writing.").arg(f.fileName()));
					return;
				}
				f.write(html.toUtf8());
			});
			bbx->addButton(bt_export, QDialogButtonBox::ActionRole);

			auto *w = new qf::gui::HtmlViewWidget();
			dlg.setCentralWidget(w);
			w->setHtmlText(html);
			if(dlg.exec()) {
				qfLogScope("importEventEntries");
				qf::core::sql::Transaction transaction;
				for(Competitors::CompetitorDocument *doc : doc_lst) {
					doc->setEmitDbEventsOnSave(false);
					if(doc->mode() == Competitors::CompetitorDocument::ModeInsert || doc->mode() == Competitors::CompetitorDocument::ModeEdit) {
						if(doc->property(KEY_IS_DATA_DIRTY).toBool()) {
							doc->save();
							Runs runs(doc->property(KEY_RUNS));
							q.prepare("UPDATE runs"
									  " SET siId=:siId, isRunning=:isRunning, cardLent=:cardLent"
									  " WHERE competitorId=:competitorId AND stageId=:stageId", qf::core::Exception::Throw);
							for (int i = 0; i < stage_cnt; ++i) {
								int stage_id = i+1;
								Run run = runs.runAtStage(stage_id);
								q.bindValue(QStringLiteral(":siId"), run.siId());
								q.bindValue(QStringLiteral(":isRunning"), run.isRunning());
								q.bindValue(QStringLiteral(":cardLent"), run.cardLent());
								q.bindValue(QStringLiteral(":competitorId"), doc->dataId());
								q.bindValue(QStringLiteral(":stageId"), stage_id);
								q.exec(qf::core::Exception::Throw);
							}
						}
					}
					else if(doc->mode() == Competitors::CompetitorDocument::ModeDelete) {
						if(!*no_drops)
							doc->drop();
					}
				}
				transaction.commit();
			}
			qDeleteAll(doc_lst);
			getPlugin<EventPlugin>()->emitReloadDataRequest();
			getPlugin<EventPlugin>()->emitDbEvent(Event::EventPlugin::DBEVENT_COMPETITOR_COUNTS_CHANGED);
			if(success_callback)
				success_callback();
		}
		catch (qf::core::Exception &e) {
			qf::gui::dialogs::MessageBox::showException(fwk, e);
		}
	});
}

void OrisImporter::importRegistrations(std::function<void ()> success_callback)
{
	int sport_id = getPlugin<EventPlugin>()->eventConfig()->sportId();
	int year = QDate::currentDate().addMonths(-2).year();

	bool ok;
	year = QInputDialog::getInt(nullptr, tr("Import ORIS Registrations"), tr("Year of registration:"), year, year-2, year+2, 1, &ok);
	if (!ok) return;

	QUrl url(QString("https://" + OrisImporter::orisDomainName() + "/API/?format=json&method=getRegistration&sport=%1&year=%2").arg(sport_id).arg(year));
	getJsonAndProcess(url, this, [=](const QJsonDocument &jsd) {
		saveJsonBackup("Registrations", jsd);
		qf::gui::framework::MainWindow *fwk = qf::gui::framework::MainWindow::frameWork();
		QJsonObject data = jsd.object().value(QStringLiteral("Data")).toObject();
		// import clubs
		int items_processed = 0;
		int items_count = data.size();
		fwk->showProgress(tr("Importing registrations"), items_processed, items_count);
		try {
			qfLogScope("importRegistrations");
			qf::core::sql::Transaction transaction;
			qf::core::sql::Query q;
			q.exec("DELETE FROM registrations", qf::core::Exception::Throw);
			q.prepare("INSERT INTO registrations (firstName, lastName, registration, licence, clubAbbr, siId, importId) VALUES (:firstName, :lastName, :registration, :licence, :clubAbbr, :siId, :importId)", qf::core::Exception::Throw);
			for(auto it = data.constBegin(); it != data.constEnd(); ++it) {
				QJsonObject obj = it.value().toObject();
				//qfInfo() << obj.value("LastName") << obj.value("SI") << obj.value("UserID");
 				QString reg = obj.value(QStringLiteral("RegNo")).toString();
				if(items_processed % 100 == 0) {
					//Log.info(items_count, obj.RegNo);
					fwk->showProgress(reg, items_processed, items_count);
				}

				q.bindValue(":firstName", obj.value(QStringLiteral("FirstName")).toString());
				q.bindValue(":lastName", obj.value(QStringLiteral("LastName")).toString());
				//var name_search_key = File.toAscii7(obj.LastName + " " + obj.FirstName, true);
				//q.bindValue(":nameSearchKey", name_search_key);
				if(!reg.isEmpty()) {
					q.bindValue(":registration", reg);
					q.bindValue(":clubAbbr", reg.mid(0, 3));
				}
				q.bindValue(":licence", obj.value(QStringLiteral("Lic")).toString());
				q.bindValue(":siId", obj.value(QStringLiteral("SI")).toString().toInt());
				q.bindValue(":importId", obj.value(QStringLiteral("UserID")).toString().toInt());

				//q.bindValue(":abbr", obj.Abbr);
				//q.bindValue(":name", obj.Name);
				//q.bindValue(":importId", obj.ID);
				q.exec(qf::core::Exception::Throw);

				items_processed++;
			}
			transaction.commit();
			fwk->hideProgress();
			getPlugin<EventPlugin>()->emitDbEvent(EventPlugin::DBEVENT_REGISTRATIONS_IMPORTED, QVariant(), true);
			if(success_callback)
				success_callback();
		}
		catch (qf::core::Exception &e) {
			qf::gui::dialogs::MessageBox::showException(fwk, e);
		}
	});
}

void OrisImporter::importClubs(std::function<void ()> success_callback)
{
	QUrl url("https://" + OrisImporter::orisDomainName() + "/API/?format=json&method=getCSOSClubList");
	getJsonAndProcess(url, this, [=](const QJsonDocument &jsd) {
		saveJsonBackup("Clubs", jsd);
		qf::gui::framework::MainWindow *fwk = qf::gui::framework::MainWindow::frameWork();
		QJsonObject data = jsd.object().value(QStringLiteral("Data")).toObject();
		// import clubs
		int items_processed = 0;
		int items_count = data.size();
		fwk->showProgress(tr("Importing clubs"), items_processed, items_count);
		try {
			qfLogScope("importClubs");
			qf::core::sql::Transaction transaction;
			qf::core::sql::Query q;
			q.exec("DELETE FROM clubs WHERE importId IS NOT NULL", qf::core::Exception::Throw);
			q.prepare("INSERT INTO clubs (name, abbr, importId) VALUES (:name, :abbr, :importId)", qf::core::Exception::Throw);
			for(auto it = data.constBegin(); it != data.constEnd(); ++it) {
				QJsonObject obj = it.value().toObject();
				//Log.debug(JSON.stringify(obj, null, 2));
				QString abbr = obj.value(QStringLiteral("Abbr")).toString();
				QString name = obj.value(QStringLiteral("Name")).toString();
				fwk->showProgress(abbr + ' ' + name, items_processed, items_count);
				q.bindValue(":abbr", abbr);
				q.bindValue(":name", name);
				q.bindValue(":importId", obj.value(QStringLiteral("ID")).toString().toInt());
				q.exec(qf::core::Exception::Throw);

				items_processed++;
			}
			transaction.commit();
			fwk->hideProgress();
			if(success_callback)
				success_callback();
		}
		catch (qf::core::Exception &e) {
			qf::gui::dialogs::MessageBox::showException(fwk, e);
		}
	});
}

void OrisImporter::getAndImportClub(const QString &club, const QString &key)
{
	QUrl url(QString("https://" + OrisImporter::orisDomainName() + "/API/?format=json&method=getClub&id=%1&eventkey=%2").arg(club).arg(key));
	getJsonAndProcess(url, this, [club](const QJsonDocument &jsd) {
		saveJsonBackup(QString("Club_%1").arg(club), jsd);
		QJsonObject data = jsd.object().value(QStringLiteral("Data")).toObject();
		auto *fwk = qf::gui::framework::MainWindow::frameWork();
		try {
			qf::core::sql::Query q;
			q.prepare("INSERT INTO clubs (name, abbr, importId) VALUES (:name, :abbr, :importId)", qf::core::Exception::Throw);
			QString abbr = data.value(QStringLiteral("Abbr")).toString();
			QString name = data.value(QStringLiteral("Name")).toString();
			q.bindValue(":abbr", abbr);
			q.bindValue(":name", name);
			q.bindValue(":importId", data.value(QStringLiteral("ID")).toString().toInt());
			q.exec(qf::core::Exception::Throw);
		}
		catch (qf::core::Exception &e) {
			qf::gui::dialogs::MessageBox::showException(fwk, e);
		}
	});
}

void OrisImporter::importMissingOneTimeClubs()
{
	auto *fwk = qf::gui::framework::MainWindow::frameWork();
	auto event_key = getPlugin<EventPlugin>()->eventConfig()->orisEventKey();
	if (event_key.isEmpty()) {
		QMessageBox::warning(fwk,tr("Warning"),tr("For import one-time clubs, you need to fill ORIS Event Key in File->Event->Edit event"));
		return;
	}

	qf::core::sql::Query q;
	// list all entries, find missing clubs by registration and try to ask ORIS for names of this clubs
	std::map <QString, int> clubs; // map of existing clubs
	std::set <QString> missing_clubs;
	try {
		q.exec("SELECT id, abbr FROM clubs ORDER BY id", qf::core::Exception::Throw);
		while(q.next()) {
			clubs[q.value(1).toString()] = q.value(0).toInt();
		}

		// from competitors table
		q.exec("SELECT id, registration FROM competitors WHERE registration != '' ORDER BY id", qf::core::Exception::Throw);
		while(q.next()) {
			auto club = q.value(1).toString().mid(0,3);
			if (!club.isEmpty()) {
				auto it = clubs.find(club);
				if (it == clubs.end()) // club not found
					missing_clubs.insert(club);
			}
		}

		if (getPlugin<EventPlugin>()->eventConfig()->isRelays()) {
			// also from relays table
			q.exec("SELECT id, club FROM relays ORDER BY id", qf::core::Exception::Throw);
			while(q.next()) {
				auto club = q.value(1).toString();
				if (!club.isEmpty()) {
					auto it = clubs.find(club);
					if (it == clubs.end()) // club not found
						missing_clubs.insert(club);
				}
			}
		}

		if (missing_clubs.empty()) {
			QMessageBox::information(fwk,tr("Information"),tr("No missing one-time clubs found."));
			return;
		}

		int items_processed = 0;
		int items_count = missing_clubs.size();
		fwk->showProgress(tr("Importing one-time clubs"), 0, items_count);
		qfLogScope("importClubs");
		for (auto &abbr : missing_clubs) {
			getAndImportClub(abbr,event_key);
			items_processed++;
		}

		fwk->hideProgress();
		qfInfo() << "Import of"<< items_processed << "new one-time clubs started...";
	}
	catch (qf::core::Exception &e) {
		qf::gui::dialogs::MessageBox::showException(fwk, e);
	}
}


