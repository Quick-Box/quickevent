#ifndef CARDREADERSETTINGSPAGE_H
#define CARDREADERSETTINGSPAGE_H

#include "../../Core/src/widgets/settingspage.h"

#include <QBluetoothDeviceInfo>
#include <QMap>
#include <QString>

class QBluetoothDeviceDiscoveryAgent;

namespace CardReader {

namespace Ui {class CardReaderSettingsPage;}

//class CardReaderWidget;

class  CardReaderSettingsPage : public Core::SettingsPage
{
	Q_OBJECT

	using Super = Core::SettingsPage;
public:
	CardReaderSettingsPage(QWidget *parent = nullptr);
	virtual ~CardReaderSettingsPage();
protected:
	void load();
	void save();
private:
	void onTestConnectionClicked();
	void onScanBtClicked();
private:
	Ui::CardReaderSettingsPage *ui;
	QBluetoothDeviceDiscoveryAgent *m_btDiscoveryAgent = nullptr;
	// QMap<QString, QBluetoothDeviceInfo> m_scannedBtSiDevices;
};
}
#endif // CARDREADERSETTINGSPAGE_H
