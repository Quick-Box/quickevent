#include "settingsdialog.h"
#include "ui_settingsdialog.h"

#include "settingspage.h"

#include <qf/core/log.h>
#include <qf/gui/framework/mainwindow.h>
#include <qf/gui/framework/plugin.h>

#include <QButtonGroup>
#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <QTimer>

namespace Core {

SettingsDialog::SettingsDialog(QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::SettingsDialog)
{
	ui->setupUi(this);

	connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &SettingsDialog::onButtonBoxRejected);

	m_buttonGroup = new QButtonGroup(this);
	connect(m_buttonGroup, &QButtonGroup::idToggled, this, [this](int page_index, bool checked) {
		qfDebug() << "id toggled:" << page_index << checked;
		if(checked) {
			ui->stackedWidget->setCurrentIndex(page_index);
			page(page_index)->load();
		}
		else {
			page(page_index)->save();
		}
	});
}

SettingsDialog::~SettingsDialog()
{
	delete ui;
}

void SettingsDialog::showEvent(QShowEvent *event)
{
	QDialog::showEvent(event);
	if(int page_index = m_buttonGroup->checkedId(); page_index >= 0) {
		page(page_index)->load();
	}
}

void SettingsDialog::onButtonBoxRejected()
{
	if(int page_index = m_buttonGroup->checkedId(); page_index >= 0) {
		page(page_index)->save();
	}
	accept();
}

SettingsPage *SettingsDialog::page(int page_index)
{
	auto *page = ui->stackedWidget->widget(page_index)->findChild<SettingsPage*>(QString(), Qt::FindDirectChildrenOnly);
	Q_ASSERT(page);
	return page;
}

void SettingsDialog::addPage(SettingsPage *page)
{
	auto caption = page->caption();
	Q_ASSERT(!caption.isEmpty());
	auto *layout = qobject_cast<QBoxLayout*>(ui->buttonsWidget->layout());
	Q_ASSERT(layout);
	int page_index = m_buttonGroup->buttons().count();
	auto *btn = new QPushButton(caption);
	// set widget minimum width to show all buttons, default behavior is to srt width of widget
	// according to width of first button added
#if QT_VERSION_MAJOR >= 6
	ui->buttonsWidget->setMinimumWidth(std::max(ui->buttonsWidget->minimumWidth(), btn->sizeHint().width() + (layout->contentsMargins().left() * 5)));
#else
	ui->buttonsWidget->setMinimumWidth(std::max(ui->buttonsWidget->minimumWidth(), btn->sizeHint().width() + layout->margin() * 5));
#endif
	btn->setCheckable(true);
	layout->insertWidget(page_index, btn);
	m_buttonGroup->addButton(btn, page_index);
	auto *frame = new QFrame();
	frame->setFrameStyle(QFrame::Box);
	auto *label = new QLabel("  " + caption);
	label->setObjectName("CaptionFrame"); // important for CSS
	auto *ly = new QVBoxLayout(frame);
#if QT_VERSION_MAJOR >= 6
	ly->setContentsMargins({0, 0, 0, 0});
#else
	ly->setMargin(0);
#endif
	ly->addWidget(label);
	ly->addWidget(page);
	ui->stackedWidget->addWidget(frame);
	adjustSize();
	if(page_index == 0) {
		btn->click();
	}
}
/*
ReportsSettingsPage *SettingsDialog::reportsSettingsPage() const
{
	auto *page = findChild<ReportsSettingsPage*>();
	Q_ASSERT(page);
	return page;
}
*/
} // namespace Core
