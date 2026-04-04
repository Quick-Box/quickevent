#pragma once

#include "../../Core/src/widgets/settingspage.h"
#include <QString>
#include <QVariantMap>

namespace Receipts {

namespace Ui {class ReceiptsSettingsPage;}

class  ReceiptsSettingsPage : public Core::SettingsPage
{
	Q_OBJECT

	using Super = Core::SettingsPage;
public:
	ReceiptsSettingsPage(QWidget *parent = nullptr);
	virtual ~ReceiptsSettingsPage();
protected:
	void load();
	void save();
private:
	QVariantMap currentTestReceiptData() const;
	void loadReceptList();
	void updateReceiptsPrinterLabel();
	void updateReceiptMediaControls();
	void onPrinterOptionsClicked();
	void onTestPrintClicked();
	void onSelectReceiptImageClicked();
	void onClearReceiptImageClicked();
	QString eventConfigKey(const QString &suffix) const;
private:
	Ui::ReceiptsSettingsPage *ui;
	int m_stageId = 1;
	QString m_receiptImageBase64;
	QString m_receiptImageFormat;
};

}

