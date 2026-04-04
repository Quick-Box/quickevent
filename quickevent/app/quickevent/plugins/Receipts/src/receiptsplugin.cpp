#include "receiptsplugin.h"
#include "receiptswidget.h"
#include "receiptsprinter.h"
#include "receiptssettings.h"
#include "receiptssettingspage.h"
#include "thirdparty/qrcodegen.hpp"
#include "../../Core/src/coreplugin.h"
#include "../../Core/src/widgets/settingsdialog.h"

#include <quickevent/core/si/readcard.h>
#include <quickevent/core/si/checkedcard.h>
#include <quickevent/core/og/timems.h>

#include <qf/core/utils/settings.h>
#include <qf/core/utils/treetable.h>
#include <qf/core/sql/query.h>
#include <qf/core/sql/querybuilder.h>
#include <qf/gui/model/sqltablemodel.h>

#include <qf/gui/framework/mainwindow.h>
#include <qf/gui/dialogs/dialog.h>
#include <qf/gui/reports/widgets/reportviewwidget.h>
#include <qf/gui/reports/processor/reportprocessor.h>
#include <qf/gui/reports/processor/reportitem.h>
#include <qf/gui/reports/processor/reportpainter.h>
#include <plugins/CardReader/src/cardreaderplugin.h>
#include <plugins/Event/src/eventplugin.h>
#include "partwidget.h"

#include <QDomDocument>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QBuffer>
#include <QImage>
#include <QPainter>
#include <QSaveFile>
#include <QSqlRecord>
#include <QPrinterInfo>
#include <QMessageBox>
#include <QStandardPaths>
#include <QUrl>
#include <QUrlQuery>

//#define QF_TIMESCOPE_ENABLED
#include <qf/core/utils/timescope.h>

namespace qfu = qf::core::utils;
namespace qff = qf::gui::framework;
using ::PartWidget;
using qff::getPlugin;
using Event::EventPlugin;
using CardReader::CardReaderPlugin;

namespace Receipts {

namespace {
QString eventConfigKey(const QString &suffix)
{
	const int current_stage = qMax(getPlugin<EventPlugin>()->currentStageId(), 1);
	return QStringLiteral("event.") + suffix + QStringLiteral(".E") + QString::number(current_stage);
}

QString configuredReceiptEventLinkUrl()
{
	return getPlugin<EventPlugin>()->eventConfig()->value(eventConfigKey(QStringLiteral("receiptEventLinkUrl"))).toString().trimmed();
}

QString defaultReceiptQrCodeCaption()
{
	return QStringLiteral("Live Results");
}

QString configuredReceiptQrCodeCaption()
{
	return getPlugin<EventPlugin>()->eventConfig()->value(eventConfigKey(QStringLiteral("receiptPrintEventQrCodeCaption")), defaultReceiptQrCodeCaption()).toString().trimmed();
}

QString receiptLinkWithCompetitorClass(const QString &base_url, const QString &class_name)
{
	const QString trimmed_base_url = base_url.trimmed();
	if(trimmed_base_url.isEmpty())
		return {};

	const QString trimmed_class_name = class_name.trimmed();
	if(trimmed_class_name.isEmpty())
		return trimmed_base_url;

	const QUrl url = QUrl::fromUserInput(trimmed_base_url);
	if(!url.isValid() || url.host().isEmpty())
		return trimmed_base_url;

	QUrl updated_url(url);
	QUrlQuery query(updated_url);
	query.removeAllQueryItems(QStringLiteral("class"));
	query.addQueryItem(QStringLiteral("class"), trimmed_class_name);
	updated_url.setQuery(query);
	return updated_url.toString();
}

bool printReceiptImageEnabled()
{
	return getPlugin<EventPlugin>()->eventConfig()->value(eventConfigKey(QStringLiteral("receiptPrintEventImage")), false).toBool();
}

bool printReceiptQrCodeEnabled()
{
	return getPlugin<EventPlugin>()->eventConfig()->value(eventConfigKey(QStringLiteral("receiptPrintEventQrCode")), false).toBool();
}

QString configuredReceiptImageBase64()
{
	return getPlugin<EventPlugin>()->eventConfig()->value(eventConfigKey(QStringLiteral("receiptImageDataBase64"))).toString();
}

QString configuredReceiptImageFormat()
{
	return getPlugin<EventPlugin>()->eventConfig()->value(eventConfigKey(QStringLiteral("receiptImageFormat")), QStringLiteral("png")).toString().trimmed().toLower();
}

int configuredReceiptImageHeightMm()
{
	bool ok = false;
	int image_height_mm = getPlugin<EventPlugin>()->eventConfig()->value(eventConfigKey(QStringLiteral("receiptImageHeightMm")), 18).toInt(&ok);
	if(!ok)
		return 18;
	if(image_height_mm < 10)
		return 10;
	if(image_height_mm > 60)
		return 60;
	return image_height_mm;
}

QString receiptImageCacheDirPath()
{
	return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/receipt-image-cache");
}

QString ensureReceiptImageFileImpl(const QString &image_base64, const QString &image_format)
{
	if(image_base64.isEmpty())
		return {};

	const QByteArray image_data = QByteArray::fromBase64(image_base64.toLatin1());
	if(image_data.isEmpty())
		return {};

	QString file_ext = image_format.trimmed().toLower();
	if(file_ext != QLatin1String("svg"))
		file_ext = QStringLiteral("png");

	const QString cache_dir_path = receiptImageCacheDirPath();
	QDir cache_dir(cache_dir_path);
	if(!cache_dir.mkpath(QStringLiteral(".")))
		return {};

	const QByteArray hash = QCryptographicHash::hash(image_data, QCryptographicHash::Sha1).toHex().left(16);
	const QString file_path = QDir::toNativeSeparators(cache_dir.filePath(QStringLiteral("receipt-image-%1.%2").arg(QString::fromLatin1(hash), file_ext)));
	bool write_file = true;
	{
		QFile existing_file(file_path);
		if(existing_file.exists() && existing_file.open(QIODevice::ReadOnly)) {
			if(existing_file.readAll() == image_data)
				write_file = false;
			existing_file.close();
		}
	}
	if(write_file) {
		QSaveFile file(file_path);
		if(!file.open(QIODevice::WriteOnly))
			return {};
		if(file.write(image_data) != image_data.size())
			return {};
		if(!file.commit())
			return {};
	}
	QFile stored_file(file_path);
	qfInfo() << "receipt image cache file:" << file_path << "exists:" << stored_file.exists() << "bytes:" << stored_file.size();
	return file_path;
}

QString ensureReceiptQrCodeFileImpl(const QString &link_url)
{
	const QString trimmed_link_url = link_url.trimmed();
	if(trimmed_link_url.isEmpty())
		return {};

	const QByteArray utf8_link = trimmed_link_url.toUtf8();
	qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText("", qrcodegen::QrCode::Ecc::LOW);
	try {
		qr = qrcodegen::QrCode::encodeText(utf8_link.constData(), qrcodegen::QrCode::Ecc::MEDIUM);
	}
	catch(const std::exception &e) {
		qfWarning() << "QR code generation failed:" << e.what();
		return {};
	}
	const int qr_size = qr.getSize();
	if(qr_size <= 0)
		return {};

	constexpr int border_modules = 4;
	constexpr int pixels_per_module = 8;
	const int image_size = (qr_size + border_modules * 2) * pixels_per_module;

	QImage qr_image(image_size, image_size, QImage::Format_RGB32);
	qr_image.fill(Qt::white);
	{
		QPainter painter(&qr_image);
		painter.setPen(Qt::NoPen);
		painter.setBrush(Qt::black);
		for(int y = 0; y < qr_size; ++y) {
			for(int x = 0; x < qr_size; ++x) {
				if(qr.getModule(x, y)) {
					painter.drawRect(
						(x + border_modules) * pixels_per_module,
						(y + border_modules) * pixels_per_module,
						pixels_per_module,
						pixels_per_module);
				}
			}
		}
	}

	QByteArray png_data;
	{
		QBuffer buffer(&png_data);
		if(!buffer.open(QIODevice::WriteOnly))
			return {};
		if(!qr_image.save(&buffer, "PNG"))
			return {};
	}
	if(png_data.isEmpty())
		return {};

	const QString cache_dir_path = receiptImageCacheDirPath();
	QDir cache_dir(cache_dir_path);
	if(!cache_dir.mkpath(QStringLiteral(".")))
		return {};

	const QByteArray hash = QCryptographicHash::hash(utf8_link, QCryptographicHash::Sha1).toHex().left(16);
	const QString file_path = QDir::toNativeSeparators(cache_dir.filePath(QStringLiteral("receipt-qr-%1.png").arg(QString::fromLatin1(hash))));
	bool write_file = true;
	{
		QFile existing_file(file_path);
		if(existing_file.exists() && existing_file.open(QIODevice::ReadOnly)) {
			if(existing_file.readAll() == png_data)
				write_file = false;
			existing_file.close();
		}
	}
	if(write_file) {
		QSaveFile file(file_path);
		if(!file.open(QIODevice::WriteOnly))
			return {};
		if(file.write(png_data) != png_data.size())
			return {};
		if(!file.commit())
			return {};
	}
	QFile stored_file(file_path);
	qfInfo() << "receipt QR cache file:" << file_path << "exists:" << stored_file.exists() << "bytes:" << stored_file.size();
	return file_path;
}

void setReceiptMediaData(qf::core::utils::TreeTable &tt, const QString &competitor_class_name = {})
{
	tt.setValue("event.receiptImageHeightMm", configuredReceiptImageHeightMm());
	if(printReceiptImageEnabled()) {
		const QString image_base64 = configuredReceiptImageBase64();
		const QString image_format = configuredReceiptImageFormat();
		// Reports consume a temporary file path, while the service persists the cached image payload in config.
		tt.setValue("event.receiptImagePath", ensureReceiptImageFileImpl(image_base64, image_format));
		tt.setValue("event.receiptImageDataBase64", image_base64);
		tt.setValue("event.receiptImageFormat", image_format);
	}
	else {
		tt.setValue("event.receiptImagePath", QString());
		tt.setValue("event.receiptImageDataBase64", QString());
		tt.setValue("event.receiptImageFormat", QString());
	}

	if(printReceiptQrCodeEnabled()) {
		const QString receipt_link = receiptLinkWithCompetitorClass(configuredReceiptEventLinkUrl(), competitor_class_name);
		tt.setValue("event.receiptQrCodeUrl", receipt_link);
		tt.setValue("event.receiptQrCodePath", ensureReceiptQrCodeFileImpl(receipt_link));
		tt.setValue("event.receiptQrCodeCaption", configuredReceiptQrCodeCaption());
	}
	else {
		tt.setValue("event.receiptQrCodeUrl", QString());
		tt.setValue("event.receiptQrCodePath", QString());
		tt.setValue("event.receiptQrCodeCaption", QString());
	}
}
}

QString ReceiptsPlugin::ensureReceiptImageFile(const QString &image_base64, const QString &image_format)
{
	return ensureReceiptImageFileImpl(image_base64, image_format);
}

QString ReceiptsPlugin::ensureReceiptQrCodeFile(const QString &link_url)
{
	return ensureReceiptQrCodeFileImpl(link_url);
}

ReceiptsPlugin::ReceiptsPlugin(QObject *parent)
	: Super("Receipts", parent)
{
	connect(this, &ReceiptsPlugin::installed, this, &ReceiptsPlugin::onInstalled);
}

void ReceiptsPlugin::onInstalled()
{
	qff::initPluginWidget<ReceiptsWidget, PartWidget>(tr("Receipts"), featureId());
	auto core_plugin = qf::gui::framework::getPlugin<Core::CorePlugin>();
	core_plugin->settingsDialog()->addPage(new ReceiptsSettingsPage());
}

ReceiptsPrinter *ReceiptsPlugin::receiptsPrinter()
{
	if(!m_receiptsPrinter) {
		m_receiptsPrinter = new ReceiptsPrinter(this);
	}
	return m_receiptsPrinter;
}

QVariantMap ReceiptsPlugin::readCardTablesData(int card_id)
{
	qfLogFuncFrame() << card_id;
	QVariantMap ret;
	quickevent::core::si::ReadCard read_card = getPlugin<CardReaderPlugin>()->readCard(card_id);
	{
		qfu::TreeTable tt;
		tt.appendColumn("position", QMetaType(QMetaType::Int));
		tt.appendColumn("code", QMetaType(QMetaType::Int));
		tt.appendColumn("punchTimeMs", QMetaType(QMetaType::Int));
		tt.appendColumn("stpTimeMs", QMetaType(QMetaType::Int));
		tt.appendColumn("lapTimeMs", QMetaType(QMetaType::Int));
 		QMapIterator<QString, QVariant> it(read_card);
		while(it.hasNext()) {
			it.next();
			if(it.key() != QLatin1String("punches")) {
				// qfInfo() << card_id << it.key() << "-->" << it.value().toString();
				tt.setValue(it.key(), it.value());
			}
		}
		int position = 0;
		int start_time_ms = read_card.startTime();
		if(start_time_ms == 0xeeee)
			start_time_ms = read_card.checkTime();
		start_time_ms *= 1000;
		int prev_stp_time_ms = 0;
		for(const auto &v : read_card.punches()) {
			quickevent::core::si::ReadPunch punch(v.toMap());
			int punch_time_ms = punch.time() * 1000 + punch.msec();
			int stp_time_ms = quickevent::core::og::TimeMs::msecIntervalAM(start_time_ms, punch_time_ms);
			int ix = tt.appendRow();
			qf::core::utils::TreeTableRow tt_row = tt.row(ix);
			++position;
			int code = punch.code();
			tt_row.setValue("position", position);
			tt_row.setValue("code", code);
			tt_row.setValue("punchTimeMs", punch_time_ms);
			tt_row.setValue("stpTimeMs", stp_time_ms);
			tt_row.setValue("lapTimeMs", stp_time_ms - prev_stp_time_ms);
			prev_stp_time_ms = stp_time_ms;
			tt.setRow(ix, tt_row);
		}
		{
			int ix = tt.appendRow();
			//int code = punch.code();
			//ttr.setValue("position", position);
			//ttr.setValue("code", code);
			int punch_time_ms = read_card.finishTime() * 1000 + read_card.finishTimeMs();
			int stp_time_ms = quickevent::core::og::TimeMs::msecIntervalAM(start_time_ms, punch_time_ms);
			qf::core::utils::TreeTableRow tt_row = tt.row(ix);
			tt_row.setValue(QStringLiteral("punchTimeMs"), punch_time_ms);
			tt_row.setValue(QStringLiteral("stpTimeMs"), stp_time_ms);
			tt_row.setValue(QStringLiteral("lapTimeMs"), stp_time_ms - prev_stp_time_ms);
			tt.setRow(ix, tt_row);
		}
		{
			qf::core::sql::QueryBuilder qb;
			qb.select2("config", "ckey, cvalue, ctype")
					.from("config")
					.where("ckey LIKE 'event.%'");
			qf::core::sql::Query q;
			q.exec(qb.toString());
			while(q.next()) {
				QVariant v = qf::core::Utils::retypeStringValue(q.value("cvalue").toString(), q.value("ctype").toString());
				tt.setValue(q.value("ckey").toString(), v);
			}
		}
		tt.setValue("stageCount", getPlugin<EventPlugin>()->stageCount());
		tt.setValue("currentStageId", getPlugin<EventPlugin>()->currentStageId());
		qfDebug() << "card:\n" << tt.toString();
		ret["card"] = tt.toVariant();
	}
	return ret;
}

QVariantMap ReceiptsPlugin::receiptTablesData(int card_id)
{
	qfLogFuncFrame() << card_id;
	QF_TIME_SCOPE("receiptTablesData()");
	bool is_relays = getPlugin<EventPlugin>()->eventConfig()->isRelays();
	QVariantMap ret;
	quickevent::core::si::ReadCard read_card = getPlugin<CardReaderPlugin>()->readCard(card_id);
	quickevent::core::si::CheckedCard checked_card = getPlugin<CardReaderPlugin>()->checkCard(read_card);
	int run_id = checked_card.runId();
	bool is_card_lent = getPlugin<CardReaderPlugin>()->isCardLent(read_card.cardNumber(), read_card.finishTime(), run_id);
	int stage_id = 0;
	int course_id = checked_card.courseId();
	int leg = 0;
	int relay_num = 0;
	int current_standings = 1;
	int competitors_finished = 0;
	int best_time = 0;
	QMap<int, int> best_laps; // position->time
	QMap<int, int> lap_stand; // position->standing in lap
	QMap<int, int> lap_stand_cummulative;  // position->cummulative standing after lap
	QString competitor_class_name;
	{
		qf::gui::model::SqlTableModel model;
		qf::core::sql::QueryBuilder qb;
		qb.select2("competitors", "*")
				.select2("runs", "*")
				.select2("classes", "name")
				.from("runs")
				.join("runs.competitorId", "competitors.id")
				.where("runs.id=" QF_IARG(run_id));
		if(is_relays) {
			qb.select2("relays", "*")
					.select("COALESCE(competitors.lastName, '') || ' '"
						" || COALESCE(competitors.firstName, '') || ' ('"
						" || COALESCE(relays.club, '') || ' '"
						" || COALESCE(relays.name, '') || ')' AS competitorName")
					.join("runs.relayId", "relays.id")
					.join("relays.classId", "classes.id");
		}
		else {
			qb.select("COALESCE(competitors.lastName, '') || ' ' || COALESCE(competitors.firstName, '') AS competitorName")
				.join("competitors.classId", "classes.id");
		}
		model.setQuery(qb.toString());
		model.reload();
		if(model.rowCount() == 1) {
			competitor_class_name = model.value(0, QStringLiteral("classes.name")).toString().trimmed();
			stage_id = model.value(0, "runs.stageId").toInt();
			qf::core::sql::Query run_laps;
			run_laps.execThrow("SELECT runlaps.position, runlaps.code, runlaps.lapTimeMs FROM runlaps WHERE runId = " QF_IARG(run_id) " ORDER BY position");
			while(run_laps.next()) {
				int pos = run_laps.value("position").toInt();
				int lapTimeMs = run_laps.value("lapTimeMs").toInt();
				{
					// best lap times and position/place/standing in lap
					qf::core::sql::QueryBuilder qb_laps;
					qb_laps.select("MIN(runlaps.lapTimeMs) AS minLapTimeMs, SUM(CASE WHEN runlaps.lapTimeMs < " QF_IARG(lapTimeMs) " THEN 1 ELSE 0 END) as standingLap");
					if(is_relays) {
						int leg = model.value(0, "runs.leg").toInt();
						int class_id = model.value(0, "relays.classId").toInt();
						qb_laps.from("relays")
							.join("relays.id", "runs.relayId", qf::core::sql::QueryBuilder::INNER_JOIN)
							.where("relays.classId=" QF_IARG(class_id)
								" AND runs.leg=" QF_IARG(leg));
					}
					else {
						int class_id = model.value(0, "competitors.classId").toInt();
						qb_laps.from("competitors")
							.join("competitors.id", "runs.competitorId",qf::core::sql::QueryBuilder::INNER_JOIN)
							.where("runs.stageId=" QF_IARG(stage_id)
								" AND competitors.classId=" QF_IARG(class_id));


					}
					qb_laps.join("runs.id", "runlaps.runId", qf::core::sql::QueryBuilder::INNER_JOIN)
						.where("runlaps.position > 0"
							" AND runlaps.lapTimeMs > 0"
							" AND runlaps.position=" QF_IARG(pos)
							" AND NOT runs.disqualified"
							" AND runs.isRunning"
							" AND NOT runs.misPunch")
						.groupBy("runlaps.position");
					qf::core::sql::Query q_laps;
					q_laps.execThrow(qb_laps.toString());
					if (q_laps.next()) {
						best_laps[pos] = q_laps.value("minLapTimeMs").toInt();
						lap_stand[pos] = q_laps.value("standingLap").toInt() + 1;
					}
				}
				{
					// current position/place/standing after lap
					qf::core::sql::Query q_run_curr_time;
					q_run_curr_time.execThrow("SELECT SUM(lapTimeMs) as currTime FROM runlaps WHERE position <= " QF_IARG(pos) " AND runId = " QF_IARG(run_id) " GROUP BY runId" );
					q_run_curr_time.next();
					int run_curr_time = q_run_curr_time.value("currTime").toInt();

					qf::core::sql::QueryBuilder qb_curr_times;
					qb_curr_times.select("runs.id, SUM(runlaps.lapTimeMs) as currTime");
					if(is_relays) {
						int leg = model.value(0, "runs.leg").toInt();
						int class_id = model.value(0, "relays.classId").toInt();
						qb_curr_times.from("relays")
							.join("relays.id", "runs.relayId", qf::core::sql::QueryBuilder::INNER_JOIN)
							.where("relays.classId=" QF_IARG(class_id)
								" AND runs.leg=" QF_IARG(leg));
					}
					else {
						int class_id = model.value(0, "competitors.classId").toInt();
						qb_curr_times.from("competitors")
							.join("competitors.id", "runs.competitorId",qf::core::sql::QueryBuilder::INNER_JOIN)
							.where("runs.stageId=" QF_IARG(stage_id)
								" AND competitors.classId=" QF_IARG(class_id));


					}
					qb_curr_times.join("runs.id", "runlaps.runId", qf::core::sql::QueryBuilder::INNER_JOIN)
						.where("runlaps.position > 0"
							" AND runlaps.lapTimeMs > 0"
							" AND runlaps.position<=" QF_IARG(pos)
							" AND NOT runs.disqualified"
							" AND runs.isRunning"
							" AND NOT runs.misPunch")
						.groupBy("runs.id");
					qf::core::sql::Query q_curr_time;
					q_curr_time.execThrow(
								"SELECT COUNT(*) as currStanding"
								" FROM (" + qb_curr_times.toString() + ") AS foo"
								" WHERE currTime < " QF_IARG(run_curr_time));
					if (q_curr_time.next()) {
						lap_stand_cummulative[pos] = q_curr_time.value("currStanding").toInt() + 1;
					}
				}
			}
			if(is_relays) {
				leg = model.value(0, "runs.leg").toInt();
				relay_num = model.value(0, "relays.number").toInt();
			}
		}
		if(checked_card.isOk()) {
			// find current standings
			qf::core::sql::QueryBuilder qb;
			if(is_relays) {
				int leg = model.value(0, "runs.leg").toInt();
				int class_id = model.value(0, "relays.classId").toInt();
				qb.select2("runs", "timeMs")
						.select("runs.disqualified OR NOT runs.isRunning OR runs.misPunch AS dis")
						.from("relays")
						.joinRestricted("relays.id", "runs.relayId",
										"relays.classId=" QF_IARG(class_id)
										" AND runs.finishTimeMs > 0"
										" AND runs.leg=" QF_IARG(leg),
										qf::core::sql::QueryBuilder::INNER_JOIN)
						.orderBy("misPunch, disqualified, relays.isRunning, runs.isRunning, runs.timeMs");
			}
			else {
				int class_id = model.value(0, "competitors.classId").toInt();
				qb.select2("runs", "timeMs")
						.select("runs.disqualified OR NOT runs.isRunning OR runs.misPunch AS dis")
						.from("competitors")
						.joinRestricted("competitors.id", "runs.competitorId", "runs.stageId=" QF_IARG(stage_id) " AND competitors.classId=" QF_IARG(class_id), qf::core::sql::QueryBuilder::INNER_JOIN)
						.where("runs.finishTimeMs > 0")
						.orderBy("misPunch, disqualified, runs.isRunning, runs.timeMs");
			}
			//qfInfo() << qb.toString();
			auto q = qf::core::sql::Query::fromExec(qb.toString());
			while (q.next()) {
				bool dis = q.value("dis").toBool();
				int time = q.value("timeMs").toInt();
				if(!dis) {
					if(time < checked_card.timeMs())
						current_standings++;
					if(best_time == 0 || time < best_time)
						best_time = time;
				}
				competitors_finished++;
			}
		}
		qfu::TreeTable tt = model.toTreeTable();
		{
			qf::core::sql::QueryBuilder qb;
			qb.select2("courses", "length, climb")
					.select("(SELECT COUNT(*) FROM coursecodes WHERE courseId=courses.id) AS controlCount")
					.from("courses")
					.where("courses.id=" QF_IARG(course_id));
			qf::core::sql::Query q;
			q.exec(qb.toString());
			if(q.next()) {
				QSqlRecord rec = q.record();
				for (int i = 0; i < rec.count(); ++i) {
					QString fld_name = rec.fieldName(i);
					tt.setValue(fld_name, rec.value(i));
				}
			}
		}
		{
			qf::core::sql::QueryBuilder qb;
			qb.select2("config", "ckey, cvalue, ctype")
					.from("config")
					.where("ckey LIKE 'event.%'");
			qf::core::sql::Query q;
			q.exec(qb.toString());
			while(q.next()) {
				QVariant v = qf::core::Utils::retypeStringValue(q.value("cvalue").toString(), q.value("ctype").toString());
				tt.setValue(q.value("ckey").toString(), v);
			}
		}
		tt.setValue("appVersion", QCoreApplication::applicationVersion());
		tt.setValue("stageCount", getPlugin<EventPlugin>()->stageCount());
		tt.setValue("currentStageId", stage_id);
		setReceiptMediaData(tt, competitor_class_name);
		qfDebug() << "competitor:\n" << tt.toString();
		ret["competitor"] = tt.toVariant();
	}
	{
		qfu::TreeTable tt;
		tt.appendColumn("position", QMetaType(QMetaType::Int));
		tt.appendColumn("code", QMetaType(QMetaType::Int));
		tt.appendColumn("stpTimeMs", QMetaType(QMetaType::Int));
		tt.appendColumn("lapTimeMs", QMetaType(QMetaType::Int));
		tt.appendColumn("standLap", QMetaType(QMetaType::Int));
		tt.appendColumn("standCummulative", QMetaType(QMetaType::Int));
		tt.appendColumn("lossMs", QMetaType(QMetaType::Int));
		tt.appendColumn("distance", QMetaType(QMetaType::Int));
		QMapIterator<QString, QVariant> it(checked_card);
		while(it.hasNext()) {
			it.next();
			if(it.key() != QLatin1String("punches"))
				tt.setValue(it.key(), it.value());
		}
		tt.setValue("isOk", checked_card.isOk());
		int position = 0;
		for(const auto &v : checked_card.punches()) {
			quickevent::core::si::CheckedPunch punch(v.toMap());
			int ix = tt.appendRow();
			qf::core::utils::TreeTableRow tt_row = tt.row(ix);
			++position;
			int code = punch.code();
			tt_row.setValue("position", position);
			tt_row.setValue("code", code);
			tt_row.setValue("stpTimeMs", punch.stpTimeMs());
			tt_row.setValue("standLap", lap_stand[position]);
			tt_row.setValue("standCummulative", lap_stand_cummulative[position]);
			int lap = punch.lapTimeMs();
			tt_row.setValue("lapTimeMs", lap);
			int best_lap = best_laps.value(position);
			if(lap > 0 && best_lap > 0) {
				int loss = lap - best_lap;
				tt_row.setValue("lossMs", loss);
			}
			tt_row.setValue("distance", punch.distance());
			tt.setRow(ix, tt_row);
		}
		{
			QSet<int> correct_codes;
			for (int i = 0; i < checked_card.punchCount(); ++i) {
				correct_codes << checked_card.punchAt(i).code();
			}
			QVariantList xc;
			for (int i = 0; i < read_card.punchCount(); ++i) {
				int code = read_card.punchAt(i).code();
				if(!correct_codes.contains(code)) {
					xc.insert(xc.count(), QVariantList() << (i+1) << code);
				}
			}
			tt.setValue("extraCodes", xc);
		}
		tt.setValue("currentStandings", current_standings);
		tt.setValue("competitorsFinished", competitors_finished);
		tt.setValue("timeMs", checked_card.timeMs());
		tt.setValue("bestTime", best_time);
		tt.setValue("isCardLent", is_card_lent);

		tt.setValue("isRelay", is_relays);
		tt.setValue("leg", leg);
		tt.setValue("relayNumber", relay_num);
		{
			qf::core::sql::QueryBuilder qb;
			qb.select2("config", "ckey, cvalue, ctype")
				.from("config")
				.where("ckey LIKE 'event.%'");
			qf::core::sql::Query q;
			q.exec(qb.toString());
			while(q.next()) {
				QVariant v = qf::core::Utils::retypeStringValue(q.value("cvalue").toString(), q.value("ctype").toString());
				tt.setValue(q.value("ckey").toString(), v);
			}
		}
		tt.setValue("appVersion", QCoreApplication::applicationVersion());
		tt.setValue("stageCount", getPlugin<EventPlugin>()->stageCount());
		tt.setValue("currentStageId", stage_id);
		setReceiptMediaData(tt, competitor_class_name);

		qfDebug() << "card:\n" << tt.toString();
		ret["card"] = tt.toVariant();
	}
	return ret;
}

void ReceiptsPlugin::previewCard(int card_id)
{
	qfLogFuncFrame() << "card id:" << card_id;
	//qfInfo() << "previewReceipe_classic, card id:" << card_id;
	auto *w = new qf::gui::reports::ReportViewWidget();
	w->setPersistentSettingsId("cardPreview");
	w->setWindowTitle(tr("Card"));
	w->setReport(findReportFile("sicard.qml"));
	QVariantMap dt = readCardTablesData(card_id);
	for(const auto &[k, v] : dt.asKeyValueRange()) {
		w->setTableData(k, v);
	}
	qff::MainWindow *fwk = qff::MainWindow::frameWork();
	qf::gui::dialogs::Dialog dlg(fwk);
	dlg.setCentralWidget(w);
	dlg.exec();
}

bool ReceiptsPlugin::printCard(int card_id)
{
	qfLogFuncFrame() << "card id:" << card_id;
	QF_TIME_SCOPE("ReceiptsPlugin::printCard()");
	try {
		QVariantMap dt = readCardTablesData(card_id);
		return receiptsPrinter()->printReceipt("sicard.qml", dt, card_id);
	}
	catch(const qf::core::Exception &e) {
		qfError() << e.toString();
	}
	return false;
}

bool ReceiptsPlugin::printTestReceipt(const QString &report_file_name, const QVariantMap &report_data)
{
	qfLogFuncFrame() << report_file_name;
	QF_TIME_SCOPE("ReceiptsPlugin::printTestReceipt()");
	if(report_file_name.isEmpty()) {
		qfError() << "Empty test receipt path.";
		return false;
	}
	try {
		return receiptsPrinter()->printReceipt(report_file_name, report_data, 0);
	}
	catch(const qf::core::Exception &e) {
		qfError() << e.toString();
	}
	return false;
}

void ReceiptsPlugin::previewError(int card_id, QString error_qml)
{
	qfLogFuncFrame() << "card id:" << card_id;
	auto *w = new qf::gui::reports::ReportViewWidget();
	w->setPersistentSettingsId("errorPreview");
	w->setWindowTitle(tr("Error"));
	w->setReport(findReportFile(error_qml));
	QVariantMap dt = readCardTablesData(card_id);
	for(const auto &[k, v] : dt.asKeyValueRange()) {
		w->setTableData(k, v);
	}
	qff::MainWindow *fwk = qff::MainWindow::frameWork();
	qf::gui::dialogs::Dialog dlg(fwk);
	dlg.setCentralWidget(w);
	dlg.exec();
}

bool ReceiptsPlugin::printError(int card_id, QString error_qml)
{
	qfLogFuncFrame() << "card id:" << card_id;
	QF_TIME_SCOPE("ReceiptsPlugin::printError()");
	try {
		QVariantMap dt = readCardTablesData(card_id);
		return receiptsPrinter()->printReceipt(error_qml, dt, card_id);
	}
		catch(const qf::core::Exception &e) {
		qfError() << e.toString();
	}
	return false;
}

void ReceiptsPlugin::previewReceipt(int card_id)
{
	qfLogFuncFrame() << "card id:" << card_id;
	//qfInfo() << "previewReceipe_classic, card id:" << card_id;
	ReceiptsSettings settings;
	auto *w = new qf::gui::reports::ReportViewWidget();
	if (settings.receiptPath().isEmpty()) {
		auto fwk = qff::MainWindow::frameWork();
		QMessageBox::warning(fwk,tr("Warning"),tr("Receipt report type is not defined.\nPlease go to Settings->Receipts and set receipt type."));
		return;
	}
	w->setPersistentSettingsId("cardPreview");
	w->setWindowTitle(tr("Receipt"));
	w->setReport(findReportFile(settings.receiptPath()));
	QVariantMap dt = receiptTablesData(card_id);
	for(const auto &[k, v] : dt.asKeyValueRange()) {
		w->setTableData(k, v);
	}
	auto fwk = qff::MainWindow::frameWork();
	qf::gui::dialogs::Dialog dlg(fwk);
	dlg.setCentralWidget(w);
	dlg.exec();
}

bool ReceiptsPlugin::printReceipt(int card_id)
{
	qfLogFuncFrame() << "card id:" << card_id;
	QF_TIME_SCOPE("ReceiptsPlugin::printReceipt()");
	try {
		ReceiptsSettings settings;
		if (settings.receiptPath().isEmpty()) {
			auto fwk = qff::MainWindow::frameWork();
			QMessageBox::warning(fwk,tr("Warning"),tr("Receipt report type is not defined.\nPlease go to Settings->Receipts and set receipt type."));
			return false;
		}
		QVariantMap dt = receiptTablesData(card_id);
		return receiptsPrinter()->printReceipt(settings.receiptPath(), dt, card_id);
	}
	catch(const qf::core::Exception &e) {
		qfError() << e.toString();
	}
	return false;
}

bool ReceiptsPlugin::isAutoPrintEnabled()
{
	return qff::MainWindow::frameWork()->findChild<ReceiptsWidget *>("ReceiptsWidget")->isAutoPrintEnabled();
}

void ReceiptsPlugin::printOnAutoPrintEnabled(int card_id)
{
	if(isAutoPrintEnabled())
		printReceipt(card_id);
}


}
