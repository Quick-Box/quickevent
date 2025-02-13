#pragma once

#include <qf/qmlwidgets/framework/dialogwidget.h>

namespace Event {
namespace services {

namespace Ui {
class OFeedClientWidget;
}

class OFeedClient;

class OFeedClientWidget : public qf::qmlwidgets::framework::DialogWidget
{
	Q_OBJECT

	using Super = qf::qmlwidgets::framework::DialogWidget;
public:
    explicit OFeedClientWidget(QWidget *parent = nullptr);
    ~OFeedClientWidget();
private:
	void onBtExportResultsXml30Clicked();
	void onBtExportStartListXml30Clicked();
    OFeedClient* service();
	bool saveSettings();
private:
    Ui::OFeedClientWidget *ui;
	bool acceptDialogDone(int result);
};

}}
