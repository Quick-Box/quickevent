#pragma once

#include <QDialog>

namespace Event {
namespace services {

namespace Ui {
class OFeedWelcomeDialog;
}

class OFeedWelcomeDialog : public QDialog
{
	Q_OBJECT

	using Super = QDialog;
public:
	explicit OFeedWelcomeDialog(QWidget *parent = nullptr);
	~OFeedWelcomeDialog();

private:
	Ui::OFeedWelcomeDialog *ui;
};

}}
