#pragma once

#include <qf/gui/framework/dialogwidget.h>

namespace Ui {
class PrintRelayAwardsOptionsDialogWidget;
}

class PrintRelayAwardsOptionsDialogWidget : public qf::gui::framework::DialogWidget
{
	Q_OBJECT
private:
	using Super = qf::gui::framework::DialogWidget;
public:
	explicit PrintRelayAwardsOptionsDialogWidget(QWidget *parent = nullptr);
	~PrintRelayAwardsOptionsDialogWidget() override;

	QVariantMap printOptions() const;
	void setPrintOptions(const QVariantMap &opts);

	void refreshTemplateList();

private:
	void onDesignerClicked();

	Ui::PrintRelayAwardsOptionsDialogWidget *ui;
};
