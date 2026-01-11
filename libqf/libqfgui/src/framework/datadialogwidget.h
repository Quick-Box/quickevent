#ifndef DATADIALOGWIDGET_H
#define DATADIALOGWIDGET_H

#include "../guiglobal.h"
#include "../datacontroller.h"
#include "dialogwidget.h"

#include "../model/datadocument.h"

namespace qf {
namespace gui {
namespace framework {

class QFGUI_DECL_EXPORT DataDialogWidget : public DialogWidget
{
	Q_OBJECT
	Q_PROPERTY(qf::gui::DataController* dataController READ dataController WRITE setDataController)
private:
	typedef DialogWidget Super;
public:
	DataDialogWidget(QWidget *parent = nullptr);
	~DataDialogWidget() Q_DECL_OVERRIDE;
public:
	qf::gui::DataController *dataController();
	void setDataController(qf::gui::DataController *dc);

	qf::gui::model::DataDocument* dataDocument(bool throw_exc = qf::core::Exception::Throw);

	virtual bool load(const QVariant &id = QVariant(), int mode = qf::gui::model::DataDocument::ModeEdit);

	bool acceptDialogDone(int result) Q_DECL_OVERRIDE;

	Q_SIGNAL void dataSaved(const QVariant &id, int mode);

	int recordEditMode();
	Q_SIGNAL void recordEditModeChanged(int record_edit_mode);
protected:
	virtual bool saveData();
	virtual bool dropData();
protected:
	qf::gui::DataController *m_dataController = nullptr;
};

}}}

#endif // DATADIALOGWIDGET_H
