#pragma once

#include <qf/gui/framework/dialogwidget.h>

#include <functional>

namespace Event {
namespace services {

namespace Ui {
class OResultsClientWidget;
}

class OResultsClient;

class OResultsClientWidget : public qf::gui::framework::DialogWidget
{
	Q_OBJECT

	using Super = qf::gui::framework::DialogWidget;
public:
	explicit OResultsClientWidget(QWidget *parent = nullptr);
	~OResultsClientWidget();
private:
	void onBtExportResultsXml30Clicked();
	void onBtExportStartListXml30Clicked();
	void onBtTestApiKeyClicked();
	void runTest(const QString &key, std::function<void(bool)> onDone);
	void updateTestStatus(const QString &name);
	OResultsClient* service();
	bool saveSettings();
private:
	Ui::OResultsClientWidget *ui;
	QString m_loadedApiKey;
	bool m_testPassed = false;
	bool m_testRunning = false;
	bool acceptDialogDone(int result);
};

}}

