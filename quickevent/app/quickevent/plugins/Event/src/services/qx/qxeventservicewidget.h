#pragma once

#include <qf/gui/framework/dialogwidget.h>

namespace Event::services::qx {

namespace Ui {
class QxEventServiceWidget;
}

class QxEventService;

class QxEventServiceWidget : public qf::gui::framework::DialogWidget
{
	Q_OBJECT

	using Super = qf::gui::framework::DialogWidget;
public:
	explicit QxEventServiceWidget(QWidget *parent = nullptr);
	~QxEventServiceWidget() override;

	bool acceptDialogDone(int result) override;
private:
	enum class MessageType { Ok, Error, Progress };
	void setMessage(const QString &msg = {}, MessageType msg_type = MessageType::Ok);
	QxEventService* service();
	bool saveSettings();
	void testConnection();
	void exportEventInfo();
	void exportStartList();
	void exportRuns();
private:
	Ui::QxEventServiceWidget *ui;
};

}

