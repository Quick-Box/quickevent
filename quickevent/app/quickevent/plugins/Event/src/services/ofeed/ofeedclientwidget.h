#pragma once

#include <qf/gui/framework/dialogwidget.h>

namespace Event {
namespace services {

namespace Ui {
class OFeedClientWidget;
}

class OFeedClient;

class OFeedClientWidget : public qf::gui::framework::DialogWidget
{
	Q_OBJECT

	using Super = qf::gui::framework::DialogWidget;
public:
	explicit OFeedClientWidget(QWidget *parent = nullptr);
	~OFeedClientWidget();
private:
	void onBtExportResultsXml30Clicked();
	void onBtExportStartListXml30Clicked();
	void onProcessChangesOnOffButtonClicked();
	void onBtPasteSetupLinkClicked();
	void onBtTestConnectionClicked();
	void onBtRefreshEventImageClicked();
	void onBtOpenEventWebsiteClicked();
	void updateTestConnectionState();
	void syncReceiptEventLinkWithDefaults();
	QString defaultReceiptEventLink() const;
	OFeedClient* service();
	bool saveSettings();
private:
	Ui::OFeedClientWidget *ui;
	bool acceptDialogDone(int result);
	bool m_isTestConnectionRunning = false;
	bool m_isImageRefreshRunning = false;
	QString m_lastAutoReceiptEventLink;
};

}}
