#ifndef QF_GUI_FRAMEWORK_DIALOGWIDGET_H
#define QF_GUI_FRAMEWORK_DIALOGWIDGET_H

#include "../guiglobal.h"
#include "ipersistentsettings.h"
#include "../frame.h"

#include <qf/core/utils.h>

#include <QWidget>

namespace qf {
namespace gui {

class Action;

namespace dialogs {
class Dialog;
}

namespace framework {

class QFGUI_DECL_EXPORT DialogWidget : public Frame, public IPersistentSettings
{
	Q_OBJECT

	Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
	/// url or path to the icon
	Q_PROPERTY(QString iconSource READ iconSource WRITE setIconSource NOTIFY iconSourceChanged)
	Q_PROPERTY(QString persistentSettingsId READ persistentSettingsId WRITE setPersistentSettingsId)
private:
	typedef Frame Super;
public:
	explicit DialogWidget(QWidget *parent = nullptr);
	~DialogWidget() override;

	QF_PROPERTY_IMPL(QString, t, T, itle)
	QF_PROPERTY_IMPL(QString, i, I, conSource)

	/// called when dialog wants to get close
	/// if returned value is false, done() action is cancelled
	Q_INVOKABLE virtual bool acceptDialogDone(int result);

	Q_SIGNAL void closeDialogRequest(int done_result);
public:
	/// define this slot to allow QML code call C++ settleDownInDialog() implementation,
	/// when settleDownInDialog() is implemented in QML and hides C++ implementations
	Q_INVOKABLE virtual void settleDownInDialog(qf::gui::dialogs::Dialog *dlg);
	Q_INVOKABLE void settleDownInDialog_qml(const QVariant &dlg);
	virtual qf::gui::Action* action(const QString &name, bool throw_exc = true);
protected:
	typedef QMap<QString, qf::gui::Action*> ActionMap;
protected:
	virtual ActionMap createActions();
	virtual ActionMap actions();
protected:
	ActionMap m_actions;
};

}}}

#endif // QF_GUI_FRAMEWORK_DIALOGWIDGET_H
