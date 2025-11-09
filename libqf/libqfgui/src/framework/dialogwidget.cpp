#include "dialogwidget.h"

#include "../action.h"
#include "../dialogs/dialog.h"

#include <qf/core/exception.h>
#include <qf/core/log.h>
#include <qf/core/assert.h>

using namespace qf::gui::framework;

DialogWidget::DialogWidget(QWidget *parent)
	: Super(parent)
	, IPersistentSettings(this)
{
}

DialogWidget::~DialogWidget() = default;

bool DialogWidget::acceptDialogDone(int result)
{
	qfLogFuncFrame();
	Q_UNUSED(result);
	return true;
}

void DialogWidget::settleDownInDialog_qml(const QVariant &dlg)
{
	auto *o = dlg.value<QObject*>();
	auto *pdlg = qobject_cast<qf::gui::dialogs::Dialog *>(o);
	QF_ASSERT(pdlg != nullptr, "Invalid dialog", return);
	settleDownInDialog(pdlg);
}

void DialogWidget::settleDownInDialog(qf::gui::dialogs::Dialog *dlg)
{
	Q_UNUSED(dlg);
}

qf::gui::Action* DialogWidget::action(const QString &name, bool throw_exc)
{
	qfLogFuncFrame() << "name:" << name;
	qf::gui::Action *ret = actions().value(name);
	if(!ret && throw_exc)
		QF_EXCEPTION(tr("Cannot find action name '%1'").arg(name));
	return ret;
}

DialogWidget::ActionMap DialogWidget::createActions()
{
	return ActionMap();
}

DialogWidget::ActionMap DialogWidget::actions()
{
	qfLogFuncFrame();
	if(m_actions.isEmpty()) {
		m_actions = createActions();
	}
	return m_actions;
}

