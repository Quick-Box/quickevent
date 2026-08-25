#include "radiosenderservicewidget.h"
#include "ui_radiosenderservicewidget.h"
#include "radiosenderservice.h"

#include "../../eventplugin.h"

#include <qf/core/assert.h>
#include <qf/gui/framework/mainwindow.h>

#include <QDialog>

using qf::gui::framework::getPlugin;

namespace Event::services {

RadioSenderServiceWidget::RadioSenderServiceWidget(RadioSenderService *service, QWidget *parent)
	: Super(parent)
	, ui(new Ui::RadioSenderServiceWidget)
	, m_service(service)
{
	Q_ASSERT(m_service);
	setPersistentSettingsId(QStringLiteral("RadioSenderServiceWidget"));
	ui->setupUi(this);
	auto *event_plugin = getPlugin<EventPlugin>();
	const auto config = event_plugin->appDbConfig().radioSenderConfig();
	ui->edListenAddress->setText(config.listenAddress);
	ui->edPort->setValue(config.port);
	ui->edStartControl->setValue(config.startControl);
	ui->edFinishControl->setValue(config.finishControl);

	ui->edTestPunch->addItem("11;1;09:34:02.714;;;");     // bib 1 start
	ui->edTestPunch->addItem("901;1;09:34:08.892;;;");// bib 1 finish
	ui->edTestPunch->addItem("11;2;09:41:06.986;;;");     // bib 2 start
	ui->edTestPunch->addItem("11;2;09:41:06.986;;ANN;");  // bib 2 start canceled
	ui->edTestPunch->addItem("11;3;09:41:06.986;;;");     // bib 3 start (note: with the time previously assigned to bib 2)
	ui->edTestPunch->addItem("11;2;09:41:30.323;DNS;;");  // bib 2 DNS (time represent when the DNS is recorded)
	ui->edTestPunch->addItem("11;4;09:41:38.751;DNS;;");  // bib 4 DNS
	ui->edTestPunch->addItem("11;4;09:41:38.751;;ANN;");  // bib 4 DNS canceled (note: DNS flag is not present, only the time)
	ui->edTestPunch->addItem("11;4;09:42:20.959;;;");     // bib 4 start
	ui->edTestPunch->addItem("11;4;09:42:20.959;;ANN;");  // bib 4 start canceled (note: the time is discarded not assigned to anyone else)
	ui->edTestPunch->addItem("11;4;09:42:36.161;;;");     // bib 4 start
	ui->edTestPunch->addItem("901;3;09:42:40.677;;;");    // bib 3 finish
	ui->edTestPunch->addItem("901;3;09:42:40.677;;ANN;"); // bib 3 finish canceled (note: the time is discarded)
	ui->edTestPunch->addItem("901;3;09:42:50.739;;;");    // bib 3 finish
	ui->edTestPunch->addItem("901;4;09:43:17.101;;;");    // bib 4 finish
	connect(ui->btSendTestPunch, &QPushButton::clicked, this, [this] {
		onTestPunch(ui->edTestPunch->currentText());
	});
	connect(m_service, &RadioSenderService::receivedLineLogged,
		this, &RadioSenderServiceWidget::updateReceivedLineLog);
	connect(m_service, &RadioSenderService::statusChanged,
		this, &RadioSenderServiceWidget::updateServiceControls);
	connect(ui->btStart, &QPushButton::clicked, this, [this] { m_service->setRunning(true); });
	connect(ui->btStop, &QPushButton::clicked, this, [this] { m_service->setRunning(false); });
	updateReceivedLineLog();
	updateServiceControls();
}

RadioSenderServiceWidget::~RadioSenderServiceWidget()
{
	delete ui;
}

bool RadioSenderServiceWidget::acceptDialogDone(int result)
{
	if (result == QDialog::Accepted)
		saveConfig();
	return true;
}

void RadioSenderServiceWidget::updateServiceControls()
{
	const bool running = m_service->isRunning();
	ui->btStart->setEnabled(!running);
	ui->btStop->setEnabled(running);
}

void RadioSenderServiceWidget::updateReceivedLineLog()
{
	ui->edReceivedLines->setPlainText(m_service->receivedLineLog().join(QStringLiteral("\n")));
}

void RadioSenderServiceWidget::saveConfig()
{
	auto *event_plugin = getPlugin<EventPlugin>();
	auto config = event_plugin->appDbConfig().radioSenderConfig();
	config.listenAddress = ui->edListenAddress->text().trimmed();
	config.port = ui->edPort->value();
	config.startControl = ui->edStartControl->value();
	config.finishControl = ui->edFinishControl->value();
	event_plugin->appDbConfig().setRadioSenderConfig(config);

	if (m_service->isRunning()) {
		m_service->stop();
		m_service->run();
	}
}

void RadioSenderServiceWidget::onTestPunch(const QString &line)
{
	m_service->processLine(line.toUtf8());
}

} // namespace Event::services
