#pragma once

#include <qf/gui/framework/dialogwidget.h>

class QTimer;
class CircularTimerWidget;

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
protected:
	void showEvent(QShowEvent *event) override;
	void hideEvent(QHideEvent *event) override;
private:
	void updateTimerIndicators();
	void onBtExportResultsXml30Clicked();
	void onBtExportStartListXml30Clicked();
	void onBtProcessChangesClicked();
	void onBtPasteSetupLinkClicked();
	void onBtTestConnectionClicked();
	void onBtRefreshEventImageClicked();
	void onBtOpenEventWebsiteClicked();
	void updateTestConnectionState();
	void updateCredentialStatus(bool valid);
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
	QTimer *m_uiTickTimer = nullptr;
	CircularTimerWidget *m_exportTimerIndicator = nullptr;
	CircularTimerWidget *m_credentialTimerIndicator = nullptr;
};

}}
