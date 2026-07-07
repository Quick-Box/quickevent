#pragma once

#include <qf/gui/framework/dialogwidget.h>

namespace Event {
namespace services {

namespace Ui {
	class PunchingTestServiceWidget;
}

class PunchingTestService;

class PunchingTestServiceWidget : public qf::gui::framework::DialogWidget
{
	Q_OBJECT
	using Super = qf::gui::framework::DialogWidget;
public:
	explicit PunchingTestServiceWidget(QWidget *parent = nullptr);
	~PunchingTestServiceWidget() override;

private:
	bool acceptDialogDone(int result) override;
	PunchingTestService *service();
	void saveSettings();

private:
	Ui::PunchingTestServiceWidget *ui;
};

}} // namespace Event::services
