#ifndef DRAWING_DRAWINGGANTTWIDGET_H
#define DRAWING_DRAWINGGANTTWIDGET_H

#include "classitem.h"

#include <qf/gui/framework/dialogwidget.h>

class QLineEdit;
class QCheckBox;
class QComboBox;
class QListWidgetItem;

namespace drawing {

namespace Ui {
class DrawingGanttWidget;
}

class GanttScene;

class DrawingGanttWidget : public qf::gui::framework::DialogWidget
{
	Q_OBJECT
private:
	typedef qf::gui::framework::DialogWidget Super;
public:
	explicit DrawingGanttWidget(QWidget *parent = nullptr);
	~DrawingGanttWidget() override;

	void settleDownInDialog(qf::gui::dialogs::Dialog *dlg) Q_DECL_OVERRIDE;
	bool acceptDialogDone(int result) Q_DECL_OVERRIDE;

	void load(int stage_id);
private slots:
	void onActSaveTriggered();
	void onActFindTriggered();
	void onClashesChanged(const QList<drawing::ClassClash> &clashes);
	void onClashItemActivated(QListWidgetItem *item);
private:
	void applyClashHighlight(int clash_ix);
	void reloadSelectedStart();
private:
	Ui::DrawingGanttWidget *ui;
	QLineEdit *m_edFind = nullptr;
	QCheckBox *m_cbShowClashes = nullptr;
	QComboBox *m_cbStart = nullptr;
	int m_stageId = -1;
	GanttScene *m_ganttScene;
	QList<ClassClash> m_clashes;
};

}

#endif
