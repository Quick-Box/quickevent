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
	OFeedClient* service();
	bool saveSettings();
private:
	Ui::OFeedClientWidget *ui;
	bool acceptDialogDone(int result);
};

}}
