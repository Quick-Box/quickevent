#include "receiptssettingspage.h"
#include "ui_receiptssettingspage.h"
#include "receiptsplugin.h"
#include "receiptssettings.h"
#include "receiptsprinteroptionsdialog.h"

#include <plugins/Event/src/eventplugin.h>

#include <qf/core/sql/query.h>
#include <qf/core/utils.h>
#include <qf/gui/framework/mainwindow.h>
#include <qf/core/utils/treetable.h>
#include <qf/core/log.h>
#include <qf/gui/style.h>

#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QImageReader>
#include <QMessageBox>
#include <QSqlRecord>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

namespace Receipts {
namespace {
QString storedReceiptImageLabel(const QString &format)
{
	const QString normalized_format = format.trimmed().isEmpty() ? QStringLiteral("PNG") : format.trimmed().toUpper();
	return QObject::tr("Stored image (%1)").arg(normalized_format);
}

QString defaultReceiptQrCodeCaption()
{
	return QStringLiteral("Live Results");
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

void appendColumnsFromValues(qf::core::utils::TreeTable &tt, const QVariantMap &values)
{
	for(auto it = values.cbegin(); it != values.cend(); ++it) {
		const int type_id = it.value().isValid() ? it.value().userType() : QMetaType::QString;
		tt.appendColumn(it.key(), QMetaType(type_id));
	}
}

void setRowValues(qf::core::utils::TreeTable &tt, int row_ix, const QVariantMap &values)
{
	auto tt_row = tt.row(row_ix);
	for(auto it = values.cbegin(); it != values.cend(); ++it)
		tt_row.setValue(it.key(), it.value());
	tt.setRow(row_ix, tt_row);
}

void applyEventConfigValues(qf::core::utils::TreeTable &tt)
{
	qf::core::sql::Query q;
	q.exec(QStringLiteral("SELECT ckey, cvalue, ctype FROM config WHERE ckey LIKE 'event.%'"));
	while(q.next()) {
		QVariant v = qf::core::Utils::retypeStringValue(q.value(QStringLiteral("cvalue")).toString(), q.value(QStringLiteral("ctype")).toString());
		tt.setValue(q.value(QStringLiteral("ckey")).toString(), v);
	}
}

void applyCurrentReceiptMedia(
	qf::core::utils::TreeTable &tt,
	const QString &competitor_class_name,
	bool print_logo,
	const QString &image_base64,
	const QString &image_format,
	int image_height_mm,
	bool print_qr_code,
	const QString &qr_url,
	const QString &qr_caption)
{
	tt.setValue(QStringLiteral("event.receiptImageHeightMm"), image_height_mm);
	if(print_logo) {
		tt.setValue(QStringLiteral("event.receiptImagePath"), ReceiptsPlugin::ensureReceiptImageFile(image_base64, image_format));
		tt.setValue(QStringLiteral("event.receiptImageDataBase64"), image_base64);
		tt.setValue(QStringLiteral("event.receiptImageFormat"), image_format);
	}
	else {
		tt.setValue(QStringLiteral("event.receiptImagePath"), QString());
		tt.setValue(QStringLiteral("event.receiptImageDataBase64"), QString());
		tt.setValue(QStringLiteral("event.receiptImageFormat"), QString());
	}

	if(print_qr_code) {
		const QString receipt_link = receiptLinkWithCompetitorClass(qr_url, competitor_class_name);
		const QString effective_caption = qr_caption.trimmed().isEmpty() ? defaultReceiptQrCodeCaption() : qr_caption.trimmed();
		tt.setValue(QStringLiteral("event.receiptQrCodeUrl"), receipt_link);
		tt.setValue(QStringLiteral("event.receiptQrCodePath"), ReceiptsPlugin::ensureReceiptQrCodeFile(receipt_link));
		tt.setValue(QStringLiteral("event.receiptQrCodeCaption"), effective_caption);
	}
	else {
		tt.setValue(QStringLiteral("event.receiptQrCodeUrl"), QString());
		tt.setValue(QStringLiteral("event.receiptQrCodePath"), QString());
		tt.setValue(QStringLiteral("event.receiptQrCodeCaption"), QString());
	}
}

struct DummyReceiptSeed
{
	QVariantMap competitorRow;
	QString competitorClassName;
	int classId = 0;
	int courseId = 0;
	int courseLength = 4500;
	int courseClimb = 120;
	int startTimeMin = 30;
	int startIntervalMin = 2;
	int competitorCount = 0;
	int cardNumber = 0;
	QList<int> controlCodes;
};

DummyReceiptSeed loadDummyReceiptSeed(int stage_id)
{
	DummyReceiptSeed seed;
	{
		qf::core::sql::Query q;
		q.exec(QStringLiteral(
			"SELECT "
			"competitors.id AS \"competitors.id\", "
			"competitors.startNumber AS \"competitors.startNumber\", "
			"competitors.classId AS \"competitors.classId\", "
			"competitors.firstName AS \"competitors.firstName\", "
			"competitors.lastName AS \"competitors.lastName\", "
			"competitors.registration AS \"competitors.registration\", "
			"competitors.club AS \"competitors.club\", "
			"competitors.country AS \"competitors.country\", "
			"competitors.siId AS \"competitors.siId\", "
			"competitors.ranking AS \"competitors.ranking\", "
			"competitors.note AS \"competitors.note\", "
			"classes.name AS \"classes.name\", "
			"classdefs.courseId AS \"classdefs.courseId\", "
			"classdefs.startTimeMin AS \"classdefs.startTimeMin\", "
			"classdefs.startIntervalMin AS \"classdefs.startIntervalMin\", "
			"courses.name AS \"courses.name\", "
			"courses.length AS \"courses.length\", "
			"courses.climb AS \"courses.climb\", "
			"TRIM(COALESCE(competitors.lastName, '') || ' ' || COALESCE(competitors.firstName, '')) AS \"competitorName\" "
			"FROM competitors "
			"LEFT JOIN classes ON competitors.classId=classes.id "
			"LEFT JOIN classdefs ON classdefs.classId=classes.id AND classdefs.stageId=%1 "
			"LEFT JOIN courses ON classdefs.courseId=courses.id "
			"ORDER BY competitors.id LIMIT 1")
			.arg(stage_id));
		if(q.next()) {
			const QSqlRecord rec = q.record();
			for(int i = 0; i < rec.count(); ++i)
				seed.competitorRow.insert(rec.fieldName(i), rec.value(i));
		}
	}

	if(seed.competitorRow.isEmpty()) {
		qf::core::sql::Query q;
		q.exec(QStringLiteral(
			"SELECT "
			"classes.id AS \"competitors.classId\", "
			"classes.name AS \"classes.name\", "
			"classdefs.courseId AS \"classdefs.courseId\", "
			"classdefs.startTimeMin AS \"classdefs.startTimeMin\", "
			"classdefs.startIntervalMin AS \"classdefs.startIntervalMin\", "
			"courses.name AS \"courses.name\", "
			"courses.length AS \"courses.length\", "
			"courses.climb AS \"courses.climb\" "
			"FROM classdefs "
			"LEFT JOIN classes ON classdefs.classId=classes.id "
			"LEFT JOIN courses ON classdefs.courseId=courses.id "
			"WHERE classdefs.stageId=%1 "
			"ORDER BY classes.id LIMIT 1")
			.arg(stage_id));
		if(q.next()) {
			const QSqlRecord rec = q.record();
			for(int i = 0; i < rec.count(); ++i)
				seed.competitorRow.insert(rec.fieldName(i), rec.value(i));
		}
	}

	if(seed.competitorRow.value(QStringLiteral("classdefs.courseId")).toInt() == 0) {
		qf::core::sql::Query q;
		q.exec(QStringLiteral(
			"SELECT id AS \"courses.id\", name AS \"courses.name\", length AS \"courses.length\", climb AS \"courses.climb\" "
			"FROM courses ORDER BY id LIMIT 1"));
		if(q.next()) {
			seed.competitorRow.insert(QStringLiteral("classdefs.courseId"), q.value(QStringLiteral("courses.id")));
			seed.competitorRow.insert(QStringLiteral("courses.name"), q.value(QStringLiteral("courses.name")));
			seed.competitorRow.insert(QStringLiteral("courses.length"), q.value(QStringLiteral("courses.length")));
			seed.competitorRow.insert(QStringLiteral("courses.climb"), q.value(QStringLiteral("courses.climb")));
		}
	}

	seed.competitorClassName = seed.competitorRow.value(QStringLiteral("classes.name")).toString().trimmed();
	seed.classId = seed.competitorRow.value(QStringLiteral("competitors.classId")).toInt();
	seed.courseId = seed.competitorRow.value(QStringLiteral("classdefs.courseId")).toInt();
	seed.courseLength = qMax(seed.competitorRow.value(QStringLiteral("courses.length")).toInt(), 4500);
	seed.courseClimb = qMax(seed.competitorRow.value(QStringLiteral("courses.climb")).toInt(), 0);
	seed.startTimeMin = qMax(seed.competitorRow.value(QStringLiteral("classdefs.startTimeMin")).toInt(), 30);
	seed.startIntervalMin = qMax(seed.competitorRow.value(QStringLiteral("classdefs.startIntervalMin")).toInt(), 2);
	seed.cardNumber = seed.competitorRow.value(QStringLiteral("competitors.siId")).toInt();

	if(seed.cardNumber <= 0) {
		const int competitor_id = seed.competitorRow.value(QStringLiteral("competitors.id")).toInt();
		seed.cardNumber = competitor_id > 0 ? (100000 + competitor_id) : 123456;
	}

	if(seed.classId > 0) {
		qf::core::sql::Query q;
		q.exec(QStringLiteral("SELECT COUNT(*) AS cnt FROM competitors WHERE classId=%1").arg(seed.classId));
		if(q.next())
			seed.competitorCount = q.value(QStringLiteral("cnt")).toInt();
	}
	if(seed.competitorCount <= 0) {
		qf::core::sql::Query q;
		q.exec(QStringLiteral("SELECT COUNT(*) AS cnt FROM competitors"));
		if(q.next())
			seed.competitorCount = q.value(QStringLiteral("cnt")).toInt();
	}

	if(seed.courseId > 0) {
		qf::core::sql::Query q;
		q.exec(QStringLiteral(
			"SELECT codes.code AS code "
			"FROM coursecodes "
			"INNER JOIN codes ON coursecodes.codeId=codes.id "
			"WHERE coursecodes.courseId=%1 "
			"ORDER BY coursecodes.position")
			.arg(seed.courseId));
		while(q.next())
			seed.controlCodes << q.value(QStringLiteral("code")).toInt();
	}

	if(seed.controlCodes.isEmpty()) {
		qf::core::sql::Query q;
		q.exec(QStringLiteral("SELECT code FROM codes ORDER BY code LIMIT 5"));
		while(q.next())
			seed.controlCodes << q.value(QStringLiteral("code")).toInt();
	}
	if(seed.controlCodes.isEmpty())
		seed.controlCodes = {31, 45, 62, 78, 90};

	QString competitor_name = seed.competitorRow.value(QStringLiteral("competitorName")).toString().trimmed();
	if(competitor_name.isEmpty())
		competitor_name = QObject::tr("Test Runner");
	seed.competitorRow.insert(QStringLiteral("competitorName"), competitor_name);

	if(seed.competitorRow.value(QStringLiteral("competitors.registration")).toString().trimmed().isEmpty())
		seed.competitorRow.insert(QStringLiteral("competitors.registration"), QStringLiteral("TEST"));
	if(seed.competitorClassName.isEmpty()) {
		seed.competitorClassName = QObject::tr("Open");
		seed.competitorRow.insert(QStringLiteral("classes.name"), seed.competitorClassName);
	}
	seed.competitorRow.insert(QStringLiteral("competitors.siId"), seed.cardNumber);
	seed.competitorRow.insert(QStringLiteral("courses.length"), seed.courseLength);
	seed.competitorRow.insert(QStringLiteral("courses.climb"), seed.courseClimb);

	return seed;
}

bool loadReceiptImagePayload(const QString &file_path, QString &image_base64, QString &image_format, QString &error_message)
{
	QFile file(file_path);
	if(!file.open(QIODevice::ReadOnly)) {
		error_message = QObject::tr("Cannot open image file '%1'.").arg(QDir::toNativeSeparators(file_path));
		return false;
	}

	const QByteArray file_data = file.readAll();
	if(file_data.isEmpty()) {
		error_message = QObject::tr("Selected image file is empty.");
		return false;
	}

	const bool is_svg = file_data.startsWith("<svg") || file_data.contains("<svg");
	if(is_svg) {
		image_base64 = QString::fromLatin1(file_data.toBase64());
		image_format = QStringLiteral("svg");
		return true;
	}

	QBuffer input_buffer;
	input_buffer.setData(file_data);
	input_buffer.open(QIODevice::ReadOnly);
	QImageReader reader(&input_buffer);
	reader.setAutoTransform(true);
	QImage image = reader.read();
	if(image.isNull()) {
		error_message = QObject::tr("Selected file is not a supported image.");
		return false;
	}

	QByteArray png_data;
	QBuffer output_buffer(&png_data);
	if(!output_buffer.open(QIODevice::WriteOnly) || !image.save(&output_buffer, "PNG") || png_data.isEmpty()) {
		error_message = QObject::tr("Cannot prepare selected image for printing.");
		return false;
	}

	image_base64 = QString::fromLatin1(png_data.toBase64());
	image_format = QStringLiteral("png");
	return true;
}
}

ReceiptsSettingsPage::ReceiptsSettingsPage(QWidget *parent)
	: Super(parent)
{
	ui = new Ui::ReceiptsSettingsPage;
	ui->setupUi(this);
	m_caption = tr("Receipts");

	ui->cbxWhenRunnerNotFound->addItem(tr("Error info"), "ErrorInfo");
	ui->cbxWhenRunnerNotFound->addItem(tr("Error info with picture"), "ErrorInfoLong");
	ui->cbxWhenRunnerNotFound->addItem(tr("Receipt without name"), "ReceiptWithoutName");
	ui->cbxWhenRunnerNotFound->setCurrentIndex(0);

	connect(ui->btPrinterOptions, &QAbstractButton::clicked, this, &ReceiptsSettingsPage::onPrinterOptionsClicked);
	connect(ui->btTestPrint, &QAbstractButton::clicked, this, &ReceiptsSettingsPage::onTestPrintClicked);
	connect(ui->chkPrintReceiptQrCode, &QCheckBox::toggled, this, &ReceiptsSettingsPage::updateReceiptMediaControls);
	connect(ui->chkPrintReceiptImage, &QCheckBox::toggled, this, &ReceiptsSettingsPage::updateReceiptMediaControls);
	connect(ui->btSelectReceiptImage, &QAbstractButton::clicked, this, &ReceiptsSettingsPage::onSelectReceiptImageClicked);
	connect(ui->btClearReceiptImage, &QAbstractButton::clicked, this, &ReceiptsSettingsPage::onClearReceiptImageClicked);
	ui->edReceiptQrCodeCaption->setPlaceholderText(defaultReceiptQrCodeCaption());
}

ReceiptsSettingsPage::~ReceiptsSettingsPage()
{
	delete ui;
}

void ReceiptsSettingsPage::load()
{
	loadReceptList();
	ReceiptsSettings settings;
	ui->chkAutoPrint->setChecked(settings.isAutoPrint());
	ui->chkThisReaderOnly->setChecked(settings.isThisReaderOnly());
	{
		auto *cbx = ui->cbxReceipt;
		QString path = settings.receiptPath();
		qfInfo() << "current receipt path:" << path;
		for (int i = 0; i < cbx->count(); ++i) {
			if(cbx->itemData(i).toString() == path) {
				cbx->setCurrentIndex(i);
				break;
			}
		}
		if(cbx->currentIndex() < 0) {
			cbx->setCurrentIndex(0);
			settings.setReceiptPath(cbx->currentData().toString());
		}
	}
	{
		auto *cbx = ui->cbxWhenRunnerNotFound;
		for (int i = 0; i < cbx->count(); ++i) {
			if(cbx->itemData(i).toString() == settings.whenRunnerNotFoundPrint()) {
				cbx->setCurrentIndex(i);
				break;
			}
		}
		if(cbx->currentIndex() < 0) {
			cbx->setCurrentIndex(0);
			settings.setWhenRunnerNotFoundPrint(cbx->currentData().toString());
		}
	}
	auto *event_plugin = qf::gui::framework::getPlugin<Event::EventPlugin>();
	Q_ASSERT(event_plugin);
	auto *event_config = event_plugin->eventConfig();
	Q_ASSERT(event_config);
	m_stageId = qMax(event_config->currentStageId(), 1);
	ui->chkPrintReceiptQrCode->setChecked(event_config->value(eventConfigKey(QStringLiteral("receiptPrintEventQrCode")), false).toBool());
	ui->edReceiptQrCodeBaseUrl->setText(event_config->value(eventConfigKey(QStringLiteral("receiptEventLinkUrl"))).toString().trimmed());
	ui->edReceiptQrCodeCaption->setText(event_config->value(eventConfigKey(QStringLiteral("receiptPrintEventQrCodeCaption")), defaultReceiptQrCodeCaption()).toString().trimmed());
	ui->chkPrintReceiptImage->setChecked(event_config->value(eventConfigKey(QStringLiteral("receiptPrintEventImage")), false).toBool());
	int image_height_mm = event_config->value(eventConfigKey(QStringLiteral("receiptImageHeightMm")), 18).toInt();
	if(image_height_mm < 10)
		image_height_mm = 10;
	else if(image_height_mm > 60)
		image_height_mm = 60;
	ui->edReceiptImageHeight->setValue(image_height_mm);
	m_receiptImageBase64 = event_config->value(eventConfigKey(QStringLiteral("receiptImageDataBase64"))).toString();
	m_receiptImageFormat = event_config->value(eventConfigKey(QStringLiteral("receiptImageFormat"))).toString().trimmed().toLower();
	if(m_receiptImageBase64.isEmpty()) {
		m_receiptImageFormat.clear();
		ui->edReceiptImageFile->clear();
		ui->edReceiptImageFile->setToolTip(QString());
	}
	else {
		ui->edReceiptImageFile->setText(storedReceiptImageLabel(m_receiptImageFormat));
		ui->edReceiptImageFile->setToolTip(tr("Image payload is stored in the event configuration."));
	}
	updateReceiptsPrinterLabel();
	updateReceiptMediaControls();
}

void ReceiptsSettingsPage::save()
{
	ReceiptsSettings settings;
	settings.setAutoPrint(ui->chkAutoPrint->isChecked());
	settings.setThisReaderOnly(ui->chkThisReaderOnly->isChecked());
	settings.setReceiptPath(ui->cbxReceipt->currentData().toString());
	settings.setWhenRunnerNotFoundPrint(ui->cbxWhenRunnerNotFound->currentData().toString());

	auto *event_plugin = qf::gui::framework::getPlugin<Event::EventPlugin>();
	Q_ASSERT(event_plugin);
	auto *event_config = event_plugin->eventConfig();
	Q_ASSERT(event_config);
	event_config->setValue(eventConfigKey(QStringLiteral("receiptPrintEventQrCode")), ui->chkPrintReceiptQrCode->isChecked());
	event_config->setValue(eventConfigKey(QStringLiteral("receiptEventLinkUrl")), ui->edReceiptQrCodeBaseUrl->text().trimmed());
	event_config->setValue(eventConfigKey(QStringLiteral("receiptPrintEventQrCodeCaption")), ui->edReceiptQrCodeCaption->text().trimmed());
	event_config->setValue(eventConfigKey(QStringLiteral("receiptPrintEventImage")), ui->chkPrintReceiptImage->isChecked());
	event_config->setValue(eventConfigKey(QStringLiteral("receiptImageHeightMm")), ui->edReceiptImageHeight->value());
	event_config->setValue(eventConfigKey(QStringLiteral("receiptImageDataBase64")), m_receiptImageBase64);
	event_config->setValue(eventConfigKey(QStringLiteral("receiptImageFormat")), m_receiptImageFormat);
	event_config->save(QStringLiteral("event"));
}

QVariantMap ReceiptsSettingsPage::currentTestReceiptData() const
{
	using qf::core::utils::TreeTable;

	auto *event_plugin = qf::gui::framework::getPlugin<Event::EventPlugin>();
	Q_ASSERT(event_plugin);
	auto *event_config = event_plugin->eventConfig();
	Q_ASSERT(event_config);

	const int stage_id = qMax(m_stageId, 1);
	const bool print_logo = ui->chkPrintReceiptImage->isChecked();
	const bool print_qr_code = ui->chkPrintReceiptQrCode->isChecked();
	const QString qr_url = ui->edReceiptQrCodeBaseUrl->text().trimmed();
	const QString qr_caption = ui->edReceiptQrCodeCaption->text().trimmed();

	DummyReceiptSeed seed = loadDummyReceiptSeed(stage_id);
	seed.competitorClassName = seed.competitorRow.value(QStringLiteral("classes.name")).toString().trimmed();
	seed.competitorRow.insert(QStringLiteral("competitorName"), tr("Test print"));
	seed.controlCodes.clear();

	TreeTable competitor_tt;
	appendColumnsFromValues(competitor_tt, seed.competitorRow);
	{
		const int row_ix = competitor_tt.appendRow();
		setRowValues(competitor_tt, row_ix, seed.competitorRow);
	}
	applyEventConfigValues(competitor_tt);
	for(auto it = seed.competitorRow.cbegin(); it != seed.competitorRow.cend(); ++it)
		competitor_tt.setValue(it.key(), it.value());
	competitor_tt.setValue(QStringLiteral("controlCount"), seed.controlCodes.count());
	competitor_tt.setValue(QStringLiteral("appVersion"), QCoreApplication::applicationVersion());
	competitor_tt.setValue(QStringLiteral("stageCount"), event_plugin->stageCount());
	competitor_tt.setValue(QStringLiteral("currentStageId"), stage_id);
	applyCurrentReceiptMedia(
		competitor_tt,
		seed.competitorClassName,
		print_logo,
		m_receiptImageBase64,
		m_receiptImageFormat,
		ui->edReceiptImageHeight->value(),
		print_qr_code,
		qr_url,
		qr_caption);

	TreeTable card_tt;
	card_tt.appendColumn(QStringLiteral("position"), QMetaType(QMetaType::Int));
	card_tt.appendColumn(QStringLiteral("code"), QMetaType(QMetaType::Int));
	card_tt.appendColumn(QStringLiteral("stpTimeMs"), QMetaType(QMetaType::Int));
	card_tt.appendColumn(QStringLiteral("lapTimeMs"), QMetaType(QMetaType::Int));
	card_tt.appendColumn(QStringLiteral("standLap"), QMetaType(QMetaType::Int));
	card_tt.appendColumn(QStringLiteral("standCummulative"), QMetaType(QMetaType::Int));
	card_tt.appendColumn(QStringLiteral("lossMs"), QMetaType(QMetaType::Int));
	card_tt.appendColumn(QStringLiteral("distance"), QMetaType(QMetaType::Int));

	applyEventConfigValues(card_tt);
	for(auto it = seed.competitorRow.cbegin(); it != seed.competitorRow.cend(); ++it)
		card_tt.setValue(it.key(), it.value());
	card_tt.setValue(QStringLiteral("courseId"), seed.courseId);
	card_tt.setValue(QStringLiteral("appVersion"), QCoreApplication::applicationVersion());
	card_tt.setValue(QStringLiteral("stageCount"), event_plugin->stageCount());
	card_tt.setValue(QStringLiteral("currentStageId"), stage_id);
	card_tt.setValue(QStringLiteral("controlCount"), seed.controlCodes.count());
	applyCurrentReceiptMedia(
		card_tt,
		seed.competitorClassName,
		print_logo,
		m_receiptImageBase64,
		m_receiptImageFormat,
		ui->edReceiptImageHeight->value(),
		print_qr_code,
		qr_url,
		qr_caption);

	int stage_start_time_ms = event_plugin->stageStartMsec(stage_id);
	if(stage_start_time_ms <= 0) {
		QTime tm = event_config->eventDateTime().time();
		stage_start_time_ms = tm.isValid() ? tm.msecsSinceStartOfDay() : (10 * 60 * 60 * 1000);
	}

	const int start_time_ms = qMax(seed.startTimeMin, 1) * 60 * 1000;
	const int check_time_ms = qMax(start_time_ms - 3 * 60 * 1000, 0);
	const int total_time_ms = qMax(15 * 60 * 1000, seed.courseLength * 360 + seed.courseClimb * 1200);
	const int finish_time_ms = start_time_ms + total_time_ms;
	const int ranking_upper_bound = qMax(seed.competitorCount, 1);
	const int competitors_finished = 0;
	const int current_standings = 0;
	const int best_time_ms = total_time_ms;
	const bool is_relay = false;
	const int relay_number = qMax(seed.competitorRow.value(QStringLiteral("competitors.startNumber")).toInt(), 1);

	card_tt.setValue(QStringLiteral("runId"), 0);
	card_tt.setValue(QStringLiteral("stageStartTimeMs"), stage_start_time_ms);
	card_tt.setValue(QStringLiteral("checkTimeMs"), check_time_ms);
	card_tt.setValue(QStringLiteral("startTimeMs"), start_time_ms);
	card_tt.setValue(QStringLiteral("finishTimeMs"), finish_time_ms);
	card_tt.setValue(QStringLiteral("cardNumber"), seed.cardNumber);
	card_tt.setValue(QStringLiteral("timeMs"), total_time_ms);
	card_tt.setValue(QStringLiteral("bestTime"), best_time_ms);
	card_tt.setValue(QStringLiteral("currentStandings"), current_standings);
	card_tt.setValue(QStringLiteral("competitorsFinished"), competitors_finished);
	card_tt.setValue(QStringLiteral("isOk"), true);
	card_tt.setValue(QStringLiteral("isBadCheck"), false);
	card_tt.setValue(QStringLiteral("isMisPunch"), false);
	card_tt.setValue(QStringLiteral("isCardLent"), false);
	card_tt.setValue(QStringLiteral("extraCodes"), QVariantList());
	card_tt.setValue(QStringLiteral("isRelay"), is_relay);
	card_tt.setValue(QStringLiteral("relayNumber"), relay_number);
	card_tt.setValue(QStringLiteral("leg"), is_relay ? 1 : 0);
	card_tt.setValue(QStringLiteral("courses.length"), seed.courseLength);
	card_tt.setValue(QStringLiteral("courses.climb"), seed.courseClimb);
	card_tt.setValue(QStringLiteral("data"), QVariantMap());

	QList<int> leg_weights;
	for(int code : seed.controlCodes)
		leg_weights << (90 + (code % 35));
	leg_weights << 100; // finish leg

	int cumulative_time_ms = 0;
	int remaining_weight = 0;
	for(int weight : leg_weights)
		remaining_weight += weight;

	for(int i = 0; i < seed.controlCodes.count(); ++i) {
		const int weight = leg_weights.value(i, 100);
		const int remaining_legs = leg_weights.count() - i - 1;
		const int remaining_time_ms = total_time_ms - cumulative_time_ms;
		const int min_reserved_ms = remaining_legs * 30 * 1000;
		const int max_leg_time_ms = qMax(30 * 1000, remaining_time_ms - min_reserved_ms);
		const int leg_time_ms = qBound(30 * 1000, (remaining_time_ms * weight) / qMax(remaining_weight, 1), max_leg_time_ms);
		cumulative_time_ms += leg_time_ms;
		remaining_weight -= weight;

		const int stand_lap = 1;
		const int stand_cummulative = qBound(1, 1, ranking_upper_bound);
		const int distance = (seed.courseLength * (i + 1)) / qMax(seed.controlCodes.count() + 1, 1);
		const int loss_ms = 0;

		const int row_ix = card_tt.appendRow();
		setRowValues(card_tt, row_ix, QVariantMap{
			{QStringLiteral("position"), i + 1},
			{QStringLiteral("code"), seed.controlCodes.value(i)},
			{QStringLiteral("stpTimeMs"), cumulative_time_ms},
			{QStringLiteral("lapTimeMs"), leg_time_ms},
			{QStringLiteral("standLap"), stand_lap},
			{QStringLiteral("standCummulative"), stand_cummulative},
			{QStringLiteral("lossMs"), loss_ms},
			{QStringLiteral("distance"), distance},
		});
	}

	const int finish_leg_time_ms = qMax(total_time_ms - cumulative_time_ms, 30 * 1000);
	const int finish_row_ix = card_tt.appendRow();
	setRowValues(card_tt, finish_row_ix, QVariantMap{
		{QStringLiteral("position"), seed.controlCodes.count() + 1},
		{QStringLiteral("code"), 0},
		{QStringLiteral("stpTimeMs"), total_time_ms},
		{QStringLiteral("lapTimeMs"), finish_leg_time_ms},
		{QStringLiteral("standLap"), 1},
		{QStringLiteral("standCummulative"), 1},
		{QStringLiteral("lossMs"), 0},
		{QStringLiteral("distance"), seed.courseLength},
	});

	QVariantMap ret;
	ret[QStringLiteral("competitor")] = competitor_tt.toVariant();
	ret[QStringLiteral("card")] = card_tt.toVariant();
	return ret;
}

void ReceiptsSettingsPage::loadReceptList()
{
	qfLogFuncFrame();
	ui->cbxReceipt->clear();
	auto *receipts_plugin = qf::gui::framework::getPlugin<ReceiptsPlugin>();
	for(const auto &i : receipts_plugin->listReportFiles("receipts")) {
		qfDebug() << i.reportName << i.reportFilePath;
		ui->cbxReceipt->addItem(i.reportName, i.reportFilePath);
	}
	ui->cbxReceipt->setCurrentIndex(-1);
}

void ReceiptsSettingsPage::updateReceiptsPrinterLabel()
{
	ReceiptsSettings settings;
	ui->btPrinterOptions->setText(settings.printerCaption());
	ui->btPrinterOptions->setIcon(qf::gui::Style::icon("printer"));
}

void ReceiptsSettingsPage::updateReceiptMediaControls()
{
	const bool qr_code_enabled = ui->chkPrintReceiptQrCode->isChecked();
	ui->edReceiptQrCodeBaseUrl->setEnabled(qr_code_enabled);
	ui->lbReceiptQrCodeCaption->setEnabled(qr_code_enabled);
	ui->edReceiptQrCodeCaption->setEnabled(qr_code_enabled);

	const bool image_enabled = ui->chkPrintReceiptImage->isChecked();
	ui->lbReceiptImageHeight->setEnabled(image_enabled);
	ui->edReceiptImageHeight->setEnabled(image_enabled);
	ui->edReceiptImageFile->setEnabled(image_enabled);
	ui->btSelectReceiptImage->setEnabled(image_enabled);
	ui->btClearReceiptImage->setEnabled(image_enabled && !m_receiptImageBase64.isEmpty());
}

void ReceiptsSettingsPage::onPrinterOptionsClicked()
{
	ReceiptsPrinterOptionsDialog dlg(this);
	//dlg.setPrinterOptions(getPlugin<ReceiptsPlugin>()->receiptsPrinter()->printerOptions());
	if(dlg.exec()) {
		//getPlugin<ReceiptsPlugin>()->setReceiptsPrinterOptions(dlg.printerOptions());
		updateReceiptsPrinterLabel();
	}
}

void ReceiptsSettingsPage::onTestPrintClicked()
{
	auto *receipts_plugin = qf::gui::framework::getPlugin<ReceiptsPlugin>();
	Q_ASSERT(receipts_plugin);
	const QString report_file_name = ui->cbxReceipt->currentData().toString();
	if(report_file_name.isEmpty()) {
		QMessageBox::warning(this, tr("Warning"), tr("Receipt report type is not defined.\nPlease choose a receipt template first."));
		return;
	}
	if(!receipts_plugin->printTestReceipt(report_file_name, currentTestReceiptData())) {
		QMessageBox::warning(this, tr("Warning"), tr("Test print failed. Check the printer setup."));
	}
}

void ReceiptsSettingsPage::onSelectReceiptImageClicked()
{
	const QString file_path = QFileDialog::getOpenFileName(
		this,
		tr("Select receipt image"),
		QString(),
		tr("Images (*.png *.jpg *.jpeg *.bmp *.gif *.webp *.svg);;All files (*)"));
	if(file_path.isEmpty())
		return;

	QString image_base64;
	QString image_format;
	QString error_message;
	if(!loadReceiptImagePayload(file_path, image_base64, image_format, error_message)) {
		QMessageBox::warning(this, tr("Warning"), error_message);
		return;
	}

	m_receiptImageBase64 = image_base64;
	m_receiptImageFormat = image_format;
	ui->edReceiptImageFile->setText(QDir::toNativeSeparators(file_path));
	ui->edReceiptImageFile->setToolTip(QDir::toNativeSeparators(file_path));
	ui->chkPrintReceiptImage->setChecked(true);
	updateReceiptMediaControls();
}

void ReceiptsSettingsPage::onClearReceiptImageClicked()
{
	m_receiptImageBase64.clear();
	m_receiptImageFormat.clear();
	ui->edReceiptImageFile->clear();
	ui->edReceiptImageFile->setToolTip(QString());
	updateReceiptMediaControls();
}

QString ReceiptsSettingsPage::eventConfigKey(const QString &suffix) const
{
	return QStringLiteral("event.") + suffix + QStringLiteral(".E") + QString::number(qMax(m_stageId, 1));
}

}
