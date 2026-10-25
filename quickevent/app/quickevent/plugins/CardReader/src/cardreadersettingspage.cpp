
//
// Author: Frantisek Vacek <fanda.vacek@volny.cz>, (C) 2011
//
// Copyright: See COPYING file that comes with this distribution
//

#include "cardreadersettingspage.h"
#include "ui_cardreadersettingspage.h"

#include "cardchecker.h"
//#include "cardreaderwidget.h"
#include "cardreaderplugin.h"
#include "cardreadersettings.h"

#include <qbluetoothdeviceinfo.h>
#include <siut/commport.h>
#include <siut/sidevicedriver.h>

#include <qf/gui/framework/mainwindow.h>

#include <qf/core/log.h>

#include <algorithm>
#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothDeviceInfo>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressDialog>
#include <QSerialPortInfo>
#include <QTimer>

namespace CardReader {
struct BtDeviceInfo {
	QString address;
	QString name;
};
}
Q_DECLARE_METATYPE(CardReader::BtDeviceInfo)

namespace CardReader {

CardReaderSettingsPage::CardReaderSettingsPage(QWidget *parent)
	: Super(parent)
{
	ui = new Ui::CardReaderSettingsPage;
	ui->setupUi(this);

	connect(ui->btTestConnection, &QAbstractButton::clicked, this, &CardReaderSettingsPage::onTestConnectionClicked);
	connect(ui->btScanBt, &QAbstractButton::clicked, this, &CardReaderSettingsPage::onScanBtClicked);

	m_caption = tr("Card reader");

	{
		auto *cbx = ui->cbxCardCheckType;
		for(auto *checker : qf::gui::framework::getPlugin<CardReaderPlugin>()->cardCheckers()) {
			cbx->addItem(checker->caption(), checker->nameId());
		}
	}
	{
		auto *cbx = ui->cbxReaderMode;
		cbx->addItem(tr("Readout"), "Readout");
		cbx->setItemData(0, tr("Readout mode - default"), Qt::ToolTipRole);
		cbx->addItem(tr("Edit on punch"), "EditOnPunch");
		cbx->setItemData(1, tr("Show Edit/Insert competitor dialog when SI Card is inserted into the reader station"), Qt::ToolTipRole);
	}

	QTimer::singleShot(0, this, &CardReaderSettingsPage::load);
}

CardReaderSettingsPage::~CardReaderSettingsPage()
{
	delete ui;
}

namespace {
void load_combo_text(QComboBox *cbx, const QVariant &val, bool init_current_index = true)
{
	int ix = cbx->findText(val.toString());
	if(ix >= 0) {
		cbx->setCurrentIndex(ix);
	}
	else {
		if(init_current_index) {
			cbx->setCurrentIndex(0);
		}
		else if(cbx->isEditable()) {
			cbx->lineEdit()->setText(val.toString());
		}
	}
}

/// Returns the device name and address shown in the BT SI reader combo box.
QString btsiDisplayText(const BtDeviceInfo &device_info)
{
	const QString displayName = device_info.name.isEmpty() ? QObject::tr("Unnamed device") : device_info.name;
	return QStringLiteral("%1 (%2)").arg(displayName, device_info.address);
}

}

void CardReaderSettingsPage::load()
{
	CardReaderSettings settings;

	// Reader enable flags
	ui->grpSerial->setChecked(settings.isSerialEnabled());
	ui->grpBt->setChecked(settings.isBtEnabled());

	// Serial settings
	{
		ui->lstDevice->clear();
		QList<QSerialPortInfo> port_list = QSerialPortInfo::availablePorts();
		for(const auto &port : port_list) {
			ui->lstDevice->addItem(port.systemLocation());
		}
	}
	load_combo_text(ui->lstDevice, settings.device(), false);
	load_combo_text(ui->lstBaudRate, settings.baudRate());
	load_combo_text(ui->lstDataBits, settings.dataBits());
	load_combo_text(ui->lstStopBits, settings.stopBits());
	load_combo_text(ui->lstParity, settings.parity());
	ui->chkShowRawComData->setChecked(settings.isShowRawComData());
	ui->chkDisableCRCCheck->setChecked(settings.isDisableCRCCheck());
	{
		// BT SI Reader settings — select the item whose data matches the saved address.
		const QVariantMap btDevice = settings.btsiDevice();
		const BtDeviceInfo savedDevice{
			btDevice.value(QStringLiteral("address")).toString(),
			btDevice.value(QStringLiteral("name")).toString()};
		if (!savedDevice.address.isEmpty()) {
			int matchIndex = -1;
			for (int i = 0; i < ui->cbxBtsiAddress->count(); ++i) {
				const auto device = ui->cbxBtsiAddress->itemData(i).value<BtDeviceInfo>();
				if (device.address.compare(savedDevice.address, Qt::CaseInsensitive) == 0) {
					matchIndex = i;
					break;
				}
			}
			if (matchIndex < 0) {
				ui->cbxBtsiAddress->addItem(
					btsiDisplayText(savedDevice),
					QVariant::fromValue(savedDevice));
				matchIndex = ui->cbxBtsiAddress->count() - 1;
			}
			ui->cbxBtsiAddress->setCurrentIndex(matchIndex);
		}
	}

	{
		auto *cbx = ui->cbxCardCheckType;
		cbx->setCurrentIndex(-1);
		for (int i = 0; i < cbx->count(); ++i) {
			if(cbx->itemData(i).toString() == settings.cardCheckType()) {
				cbx->setCurrentIndex(i);
				break;
			}
		}
		if(cbx->currentIndex() < 0) {
			cbx->setCurrentIndex(0);
			settings.setCardCheckType(cbx->currentData().toString());
		}
	}
	{
		auto *cbx = ui->cbxReaderMode;
		for (int i = 0; i < cbx->count(); ++i) {
			cbx->setCurrentIndex(-1);
			if(cbx->itemData(i).toString() == settings.readerMode()) {
				cbx->setCurrentIndex(i);
				break;
			}
		}
		if(cbx->currentIndex() < 0) {
			cbx->setCurrentIndex(0);
			settings.setReaderMode(cbx->currentData().toString());
		}
	}

}

void CardReaderSettingsPage::save()
{
	CardReaderSettings settings;

	settings.setSerialEnabled(ui->grpSerial->isChecked());
	settings.setBtEnabled(ui->grpBt->isChecked());
	settings.setDevice(ui->lstDevice->currentText());
	settings.setBaudRate(ui->lstBaudRate->currentText().toInt());
	settings.setDataBits(ui->lstDataBits->currentText().toInt());
	settings.setStopBits(ui->lstStopBits->currentText().toInt());
	settings.setParity(ui->lstParity->currentText());
	settings.setShowRawComData(ui->chkShowRawComData->isChecked());
	settings.setDisableCRCCheck(ui->chkDisableCRCCheck->isChecked());
	{
		QVariantMap btDevice;
		const int ix = ui->cbxBtsiAddress->currentIndex();
		if (ix >= 0) {
			const auto device = ui->cbxBtsiAddress->itemData(ix).value<BtDeviceInfo>();
			btDevice.insert(QStringLiteral("address"), device.address);
			btDevice.insert(QStringLiteral("name"), device.name);
		}
		settings.setBtsiDevice(btDevice);
	}

	settings.setCardCheckType(ui->cbxCardCheckType->currentData().toString());
	settings.setReaderMode(ui->cbxReaderMode->currentData().toString());
}



void CardReaderSettingsPage::onScanBtClicked()
{
	ui->btScanBt->setEnabled(false);
	ui->btScanBt->setText(tr("Scanning..."));
	ui->cbxBtsiAddress->clear();

	if (m_btDiscoveryAgent) {
		m_btDiscoveryAgent->stop();
		m_btDiscoveryAgent->deleteLater();
	}
	m_btDiscoveryAgent = new QBluetoothDeviceDiscoveryAgent(this);
	m_btDiscoveryAgent->setLowEnergyDiscoveryTimeout(10000);

	connect(m_btDiscoveryAgent, &QBluetoothDeviceDiscoveryAgent::finished, this, [this]() {
		ui->btScanBt->setEnabled(true);
		ui->btScanBt->setText(tr("Scan"));

		const auto devices = m_btDiscoveryAgent->discoveredDevices();
		QList<QBluetoothDeviceInfo> bleDevices;
		for (const auto &dev : devices) {
			if (dev.coreConfigurations() & QBluetoothDeviceInfo::LowEnergyCoreConfiguration)
				bleDevices.append(dev);
		}
		std::ranges::stable_sort(bleDevices, [](const QBluetoothDeviceInfo &a, const QBluetoothDeviceInfo &b) {
			return a.name().startsWith(QLatin1String("Reader BT")) > b.name().startsWith(QLatin1String("Reader BT"));
		});
		const QString current = ui->cbxBtsiAddress->currentText();
		ui->cbxBtsiAddress->clear();
		for (const auto &bt_info : bleDevices) {
			const BtDeviceInfo device_info{bt_info.address().toString(), bt_info.name()};
			ui->cbxBtsiAddress->addItem(
				btsiDisplayText(device_info),
				QVariant::fromValue(device_info));
		}
		ui->cbxBtsiAddress->setCurrentText(current);
	});

	connect(m_btDiscoveryAgent, &QBluetoothDeviceDiscoveryAgent::errorOccurred, this,
		[this](QBluetoothDeviceDiscoveryAgent::Error error) {
			Q_UNUSED(error)
			ui->btScanBt->setEnabled(true);
			ui->btScanBt->setText(tr("Scan"));
			QMessageBox::warning(this, tr("Scan"), tr("Bluetooth scan error: %1").arg(m_btDiscoveryAgent->errorString()));
		});

	m_btDiscoveryAgent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
}

void CardReaderSettingsPage::onTestConnectionClicked()
{
	QString device = ui->lstDevice->currentText();
	int baud_rate = ui->lstBaudRate->currentText().toInt();
	int data_bits = ui->lstDataBits->currentText().toInt();
	int stop_bits = ui->lstStopBits->currentText().toInt();
	QString parity = ui->lstParity->currentText();
	auto *comport = new siut::CommPort();
	if(comport->openComm(device, baud_rate, data_bits, parity, stop_bits > 1)) {
		auto *progress = new QProgressDialog(tr("Loading SI station info ..."), tr("Cancel"), 0, 0, this);
		progress->setWindowModality(Qt::WindowModal);
		progress->setMinimumDuration(0);
		progress->setValue(0);
		auto *sidriver = new siut::DeviceDriver();
		connect(comport, &siut::CommPort::readyRead, this, [comport, sidriver]() {
			QByteArray ba = comport->readAll();
			sidriver->processData(ba);
		});
		connect(sidriver, &siut::DeviceDriver::dataToSend, comport, &siut::CommPort::sendData);
		auto *cmd = new siut::SiTaskStationConfig();
		connect(cmd, &siut::SiTaskStationConfig::finished, this, [this, comport, sidriver, progress](bool ok, QVariant result) {
			if(ok) {
				siut::SiStationConfig cfg(result.toMap());
				QString msg = cfg.toString();
				QMessageBox::information(this, tr("Information"), tr("SI reader config:%1").arg(msg));
			}
			else {
				QMessageBox::warning(this, tr("Warning"), tr("Device %1 is not SI reader").arg(comport->portName()));
			}
			progress->deleteLater();
			sidriver->deleteLater();
			comport->deleteLater();
		}, Qt::QueuedConnection);
		sidriver->setSiTask(cmd);
		auto *timer = new QTimer(progress);
		connect(timer, &QTimer::timeout, progress, [progress, cmd]() {
			if(progress->wasCanceled())
				cmd->abort();
		});
		timer->start(500);
	}
	else {
		QString error_msg = comport->errorToUserHint();
		QMessageBox::warning(this, tr("Warning"), tr("Error open device %1 - %2").arg(device, error_msg));
		delete comport;
	}
}

}
