#include "runssettingspage.h"
#include "ui_runssettingspage.h"

#include "runstablemodel.h"
#include "runstablewidget.h"
#include "runsplugin.h"
#include "runswidget.h"

#include <qf/gui/framework/mainwindow.h>
#include <qf/gui/framework/plugin.h>
#include <qf/gui/tableview.h>

#include <QHeaderView>
#include <QListWidget>

namespace Runs {

namespace {

RunsTableWidget *runsTableWidget()
{
	auto runs_plugin = qf::gui::framework::getPlugin<RunsPlugin>();
	auto *runs_widget = runs_plugin->runsWidget();
	Q_ASSERT(runs_widget);
	return runs_widget->findChild<RunsTableWidget*>();
}

}

RunsSettingsPage::RunsSettingsPage(QWidget *parent)
	: Super(parent)
{
	m_caption = tr("Runs");
	m_requiresOpenEvent = true;
	ui =new ::Ui::RunsSettingsPage;
	ui->setupUi(this);

	auto *model = runsTableWidget()->runsModel();
	for(int column = 0; column < model->columnCount({}); ++column) {
		QString col_name = model->headerData(column, Qt::Horizontal).toString();
		auto *item = new QListWidgetItem(col_name);
		item->setData(Qt::UserRole, column);
		item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsDragEnabled);
		item->setCheckState(Qt::Checked);
		ui->lwColumns->addItem(item);
	}
}

RunsSettingsPage::~RunsSettingsPage()
{
	delete ui;
}

void RunsSettingsPage::load()
{
	// Default: all visible
	for(int i = 0; i < ui->lwColumns->count(); ++i) {
		ui->lwColumns->item(i)->setCheckState(Qt::Checked);
	}

	auto *runs_table = runsTableWidget();
	if(!runs_table)
		return;

	auto *hh = runs_table->tableView()->horizontalHeader();
	const int count = ui->lwColumns->count();

	// Reorder list items to match the current visual column order in the header view
	for(int target_visual = 0; target_visual < count; ++target_visual) {
		const int logical = hh->logicalIndex(target_visual);
		// Find the item that owns this logical index (search from target_visual onward)
		for(int i = target_visual; i < count; ++i) {
			if(ui->lwColumns->item(i)->data(Qt::UserRole).toInt() == logical) {
				if(i != target_visual) {
					auto *item = ui->lwColumns->takeItem(i);
					ui->lwColumns->insertItem(target_visual, item);
				}
				break;
			}
		}
	}

	// Set check state based on current header visibility
	for(int i = 0; i < count; ++i) {
		auto *item = ui->lwColumns->item(i);
		const int logical = item->data(Qt::UserRole).toInt();
		item->setCheckState(hh->isSectionHidden(logical) ? Qt::Unchecked : Qt::Checked);
	}
}

void RunsSettingsPage::save()
{
	auto *runs_table = runsTableWidget();
	if(!runs_table)
		return;

	const int count = ui->lwColumns->count();

	QStringList hidden_columns;
	QStringList column_order;

	for(int i = 0; i < count; ++i) {
		const auto *item = ui->lwColumns->item(i);
		const int logical = item->data(Qt::UserRole).toInt();
		const auto field_name = runs_table->runsModel()->columnDefinition(logical).fieldName();
		column_order << field_name;
		if(item->checkState() != Qt::Checked) {
			hidden_columns << field_name;
		}
	}

	runs_table->setColumnsHidden(hidden_columns);
	runs_table->setColumnsOrder(column_order);

	RunsPlugin::saveRunsTableHiddenColumns(hidden_columns);
	RunsPlugin::saveRunsTableColumnOrder(column_order);
}

} // namespace Runs
