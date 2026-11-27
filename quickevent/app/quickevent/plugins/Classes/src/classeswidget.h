#ifndef CLASSESWIDGET_H
#define CLASSESWIDGET_H

#include <QFrame>

#include "partwidget.h"

class QAction;
class QComboBox;

namespace Ui {
	class ClassesWidget;
}

namespace qf::gui { class ForeignKeyComboBox; }
namespace qf::gui::model { class SqlTableModel; }

namespace quickevent { namespace core { class CodeDef; }}

class CourseItemDelegate;
struct ImportCourseDef;

class ClassesWidget : public QFrame
{
	Q_OBJECT
private:
	typedef QFrame Super;
public:
	explicit ClassesWidget(QWidget *parent = nullptr);
	~ClassesWidget() Q_DECL_OVERRIDE;

	Q_INVOKABLE int selectedStageId();

	void settleDownInPartWidget(::PartWidget *part_widget);
private:
	void edit_courses();
	void edit_codes();
	void edit_classes_layout();

	void import_ocad_txt();
	void import_ocad_v8();
	void import_ocad_iofxml_2();
	void import_ocad_iofxml_3();

	Q_SLOT void reset();
	Q_SLOT void reload();
	Q_SLOT void reloadCourseCodes();

	void setDrawLockForAllClasses(bool is_locked);
	void updateDrawLockAction();
private:
	void importCourses(const QList<ImportCourseDef> &course_defs, const QList<quickevent::core::CodeDef> &code_defs);
private:
	Ui::ClassesWidget *ui;
	qf::gui::model::SqlTableModel *m_classesModel;
	qf::gui::model::SqlTableModel *m_courseCodesModel;
	QComboBox *m_cbxStage = nullptr;
	CourseItemDelegate *m_courseItemDelegate = nullptr;
	QAction *m_actDrawLockAll = nullptr;
};

#endif // CLASSESWIDGET_H
