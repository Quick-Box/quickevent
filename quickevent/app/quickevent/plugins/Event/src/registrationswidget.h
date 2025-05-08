#ifndef REGISTRATIONSWIDGET_H
#define REGISTRATIONSWIDGET_H

#include <QWidget>

namespace Ui {
class RegistrationsWidget;
}

namespace qf {
namespace qmlwidgets {
class TableView;
}}

//class QSortFilterProxyModel;

class RegistrationsWidget : public QWidget
{
	Q_OBJECT
public:
	//enum class FocusWidget {Registration};
public:
	explicit RegistrationsWidget(QWidget *parent = 0);
	~RegistrationsWidget();

	void checkModel();

	//void setFocusToWidget(FocusWidget fw);
	qf::qmlwidgets::TableView* tableView();
private:
	Ui::RegistrationsWidget *ui;
	//QSortFilterProxyModel *m_tableModel = nullptr;
};

#endif // REGISTRATIONSWIDGET_H
