#include "dialog.h"
#include "internal/captionframe.h"
#include "../framework/dialogwidget.h"
#include "../framework/datadialogwidget.h"
#include "../menubar.h"
#include "../toolbar.h"
#include "../dialogbuttonbox.h"
#include "../internal/desktoputils.h"

#include <qf/core/log.h>
#include <qf/core/assert.h>

#include <QVBoxLayout>
#include <QSettings>
#include <QShowEvent>
#include <QPushButton>

using namespace qf::gui::dialogs;

Dialog::Dialog(QWidget *parent)
	: Dialog(QDialogButtonBox::NoButton, parent)
{
	qfLogFuncFrame();
}

Dialog::Dialog(QDialogButtonBox::StandardButtons buttons, QWidget *parent)
	: QDialog(parent)
	, framework::IPersistentSettings(this)
{
	qfLogFuncFrame();
	setButtons(buttons);
}

Dialog::~Dialog()
{
	savePersistentSettings();
}

void Dialog::setCentralWidget(QWidget *central_widget)
{
	if(central_widget != m_centralWidget) {
		QF_SAFE_DELETE(m_centralWidget);
		m_centralWidget = central_widget;
		if(m_centralWidget) {
			m_centralWidget->setParent(nullptr);
			m_centralWidget->setParent(this);
			auto sp = m_centralWidget->sizePolicy();
			sp.setVerticalPolicy(QSizePolicy::MinimumExpanding);
			m_centralWidget->setSizePolicy(sp);
		}
		auto *dialog_widget = qobject_cast<qf::gui::framework::DialogWidget *>(central_widget);
		if(dialog_widget) {
			connect(dialog_widget, &qf::gui::framework::DialogWidget::closeDialogRequest, this, &Dialog::done);
			QMetaObject::invokeMethod(this, "settleDownDialogWidget", Qt::QueuedConnection);
		}
		else {
			updateCaptionFrame();
			updateLayout();
		}
	}
}

void Dialog::settleDownDialogWidget()
{
	auto *dialog_widget = qobject_cast<qf::gui::framework::DialogWidget *>(m_centralWidget);
	if(dialog_widget) {
		QVariant dlg = QVariant::fromValue(this);
		bool ok = QMetaObject::invokeMethod(dialog_widget, "settleDownInDialog_qml", Q_ARG(QVariant, dlg));
		if(!ok) {
			qfWarning() << this << "Method settleDownInDialog_qml() invocation failed!";
		}
		else {
			updateCaptionFrame();
			updateLayout();
		}
	}
}

qf::gui::MenuBar* Dialog::menuBar()
{
	if(!m_menuBar) {
		m_menuBar = new MenuBar(this);
	}
	return m_menuBar;
}

qf::gui::ToolBar *Dialog::toolBar(const QString &name, bool create_if_not_exists)
{
	qf::gui::ToolBar *ret = m_toolBars.value(name);
	if(ret) {
		return ret;
	}
	if(!create_if_not_exists) {
		return nullptr;
	}
	ret = new qf::gui::ToolBar(this);
	m_toolBars[name] = ret;
	updateLayout();
	return ret;
}

int Dialog::exec()
{
	loadPersistentSettingsRecursively();
	return Super::exec();
}

void Dialog::done(int result)
{
	qfLogFuncFrame() << result;
	qf::gui::framework::DialogWidget *dw = dialogWidget();
	bool ok = true;
	if(dw) {
		ok = dw->acceptDialogDone(result);
	}
	if(ok) {
		Super::done(result);
	}
	/*
	QVariant ok = true;
	QMetaObject::invokeMethod(this, "doneRequest_qml",
							  Q_RETURN_ARG(QVariant, ok),
							  Q_ARG(QVariant, result));
	if(ok.toBool()) {
		Super::done(result);
	}
	*/
}

void Dialog::setRecordEditMode(int mode)
{
	if(m_captionFrame)
		m_captionFrame->setRecordEditMode(mode);
	if(m_dialogButtonBox) {
		auto bt_save = m_dialogButtonBox->button(QDialogButtonBox::Save);
		if(bt_save) {
			if(mode == qf::gui::model::DataDocument::ModeDelete) {
				bt_save->setText(tr("Delete"));
			}
			else {
				bt_save->setText(tr("Save"));
			}
		}
	}
}

void Dialog::loadPersistentSettings()
{
	qfLogFuncFrame();
	QString id = persistentSettingsId();
	if(id.isEmpty() && dialogWidget() && !dialogWidget()->persistentSettingsId().isEmpty()) {
		setPersistentSettingsId(dialogWidget()->persistentSettingsId() + "Dlg");
	}
	QString path = persistentSettingsPath();
	qfDebug() << "\t persistentSettingsPath:" << path;
	if(!path.isEmpty()) {
		QSettings settings;
		settings.beginGroup(path);
		const QRect geometry = settings.value("geometry").toRect();
		if(geometry.isValid()) {
			if(isSavePersistentPosition()) {
				if(auto geometry2 = qf::gui::internal::DesktopUtils::moveRectToVisibleDesktopScreen(geometry); geometry2.isValid()) {
					setGeometry(geometry2);
					return;
				}
			}
			this->resize(geometry.size());
		}
	}
}

void Dialog::savePersistentSettings()
{
	QString path = persistentSettingsPath();
	qfLogFuncFrame() << path;
	if(!path.isEmpty()) {
		QRect geometry = this->geometry();
		QSettings settings;
		settings.beginGroup(path);
		settings.setValue("geometry", geometry);
	}
}

void Dialog::updateLayout()
{
	QLayout *ly_orig = layout();
	QF_SAFE_DELETE(ly_orig);
	QBoxLayout *ly_root = new QVBoxLayout();
	ly_root->setContentsMargins(0, 0, 0, 0);
	ly_root->setSpacing(0);
	setLayout(ly_root);

	if(m_captionFrame)
		ly_root->addWidget(m_captionFrame);

	if(m_menuBar)
		ly_root->addWidget(m_menuBar);

	if(m_toolBars.count() == 1) {
		ToolBar *tb = m_toolBars.first();
		tb->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		ly_root->addWidget(tb);
	}
	else if(!m_toolBars.isEmpty()) {
		auto *ly1 = new QHBoxLayout(nullptr);
		Q_FOREACH(auto tb_name, m_toolBars.keys()) {
			ToolBar *tb = m_toolBars.value(tb_name);
			tb->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
			ly1->addWidget(tb);
		}
		ly1->addStretch();
		ly_root->addLayout(ly1);
	}

	QBoxLayout *ly = new QVBoxLayout();
	ly->setContentsMargins(4, 4, 4, 4);
	ly_root->addLayout(ly);

	if(m_centralWidget) {
		ly->addWidget(m_centralWidget);
		if(!m_centralWidget->windowTitle().isEmpty())
			setWindowTitle(m_centralWidget->windowTitle());
	}
	if(m_dialogButtonBox)
		ly->addWidget(m_dialogButtonBox);

}

qf::gui::framework::DialogWidget *Dialog::dialogWidget()
{
	auto *ret = qobject_cast<qf::gui::framework::DialogWidget *>(m_centralWidget);
	return ret;
}

void Dialog::showEvent(QShowEvent *ev)
{
	Super::showEvent(ev);
	if(!ev->spontaneous()) {
		// There are two kinds of show events: show events caused by the window system (spontaneous), and internal show events.
		// Spontaneous (QEvent::spontaneous()) show events are sent just after the window system shows the window;
		// they are also sent when a top-level window is redisplayed after being iconified. Internal show events are delivered just before the widget becomes visible.
		if(ev->type() == QEvent::Show) {
			emit visibleChanged(true);
		}
		else if(ev->type() == QEvent::Hide) {
			emit visibleChanged(false);
		}
	}
}

void Dialog::updateCaptionFrame()
{
	qfLogFuncFrame() << m_centralWidget;
	auto *dialog_widget = qobject_cast<qf::gui::framework::DialogWidget *>(m_centralWidget);
	auto *data_dialog_widget = qobject_cast<qf::gui::framework::DataDialogWidget *>(m_centralWidget);
	if(dialog_widget) {
		if(!m_captionFrame) {
			m_captionFrame = new qf::gui::dialogs::internal::CaptionFrame(this);
			connect(dialog_widget, &qf::gui::framework::DialogWidget::titleChanged, m_captionFrame, &qf::gui::dialogs::internal::CaptionFrame::setText);
			if(data_dialog_widget) {
				connect(data_dialog_widget, &qf::gui::framework::DataDialogWidget::recordEditModeChanged, this, &Dialog::setRecordEditMode);
				setRecordEditMode(data_dialog_widget->recordEditMode());
				//qfInfo() << "conected:" << ok;
			}
		}
		m_captionFrame->setText(dialog_widget->title());
		m_captionFrame->setIconSource(dialog_widget->iconSource());
		m_captionFrame->setIcon(m_captionFrame->createIcon());
		if(windowTitle() == QLatin1String("Form") && !dialog_widget->title().isEmpty())
			setWindowTitle(dialog_widget->title());
	}
	else {
		QF_SAFE_DELETE(m_captionFrame);
	}
}

void Dialog::setButtonBox(DialogButtonBox *dbb)
{
	qfLogFuncFrame() << dbb;
	if(dbb != m_dialogButtonBox) {
		QF_SAFE_DELETE(m_dialogButtonBox);
		m_dialogButtonBox = dbb;
		if(m_dialogButtonBox) {
			qfDebug() << "\t adding:" << m_dialogButtonBox << "to layout:" << layout();
			/// widget cannot be simply reparented
			/// NULL parent should be set first
			m_dialogButtonBox->setParent(nullptr);
			m_dialogButtonBox->setParent(this);
			updateLayout();
			connect(m_dialogButtonBox, &DialogButtonBox::accepted, this, &Dialog::accept);
			connect(m_dialogButtonBox, &DialogButtonBox::rejected, this, &Dialog::reject);
		}
		emit buttonBoxChanged();
	}
}

void Dialog::setButtons(QDialogButtonBox::StandardButtons buttons)
{
	DialogButtonBox *bbx = buttonBox();
	if(!bbx) {
		bbx = new DialogButtonBox();
		setButtonBox(bbx);
	}
	bbx->setStandardButtons(buttons);
}

void Dialog::setDefaultButton(int standard_button)
{
	QPushButton *bt = nullptr;
	DialogButtonBox *bbx = buttonBox();
	if(bbx) {
		bt = bbx->button((QDialogButtonBox::StandardButton)standard_button);
		if(bt) {
			bt->setDefault(true);
			bt->setFocus();
		}
	}
	QF_CHECK(bt != nullptr, QString("Cannot find standard button: %1").arg(standard_button));
}

