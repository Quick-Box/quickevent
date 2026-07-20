#ifndef PRINTAWARDSOPTIONSDIALOGWIDGET_H
#define PRINTAWARDSOPTIONSDIALOGWIDGET_H

#include <qf/gui/framework/dialogwidget.h>

#include <QDialog>

namespace Ui {
class PrintAwardsOptionsDialogWidget;
}

class PrintAwardsOptionsDialogWidget : public qf::gui::framework::DialogWidget
{
	Q_OBJECT
private:
	using Super = qf::gui::framework::DialogWidget;
public:
	explicit PrintAwardsOptionsDialogWidget(QWidget *parent = nullptr);
	~PrintAwardsOptionsDialogWidget();

	QVariantMap printOptions() const;
	void setPrintOptions(const QVariantMap &opts);
private:
	Ui::PrintAwardsOptionsDialogWidget *ui;
};

#endif
