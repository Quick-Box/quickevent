#ifndef QF_GUI_DIALOGS_DIALOG_H
#define QF_GUI_DIALOGS_DIALOG_H

#include "../guiglobal.h"
#include "../framework/ipersistentsettings.h"
#include "../menubar.h"
#include "../toolbar.h"

#include <qf/core/utils.h>

#include <QDialog>
#include <QDialogButtonBox>

class QLabel;
class QToolButton;

namespace qf::gui {

class DialogButtonBox;

namespace framework { class DialogWidget; }

namespace dialogs {

namespace internal { class CaptionFrame; }

class QFGUI_DECL_EXPORT Dialog : public QDialog, public framework::IPersistentSettings
{
	Q_OBJECT
	Q_PROPERTY(QString persistentSettingsId READ persistentSettingsId WRITE setPersistentSettingsId)
	Q_PROPERTY(qf::gui::DialogButtonBox* buttonBox READ buttonBox WRITE setButtonBox NOTIFY buttonBoxChanged)
	Q_PROPERTY(bool savePersistentPosition READ isSavePersistentPosition WRITE setSavePersistentPosition NOTIFY savePersistentPositionChanged)
private:
	typedef QDialog Super;
public:
	enum DoneResult {ResultReject = Rejected, ResultAccept = Accepted, ResultDelete = 2};
public:
	explicit Dialog(QWidget *parent = nullptr);
	explicit Dialog(QDialogButtonBox::StandardButtons buttons, QWidget *parent = nullptr);
	~Dialog() Q_DECL_OVERRIDE;

	QF_PROPERTY_BOOL_IMPL2(s, S, avePersistentPosition, true)
public:
	void setCentralWidget(QWidget *central_widget);

	Q_INVOKABLE MenuBar* menuBar();
	Q_INVOKABLE ToolBar* toolBar(const QString &name, bool create_if_not_exists = false);

	DialogButtonBox* buttonBox() {return m_dialogButtonBox;}
	void setButtonBox(DialogButtonBox *dbb);
	Q_SIGNAL void buttonBoxChanged();

	void setButtons(QDialogButtonBox::StandardButtons buttons);
	Q_INVOKABLE void setDefaultButton(int standard_button);

	Q_SLOT void loadPersistentSettings();

	int exec() Q_DECL_OVERRIDE;
	void done(int result) Q_DECL_OVERRIDE;

	Q_SLOT void setRecordEditMode(int mode);

	Q_SIGNAL void visibleChanged(bool visible);
private:
	Q_SLOT void savePersistentSettings();

	Q_SLOT void settleDownDialogWidget();
protected:
	void updateCaptionFrame();
	void updateLayout();

	qf::gui::framework::DialogWidget* dialogWidget();

	void showEvent(QShowEvent *ev) Q_DECL_OVERRIDE;
protected:
	bool m_doneCancelled;
	internal::CaptionFrame *m_captionFrame = nullptr;
	MenuBar *m_menuBar = nullptr;
	QMap<QString, ToolBar*> m_toolBars;
	QWidget *m_centralWidget = nullptr;
	DialogButtonBox *m_dialogButtonBox = nullptr;
};

}}

#endif // QF_GUI_DIALOGS_DIALOG_H
