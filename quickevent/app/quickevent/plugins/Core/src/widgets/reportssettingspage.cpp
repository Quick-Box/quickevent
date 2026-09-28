#include "reportssettingspage.h"
#include "reportstablemodel.h"
#include "ui_reportssettingspage.h"
// #include "../reportssettings.h"

#include <qf/core/log.h>
#include <qf/gui/framework/plugin.h>
#include <qf/gui/framework/reportfilecache.h>
#include <qf/gui/framework/mainwindow.h>
#include <qf/gui/style.h>

#include <QFileDialog>
#include <QHeaderView>
#include <QSettings>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QMouseEvent>
#include <QStyle>
#include <QStyledItemDelegate>

#include <functional>
#include <utility>

namespace Core {

namespace {

class SaveButtonDelegate : public QStyledItemDelegate
{
public:
	explicit SaveButtonDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}

	using ClickHandler = std::function<void(const QModelIndex &)>;
	void setClickHandler(ClickHandler handler) { m_clickHandler = std::move(handler); }

	void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
	{
		QStyleOptionButton button;
		button.rect = option.rect.adjusted(2, 2, -2, -2);
		button.state = QStyle::State_Enabled;
		button.text = index.data().toString();
		option.widget->style()->drawControl(QStyle::CE_PushButton, &button, painter, option.widget);
	}

	bool editorEvent(QEvent *event, QAbstractItemModel *, const QStyleOptionViewItem &option, const QModelIndex &index) override
	{
		if (event->type() == QEvent::MouseButtonRelease) {
			auto *mouse_event = static_cast<QMouseEvent *>(event);
			if (option.rect.contains(mouse_event->position().toPoint()) && m_clickHandler)
				m_clickHandler(index);
		}
		return true;
	}

private:
	ClickHandler m_clickHandler;
};

}

ReportsSettingsPage::ReportsSettingsPage(QWidget *parent) :
	Super(parent),
	ui(new Ui::ReportsSettingsPage),
	m_reportModel(new ReportsTableModel(this)),
	m_reportProxyModel(new ::QSortFilterProxyModel(this))
{
	m_caption = tr("Reports");
	ui->setupUi(this);

	ui->btResizeColumnsToFit->setIcon(qf::gui::Style::icon("zoom_fitwidth"));

	connect(ui->btResizeColumnsToFit, &QPushButton::clicked, this, &ReportsSettingsPage::resizeTableColumnsToFit);

	m_reportProxyModel->setSourceModel(m_reportModel);
	m_reportProxyModel->setSortRole(Qt::DisplayRole);
	ui->tblReportFiles->setModel(m_reportProxyModel);
	auto *save_delegate = new SaveButtonDelegate(ui->tblReportFiles);
	save_delegate->setClickHandler([this](const QModelIndex &index) {
		if (!index.isValid())
			return;
		const QModelIndex source_index = m_reportProxyModel->mapToSource(index);
		const auto &report = m_reportModel->reportAt(source_index.row());
		QString error_text;
		if (!m_reportModel->saveReportToDb(source_index, &error_text)) {
			QMessageBox::warning(this, tr("Save report"), tr("Failed to save report '%1' to the database:\n%2").arg(report.relativePath, error_text));
			return;
		}
		QMessageBox::information(this, tr("Save report"), tr("Report '%1' was saved to the database.").arg(report.relativePath));
	});
	ui->tblReportFiles->setItemDelegateForColumn(ReportsTableModel::SaveToDbColumn, save_delegate);
	ui->tblReportFiles->setEditTriggers(QAbstractItemView::NoEditTriggers);
	ui->tblReportFiles->setSelectionBehavior(QAbstractItemView::SelectRows);
	ui->tblReportFiles->setSelectionMode(QAbstractItemView::SingleSelection);

	ui->tblReportFiles->setSortingEnabled(true);
	ui->tblReportFiles->horizontalHeader()->setStretchLastSection(true);

	connect(ui->btClearLocalChanges, &QPushButton::clicked, this, [this]() {
		qf::gui::framework::Plugin::reportFileCache()->clearLocalChanges();
		loadModel();
	});
}

ReportsSettingsPage::~ReportsSettingsPage()
{
	delete ui;
}

void ReportsSettingsPage::resizeTableColumnsToFit()
{
	ui->tblReportFiles->resizeColumnsToContents();
}

void ReportsSettingsPage::load()
{
	const auto dir = qf::gui::framework::Plugin::reportFileCache()->effectiveReportsDir();
	ui->edReportsDirectory->setText(dir);
	loadModel();
	ui->tblReportFiles->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);
}

void ReportsSettingsPage::save()
{
	//ReportsSettings settings;
	// nothing to save for now
}

void ReportsSettingsPage::loadModel()
{
	ui->tblReportFiles->setSortingEnabled(false);
	m_reportModel->load();
	ui->tblReportFiles->setSortingEnabled(true);
}

}
