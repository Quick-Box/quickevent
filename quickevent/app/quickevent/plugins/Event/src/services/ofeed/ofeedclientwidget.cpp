#include "ofeedclientwidget.h"
#include "ui_ofeedclientwidget.h"
#include "ofeedclient.h"

#include <qf/gui/framework/mainwindow.h>
#include <qf/gui/dialogs/messagebox.h>
#include <qf/gui/style.h>

#include <qf/core/assert.h>
#include <qf/core/log.h>

#include <QClipboard>
#include <QDesktopServices>
#include <QFileDialog>
#include <QGuiApplication>
#include <QPointer>
#include <QUrl>
#include <QUrlQuery>

#include <initializer_list>

#include <plugins/Event/src/eventplugin.h>

namespace Event::services {

namespace {
struct ParsedOFeedSetupLink {
	QString host_url;
	QString event_id;
	QString event_password;
	QString error;
};

QString firstNonEmptyQueryValue(const QUrlQuery &query, std::initializer_list<const char *> keys)
{
	for(const char *key : keys) {
		const QString value = query.queryItemValue(QString::fromLatin1(key), QUrl::FullyDecoded).trimmed();
		if(!value.isEmpty())
			return value;
	}
	return {};
}

QString userFacingHostUrl(const QString &host_url)
{
	QString value = host_url.trimmed();
	if(value.isEmpty())
		return {};

	if(!value.contains(QStringLiteral("://")))
		value.prepend(QStringLiteral("https://"));

	QUrl parsed_url = QUrl::fromUserInput(value);
	if(!parsed_url.isValid() || parsed_url.host().isEmpty())
		return {};

	QString host = parsed_url.host().toLower();
	if(host == QStringLiteral("api.orienteerfeed.com")) {
		parsed_url.setHost(QStringLiteral("orienteerfeed.com"));
	}
	else if(host == QStringLiteral("www.orienteerfeed.com")) {
		parsed_url.setHost(QStringLiteral("orienteerfeed.com"));
	}

	QUrl base_url;
	base_url.setScheme(parsed_url.scheme().isEmpty() ? QStringLiteral("https") : parsed_url.scheme());
	base_url.setHost(parsed_url.host());
	if(parsed_url.port() > 0)
		base_url.setPort(parsed_url.port());
	return base_url.toString();
}

QString eventWebsiteUrl(const QString &host_url, const QString &event_id)
{
	const QString base_host_url = userFacingHostUrl(host_url);
	const QString trimmed_event_id = event_id.trimmed();
	if(base_host_url.isEmpty() || trimmed_event_id.isEmpty())
		return {};

	QUrl url(base_host_url);
	url.setPath(QStringLiteral("/events/%1").arg(trimmed_event_id));
	return url.toString();
}

QString receiptEventLinkUrl(const QString &host_url, const QString &event_id)
{
	QString base_host_url = userFacingHostUrl(host_url);
	if(base_host_url.isEmpty())
		base_host_url = QStringLiteral("https://orienteerfeed.com");

	QUrl url(base_host_url);
	const QString trimmed_event_id = event_id.trimmed();
	if(trimmed_event_id.isEmpty())
		return url.toString();

	url.setPath(QStringLiteral("/events/%1").arg(trimmed_event_id));
	QUrlQuery query(url);
	query.addQueryItem(QStringLiteral("tab"), QStringLiteral("results"));
	url.setQuery(query);
	return url.toString();
}

ParsedOFeedSetupLink parseOFeedSetupLink(const QString &input)
{
	ParsedOFeedSetupLink result;
	const QString text = input.trimmed();
	if(text.isEmpty()) {
		result.error = QObject::tr("Clipboard does not contain OFeed setup link.");
		return result;
	}

	QUrl url(text);
	if(!url.isValid() || url.scheme().isEmpty()) {
		url = QUrl::fromUserInput(text);
	}

	QUrlQuery query;
	if(url.isValid() && !url.scheme().isEmpty()) {
		query = QUrlQuery(url);
	}
	if(!query.hasQueryItem(QStringLiteral("id"))
		&& !query.hasQueryItem(QStringLiteral("eventId"))
		&& !query.hasQueryItem(QStringLiteral("event_id"))
		&& text.contains(QLatin1Char('='))) {
		query = QUrlQuery(text);
	}

	result.host_url = userFacingHostUrl(firstNonEmptyQueryValue(query, {"url", "host", "hostUrl", "baseUrl", "base_url"}));
	result.event_id = firstNonEmptyQueryValue(query, {"id", "eventId", "event_id"});
	result.event_password = firstNonEmptyQueryValue(query, {"pwd", "password", "pass"});

	const QString auth = firstNonEmptyQueryValue(query, {"auth"});
	if(!auth.isEmpty() && auth.compare(QStringLiteral("basic"), Qt::CaseInsensitive) != 0) {
		result.error = QObject::tr("Unsupported auth type '%1' in setup link.").arg(auth);
		return result;
	}

	if(result.event_id.isEmpty() || result.event_password.isEmpty()) {
		result.error = QObject::tr("Setup link must contain id and pwd (or password) query parameters.");
	}
	return result;
}
}

OFeedClientWidget::OFeedClientWidget(QWidget *parent)
	: Super(parent)
	, ui(new Ui::OFeedClientWidget)
{
	setPersistentSettingsId("OFeedClientWidget");
	ui->setupUi(this);

	OFeedClient *svc = service();
	if(svc) {
		OFeedClientSettings ss = svc->settings();
		ui->edExportInterval->setValue(ss.exportIntervalSec());
		ui->edHostUrl->setText(userFacingHostUrl(svc->hostUrl()));
		ui->edEventId->setText(svc->eventId());
		ui->edEventPassword->setText(svc->eventPassword());
		ui->edChangelogOrigin->setText(svc->changelogOrigin());
		ui->additionalSettingsRunXmlValidation->setChecked(svc->runXmlValidation());
		ui->additionalSettingsPrintEventImageOnReceipt->setChecked(svc->printEventImageOnReceipt());
		ui->edReceiptImageHeight->setValue(svc->receiptImageHeightMm());
		ui->lbReceiptImageHeight->setEnabled(svc->printEventImageOnReceipt());
		ui->edReceiptImageHeight->setEnabled(svc->printEventImageOnReceipt());
		ui->additionalSettingsPrintEventQrCodeOnReceipt->setChecked(svc->printEventQrCodeOnReceipt());
		ui->edReceiptEventLink->setText(svc->receiptEventLinkUrl());
		ui->edReceiptEventQrCodeCaption->setText(svc->receiptEventQrCodeCaption());
		ui->edReceiptEventQrCodeCaption->setPlaceholderText(svc->defaultReceiptEventQrCodeCaption());
		ui->lbReceiptEventLink->setEnabled(svc->printEventQrCodeOnReceipt());
		ui->edReceiptEventLink->setEnabled(svc->printEventQrCodeOnReceipt());
		ui->lbReceiptEventQrCodeCaption->setEnabled(svc->printEventQrCodeOnReceipt());
		ui->edReceiptEventQrCodeCaption->setEnabled(svc->printEventQrCodeOnReceipt());
		ui->lbEventImageCacheStatus->setText(svc->hasCachedEventImage() ? tr("Cached image is available") : tr("No cached image"));
		ui->processChangesOnOffButton->setText(svc->runChangesProcessing() ? tr("ON") : tr("OFF"));
		ui->processChangesOnOffButton->setChecked(svc->runChangesProcessing());
		ui->processChangesOnOffButton->setStyleSheet(
		"QPushButton {"
		"  padding: 5px;"
		"  border-radius: 4px;"
		"  border: 2px solid gray;"
		"}"
		"QPushButton:checked {"
		"  border: 2px solid green;"
		"  color: green;"
		"}"
		"QPushButton:!checked {"
		"  border: 2px solid red;"
		"  color: red;"
		"}"
		);
		ui->processChangesOnOffLabel->setText(svc->runChangesProcessing() ? tr("Changes are automatically processed") : tr("Processing changes is deactivated"));
	}
	syncReceiptEventLinkWithDefaults();

	connect(ui->btExportResultsXml30, &QPushButton::clicked, this, &OFeedClientWidget::onBtExportResultsXml30Clicked);
	connect(ui->btExportStartListXml30, &QPushButton::clicked, this, &OFeedClientWidget::onBtExportStartListXml30Clicked);
	connect(ui->processChangesOnOffButton, &QPushButton::clicked,this, &OFeedClientWidget::onProcessChangesOnOffButtonClicked);
	connect(ui->btPasteSetupLink, &QPushButton::clicked, this, &OFeedClientWidget::onBtPasteSetupLinkClicked);
	connect(ui->btTestConnection, &QPushButton::clicked, this, &OFeedClientWidget::onBtTestConnectionClicked);
	connect(ui->btRefreshEventImage, &QPushButton::clicked, this, &OFeedClientWidget::onBtRefreshEventImageClicked);
	connect(ui->btOpenEventWebsite, &QToolButton::clicked, this, &OFeedClientWidget::onBtOpenEventWebsiteClicked);
	const QIcon show_password_icon = qf::gui::Style::icon("eye");
	const QIcon hide_password_icon = qf::gui::Style::icon("eye-off");
	const QIcon open_event_website_icon = qf::gui::Style::icon("globe");
	ui->btOpenEventWebsite->setIcon(open_event_website_icon);
	const auto update_password_visibility = [this, show_password_icon, hide_password_icon](bool visible) {
		ui->edEventPassword->setEchoMode(visible ? QLineEdit::EchoMode::Normal : QLineEdit::EchoMode::Password);
		ui->btToggleEventPasswordVisibility->setIcon(visible ? hide_password_icon : show_password_icon);
		ui->btToggleEventPasswordVisibility->setToolTip(visible ? tr("Hide password value") : tr("Show password value"));
	};
	connect(ui->btToggleEventPasswordVisibility, &QToolButton::toggled, this, update_password_visibility);
	update_password_visibility(ui->btToggleEventPasswordVisibility->isChecked());
	connect(ui->additionalSettingsPrintEventImageOnReceipt, &QCheckBox::toggled, this, [this](bool on) {
		ui->lbReceiptImageHeight->setEnabled(on);
		ui->edReceiptImageHeight->setEnabled(on);
		updateTestConnectionState();
	});
	connect(ui->additionalSettingsPrintEventQrCodeOnReceipt, &QCheckBox::toggled, this, [this](bool on) {
		ui->lbReceiptEventLink->setEnabled(on);
		ui->edReceiptEventLink->setEnabled(on);
		ui->lbReceiptEventQrCodeCaption->setEnabled(on);
		ui->edReceiptEventQrCodeCaption->setEnabled(on);
	});
	connect(ui->edHostUrl, &QLineEdit::textChanged, this, [this]() {
		updateTestConnectionState();
		syncReceiptEventLinkWithDefaults();
	});
	connect(ui->edEventId, &QLineEdit::textChanged, this, [this]() {
		updateTestConnectionState();
		syncReceiptEventLinkWithDefaults();
	});
	connect(ui->edEventPassword, &QLineEdit::textChanged, this, &OFeedClientWidget::updateTestConnectionState);
	updateTestConnectionState();
}

OFeedClientWidget::~OFeedClientWidget()
{
	delete ui;
}

bool OFeedClientWidget::acceptDialogDone(int result)
{
	if(result == QDialog::Accepted) {
		if(!saveSettings()) {
			return false;
		}
	}
	return true;
}

OFeedClient *OFeedClientWidget::service()
{
	auto *svc = qobject_cast<OFeedClient*>(Service::serviceByName(OFeedClient::serviceName()));
	QF_ASSERT(svc, OFeedClient::serviceName() + " doesn't exist", return nullptr);
	return svc;
}

bool OFeedClientWidget::saveSettings()
{
	OFeedClient *svc = service();
	if(svc) {
		OFeedClientSettings ss = svc->settings();
		ss.setExportIntervalSec(ui->edExportInterval->value());
		svc->setHostUrl(ui->edHostUrl->text().trimmed());
		svc->setEventId(ui->edEventId->text().trimmed());
		svc->setEventPassword(ui->edEventPassword->text().trimmed());
		svc->setChangelogOrigin(ui->edChangelogOrigin->text().trimmed());
		svc->setRunXmlValidation(ui->additionalSettingsRunXmlValidation->isChecked());
		svc->setPrintEventImageOnReceipt(ui->additionalSettingsPrintEventImageOnReceipt->isChecked());
		svc->setReceiptImageHeightMm(ui->edReceiptImageHeight->value());
		svc->setPrintEventQrCodeOnReceipt(ui->additionalSettingsPrintEventQrCodeOnReceipt->isChecked());
		svc->setReceiptEventLinkUrl(ui->edReceiptEventLink->text().trimmed());
		svc->setReceiptEventQrCodeCaption(ui->edReceiptEventQrCodeCaption->text().trimmed());
		svc->setSettings(ss);
	}
	return true;
}

void OFeedClientWidget::onBtExportResultsXml30Clicked()
{
	OFeedClient *svc = service();
	if(svc) {
		saveSettings();
		qfInfo() << OFeedClient::serviceName() + " [results - manual upload]";
		svc->exportResultsIofXml3();
	}
}

void OFeedClientWidget::onBtExportStartListXml30Clicked()
{
	OFeedClient *svc = service();
	if(svc) {
		saveSettings();
		qfInfo() << OFeedClient::serviceName() + " [startlist - manual upload]";
		svc->exportStartListIofXml3();
	}
}

void OFeedClientWidget::onProcessChangesOnOffButtonClicked()
{
	OFeedClient *svc = service();
    if (!svc)
        return;

    bool newState = !svc->runChangesProcessing();
    svc->setRunChangesProcessing(newState);

    // Update button text or icon
    ui->processChangesOnOffButton->setText(newState ? tr("ON") : tr("OFF"));
	ui->processChangesOnOffButton->setChecked(svc->runChangesProcessing());
	ui->processChangesOnOffLabel->setText(svc->runChangesProcessing() ? tr("Changes are automatically processed") : tr("Processing changes is deactivated"));
}

void OFeedClientWidget::onBtPasteSetupLinkClicked()
{
	const QClipboard *clipboard = QGuiApplication::clipboard();
	const QString clipboard_text = clipboard ? clipboard->text(QClipboard::Clipboard).trimmed() : QString();
	const ParsedOFeedSetupLink parsed_link = parseOFeedSetupLink(clipboard_text);
	if(!parsed_link.error.isEmpty()) {
		qf::gui::dialogs::MessageBox::showError(this, parsed_link.error);
		return;
	}

	if(!parsed_link.host_url.isEmpty())
		ui->edHostUrl->setText(parsed_link.host_url);
	ui->edEventId->setText(parsed_link.event_id);
	ui->edEventPassword->setText(parsed_link.event_password);

	ui->lbConnectionTestResult->setStyleSheet("color:#0a7a2f;");
	ui->lbConnectionTestResult->setText(tr("Setup link parsed. Credentials were filled in."));
	updateTestConnectionState();
}

void OFeedClientWidget::onBtTestConnectionClicked()
{
	OFeedClient *svc = service();
	if(!svc) {
		return;
	}

	m_isTestConnectionRunning = true;
	ui->btTestConnection->setText(tr("Testing..."));
	ui->lbConnectionTestResult->setStyleSheet("color:#666;");
	ui->lbConnectionTestResult->setText(tr("Testing connection..."));
	updateTestConnectionState();

	const QString host_url = ui->edHostUrl->text().trimmed();
	const QString event_id = ui->edEventId->text().trimmed();
	const QString event_password = ui->edEventPassword->text().trimmed();
	QPointer<OFeedClientWidget> widget_guard(this);

	svc->testConnection(host_url, event_id, event_password, [widget_guard](bool success, const QString &message) {
		if(!widget_guard) {
			return;
		}
		widget_guard->m_isTestConnectionRunning = false;
		widget_guard->ui->btTestConnection->setText(widget_guard->tr("Test connection"));
		widget_guard->ui->lbConnectionTestResult->setStyleSheet(success ? "color:#0a7a2f;" : "color:#b00020;");
		widget_guard->ui->lbConnectionTestResult->setText(message);
		widget_guard->updateTestConnectionState();
	});
}

void OFeedClientWidget::onBtRefreshEventImageClicked()
{
	OFeedClient *svc = service();
	if(!svc)
		return;

	saveSettings();
	m_isImageRefreshRunning = true;
	ui->lbEventImageCacheStatus->setStyleSheet("color:#666;");
	ui->lbEventImageCacheStatus->setText(tr("Refreshing image cache..."));
	updateTestConnectionState();

	QPointer<OFeedClientWidget> widget_guard(this);
	svc->refreshEventImageCache([widget_guard](bool success, const QString &message) {
		if(!widget_guard)
			return;
		widget_guard->m_isImageRefreshRunning = false;
		widget_guard->ui->lbEventImageCacheStatus->setStyleSheet(success ? "color:#0a7a2f;" : "color:#b00020;");
		widget_guard->ui->lbEventImageCacheStatus->setText(message);
		widget_guard->updateTestConnectionState();
	});
}

void OFeedClientWidget::onBtOpenEventWebsiteClicked()
{
	const QString event_website_url = eventWebsiteUrl(ui->edHostUrl->text(), ui->edEventId->text());
	if(event_website_url.isEmpty())
		return;
	QDesktopServices::openUrl(QUrl(event_website_url));
}

QString OFeedClientWidget::defaultReceiptEventLink() const
{
	return receiptEventLinkUrl(ui->edHostUrl->text(), ui->edEventId->text());
}

void OFeedClientWidget::syncReceiptEventLinkWithDefaults()
{
	const QString current_link = ui->edReceiptEventLink->text().trimmed();
	const QString default_link = defaultReceiptEventLink();
	if(current_link.isEmpty() || current_link == m_lastAutoReceiptEventLink) {
		ui->edReceiptEventLink->setText(default_link);
	}
	ui->edReceiptEventLink->setPlaceholderText(default_link);
	m_lastAutoReceiptEventLink = default_link;
}

void OFeedClientWidget::updateTestConnectionState()
{
	const bool has_required_credentials = !ui->edHostUrl->text().trimmed().isEmpty()
		&& !ui->edEventId->text().trimmed().isEmpty()
		&& !ui->edEventPassword->text().trimmed().isEmpty();
	const bool can_refresh_event_image = has_required_credentials
		&& ui->additionalSettingsPrintEventImageOnReceipt->isChecked()
		&& !m_isImageRefreshRunning;
	const QString event_website_url = eventWebsiteUrl(ui->edHostUrl->text(), ui->edEventId->text());
	const bool has_event_website_url = !event_website_url.isEmpty();
	ui->btOpenEventWebsite->setEnabled(has_event_website_url);
	ui->btOpenEventWebsite->setToolTip(has_event_website_url ? tr("Open event page in browser") : tr("Fill Url and Event id to open event page"));
	ui->btTestConnection->setEnabled(has_required_credentials && !m_isTestConnectionRunning);
	ui->btRefreshEventImage->setEnabled(can_refresh_event_image);
}
}
