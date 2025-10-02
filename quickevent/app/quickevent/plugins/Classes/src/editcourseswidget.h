#ifndef EDITCOURSESWIDGET_H
#define EDITCOURSESWIDGET_H

#include <qf/gui/framework/dialogwidget.h>

namespace Ui {
class EditCoursesWidget;
}

namespace qf::gui::model { class SqlTableModel; }

class EditCoursesWidget : public qf::gui::framework::DialogWidget
{
	Q_OBJECT
private:
	typedef qf::gui::framework::DialogWidget Super;
public:
	explicit EditCoursesWidget(int stage_id, QWidget *parent = 0);
	~EditCoursesWidget() Q_DECL_OVERRIDE;

	void editCourseCodes(const QModelIndex &ix);
private:
	Ui::EditCoursesWidget *ui;
	qf::gui::model::SqlTableModel *m_coursesModel;
};

#endif // EDITCOURSESWIDGET_H
