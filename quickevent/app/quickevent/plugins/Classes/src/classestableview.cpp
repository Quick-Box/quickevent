#include "classesplugin.h"
#include "classestableview.h"

#include <qf/core/assert.h>
#include <qf/core/sql/transaction.h>
#include <qf/gui/dialogs/messagebox.h>
#include <qf/gui/framework/mainwindow.h>

#include <QInputDialog>

using qf::gui::framework::getPlugin;
using Classes::ClassesPlugin;

ClassesTableView::ClassesTableView(QWidget *parent)
	: Super(parent)
{

}

void ClassesTableView::insertRow()
{
	try {
		QString class_name = QInputDialog::getText(this, tr("Get text"), tr("New class name:"), QLineEdit::Normal);
		if(class_name.isEmpty())
			return;
		qf::core::sql::Transaction transaction;
		getPlugin<ClassesPlugin>()->createClass(class_name);
		transaction.commit();
		reload();
	}
	catch (qf::core::Exception &e) {
		qf::gui::dialogs::MessageBox::showException(this, e);
	}
}

void ClassesTableView::removeSelectedRows()
{
	if(!qf::gui::dialogs::MessageBox::askYesNo(this, tr("Really delete all selected classes? This action cannot be undone!"), false))
		return;
	try {
		qf::core::sql::Transaction transaction;
		for(int i : selectedRowsIndexes()) {
			int class_id = tableRow(i).value(tableModel()->idColumnName()).toInt();
			getPlugin<ClassesPlugin>()->dropClass(class_id);
		}
		transaction.commit();
		reload();
	}
	catch (qf::core::Exception &e) {
		qf::gui::dialogs::MessageBox::showException(this, e);
	}
}
