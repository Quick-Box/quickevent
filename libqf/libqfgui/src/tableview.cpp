#include "tableview.h"

#include "tableviewproxymodel.h"
#include "headerview.h"
#include "action.h"
#include "tableitemdelegate.h"
#include "dialogs/messagebox.h"
#include "dialogbuttonbox.h"
#include "exportcsvtableviewwidget.h"
#include "style.h"
#include "texteditwidget.h"
#include "log.h"

#include "dialogs/dialog.h"
#include "dialogs/filedialog.h"

#include "reports/widgets/printtableviewwidget/printtableviewwidget.h"
#include "reports/widgets/reportviewwidget.h"

#include "internal/dlgtableviewcopyspecial.h"
#include "internal/tableviewcopytodialogwidget.h"

#include <qf/core/string.h>
#include <qf/core/collator.h>
#include <qf/core/assert.h>
#include <qf/core/exception.h>
#include <qf/core/sql/query.h>
#include <qf/gui/model/sqltablemodel.h>
#include <qf/gui/model/tablemodel.h>
#include <qf/core/utils/treetable.h>
#include <qf/core/sql/transaction.h>

#include <QSortFilterProxyModel>
#include <QKeyEvent>
#include <QMenu>
#include <QPushButton>
#include <QSettings>
#include <QJsonDocument>
#include <QApplication>
#include <QClipboard>
#include <QTextEdit>
#include <QInputDialog>
#include <QFileDialog>
#include <QPainter>
#include <QToolButton>
#include <QTextStream>
#include <algorithm>
//#define QF_TIMESCOPE_ENABLED
#include <qf/core/utils/timescope.h>

namespace qfc = qf::core;
namespace qfu = qf::core::utils;
namespace qfm = qf::gui::model;
using namespace qf::gui;

TableView::TableView(QWidget *parent) :
	Super(parent), framework::IPersistentSettings(this)
{
	qfLogFuncFrame() << this;
	setItemDelegate(new TableItemDelegate(this));
	{
		auto *h = new HeaderView(Qt::Horizontal, this);
		setHorizontalHeader(h);
		connect(this, &TableView::seekStringChanged, h, &HeaderView::setSeekString);
	}
	{
		auto *h = new HeaderView(Qt::Vertical, this);
		setVerticalHeader(h);
	}
	setSortingEnabled(true);

	createActions();
	{
		auto *style = Style::instance();
		auto *bt = new QToolButton(this);
		bt->setAutoRaise(true);
		bt->setIcon(style->icon("menu"));
		bt->setToolTip(tr("Left click selects all, right click for menu."));
		QObject::connect(bt, &QPushButton::clicked, this, &QTableView::selectAll);
		//qfInfo() << "addidng actions";
		bt->setContextMenuPolicy(Qt::ActionsContextMenu);
		QList<QAction*> lst;
		Q_FOREACH(auto a, contextMenuActionsForGroups(AllActions))
			lst << a;
		bt->addActions(lst);
		m_leftTopCornerButton = bt;
	}
	m_contextMenuActions = standardContextMenuActions();
	setContextMenuPolicy(Qt::DefaultContextMenu);

	// queued connection is important since the message box showing the exception, can steal cell editor focus and emit next postRow request,
	// which can cause second exception in the row
	connect(this, &TableView::sqlException, this, &TableView::onSqlException, Qt::QueuedConnection);

	m_proxyModel = new TableViewProxyModel(this);
	connect(m_proxyModel, &TableViewProxyModel::modelReset, this, &TableView::refreshActions);
	m_proxyModel->setDynamicSortFilter(false);
	connect(qobject_cast<HeaderView*>(horizontalHeader()), &HeaderView::sortColumnAdded, m_proxyModel, &TableViewProxyModel::addSortColumn);
	Super::setModel(m_proxyModel);
	/*
	connect(this, &TableView::readOnlyChanged, [this] (bool b) {
		setEditRowsEnabled(!b);
	});
	*/
}

TableView::~TableView()
{
	qfLogFuncFrame() << this;
	savePersistentSettings();
}

QWidget *TableView::cornerWidget() const
{
	foreach(QAbstractButton *bt, findChildren<QAbstractButton*>(QString(), Qt::FindDirectChildrenOnly)) {
		if(bt->metaObject()->className() == QString("QTableCornerButton")) { /// src/gui/itemviews/qtableview.cpp:103
			return bt;
		}
	}
	return nullptr;
}

QAbstractButton *TableView::leftTopCornerButton() const
{
	return m_leftTopCornerButton;
}

QSortFilterProxyModel *TableView::sortFilterProxyModel() const
{
	return m_proxyModel;
}

QAbstractProxyModel* TableView::lastProxyModel() const
{
	QAbstractProxyModel *ret = nullptr;
	for(auto m=Super::model(); m; ) {
		auto *pxm = qobject_cast<QAbstractProxyModel*>(m);
		if(pxm) {
			ret = pxm;
			m = pxm->sourceModel();
		}
		else
			break;
	}
	QF_ASSERT_EX(ret != nullptr, "SortFilterProxyModel must exist.");
	return ret;
}

void TableView::copySelectionToClipboard(const QTableView *table_view)
{
	copySpecial_helper(table_view, "\t", "\n", "", ReplaceEscapes::QuoteIfNeeded);
}

void TableView::setModel(QAbstractItemModel *model)
{
	Super::setModel(model);
}

qf::gui::model::TableModel *TableView::tableModel() const
{
	auto *ret = qobject_cast<qf::gui::model::TableModel *>(lastProxyModel()->sourceModel());
	return ret;
}

void TableView::setTableModel(gui::model::TableModel *m)
{
	qf::gui::model::TableModel *old_m = tableModel();
	if (old_m != m) {
		QAbstractProxyModel *pxm = lastProxyModel();
		pxm->setSourceModel(m);
		m_proxyModel->setSortRole(qf::gui::model::TableModel::SortRole);
		if(pxm->columnCount() > 0) {
			qfDebug() << persistentSettingsPath() << "Load persistent settings.";
			// it does not make sense to load column widths unless we have table columns
			loadPersistentSettings();
		}
		refreshActions();
		//emit tableModelChanged();
	}
}

void TableView::refreshActions()
{
	qfLogFuncFrame() << "model:" << model();
	enableAllActions(false);
	auto *m = tableModel();
	if(!m)
		return;
	action("filter")->setEnabled(true);
	action("copy")->setEnabled(true);
	action("copySpecial")->setEnabled(true);
	action("select")->setEnabled(true);
	action("reload")->setEnabled(true);
	action("resizeColumnsToContents")->setEnabled(true);
	action("resetColumnsSettings")->setEnabled(true);
	action("showCurrentCellText")->setEnabled(true);
	//action("insertRowsStatement")->setEnabled(true);
	//action("import")->setEnabled(true);
	//action("importCSV")->setEnabled(true);
	action("export")->setEnabled(true);
	action("exportReport")->setEnabled(true);
	action("exportCSV")->setEnabled(true);
	//action("exportXML")->setEnabled(true);
	//action("exportXLS")->setEnabled(true);
	//action("exportHTML")->setEnabled(true);

	//action("insertRow")->setVisible(isInsertRowActionVisible());
	//action("removeSelectedRows")->setVisible(isRemoveRowActionVisibleInExternalMode());
	//action("postRow")->setVisible(true);
	//action("revertRow")->setVisible(true);

	bool is_insert_rows_allowed = m_proxyModel->rowFilterString().isEmpty();
	is_insert_rows_allowed = is_insert_rows_allowed && !isReadOnly();
	bool is_edit_rows_allowed = true;//m->isEditRowsAllowed() && !isReadOnly();
	is_edit_rows_allowed = is_edit_rows_allowed && !isReadOnly();
	bool is_delete_rows_allowed = true;//m->rowCount()>0 && m->isDeleteRowsAllowed() && !isReadOnly();
	is_delete_rows_allowed = is_delete_rows_allowed && !isReadOnly();
	bool is_clone_rows_allowed = currentIndex().isValid() && m_proxyModel->rowFilterString().isEmpty();
	is_clone_rows_allowed = is_clone_rows_allowed && !isReadOnly();
	//qfInfo() << "\tinsert allowed:" << is_insert_rows_allowed;
	//qfDebug() << "\tdelete allowed:" << is_delete_rows_allowed;
	//qfDebug() << "\tedit allowed:" << is_edit_rows_allowed;
	//action("insertRow")->setVisible(is_insert_rows_allowed && isInsertRowActionVisible());
	Action *a_insert_row = action("insertRow");
	Action *a_remove_sel_rows = action("removeSelectedRows");
	Action *a_clone_row = action("cloneRow");
	a_clone_row->setEnabled(a_clone_row->isVisible() && is_clone_rows_allowed);
	//action("cloneRow")->setVisible(iscloneRowActionVisible());
	a_remove_sel_rows->setEnabled(a_remove_sel_rows->isVisible() && is_delete_rows_allowed);
	//action("postRow")->setVisible((is_edit_rows_allowed || is_insert_rows_allowed) && action("postRow")->isVisible());
	//action("revertRow")->setVisible(action("postRow")->isVisible() && action("revertRow")->isVisible());
	//action("editRowExternal")->setVisible(is_edit_rows_allowed && action("editRowExternal")->isVisible());

	QModelIndex curr_ix = currentIndex();
	qfu::TableRow curr_row;
	if(curr_ix.isValid())
		curr_row = m->tableRow(toTableModelRowNo(curr_ix.row()));
	//qfDebug() << QF_FUNC_NAME << "valid:" << r.isValid() << "dirty:" << r.isDirty();
	if(curr_row.isDirty()) {
		action("postRow")->setEnabled(true);
		action("revertRow")->setEnabled(true);
	}
	else {
		a_insert_row->setEnabled(a_insert_row->isVisible() && is_insert_rows_allowed);
		a_clone_row->setEnabled(a_clone_row->isVisible() && is_clone_rows_allowed && curr_ix.isValid());
		action("reload")->setEnabled(true);
		//action("sortAsc")->setEnabled(true);
		//action("sortDesc")->setEnabled(true);
		//action("filter")->setEnabled(true);
		//action("addColumnFilter")->setEnabled(true);
		//action("removeColumnFilter")->setEnabled(true);
		//action("deleteColumnFilters")->setEnabled(true);
		action("setValueInSelection")->setEnabled(true);
		action("setNullInSelection")->setEnabled(true);
		action("generateSequenceInSelection")->setEnabled(true);
		action("paste")->setEnabled(is_edit_rows_allowed && is_insert_rows_allowed);
	}
	action("revertRow")->setEnabled(action("postRow")->isEnabled());
	action("saveCurrentCellBlob")->setEnabled(true);
	action("loadCurrentCellBlob")->setEnabled(is_edit_rows_allowed);
}

void TableView::resetColumnsSettings()
{
	QHeaderView *hh = horizontalHeader();
	if(!hh)
		return;
	for (int i = 0; i < hh->count(); ++i) {
		int vi = hh->visualIndex(i);
		if(vi != i) {
			hh->moveSection(vi, i);
		}
	}
	resizeColumnsToContents();
}

void TableView::reload(bool preserve_sorting)
{
	qfLogFuncFrame() << "preserve_sorting:" << preserve_sorting;
	int sort_column = -1;
	Qt::SortOrder sort_order(Qt::AscendingOrder);
	if(horizontalHeader()) {
		//savePersistentSettings();
		sort_column = horizontalHeader()->sortIndicatorSection();
		sort_order = horizontalHeader()->sortIndicatorOrder();
		horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);
	}
	qf::gui::model::TableModel *table_model = tableModel();
	if(table_model) {
		QModelIndex ix = currentIndex();
		int r = ix.row();
		int c = ix.column();
		table_model->reload();
		//qfDebug() << "\t emitting reloaded()";
		//emit reloaded();
		//qfDebug() << "\ttable:" << table();
		ix = model()->index(r, c);
		setCurrentIndex(ix);
		//updateDataArea();
	}
	if(horizontalHeader() && preserve_sorting) {
		if(sort_column >= 1) {
			horizontalHeader()->setSortIndicator(sort_column, sort_order);
		}
	}
	refreshActions();
}

void TableView::enableAllActions(bool on)
{
	Q_FOREACH(auto a, m_actions) {
		a->setEnabled(on);
		//if(on) a->setVisible(true);
	}
}

void TableView::setInsertRowEnabled(bool b)
{
	Action *a = action(QStringLiteral("insertRow"));
	a->setVisible(b);
}

void TableView::setRemoveRowEnabled(bool b)
{
	Action *a = action(QStringLiteral("removeSelectedRows"));
	a->setVisible(b);
}

void TableView::setCloneRowEnabled(bool b)
{
	Action *a = action(QStringLiteral("cloneRow"));
	a->setVisible(b);
}

void TableView::setEditRowsMenuSectionEnabled(bool b)
{
	setInsertRowEnabled(b);
	setRemoveRowEnabled(b);
	setCloneRowEnabled(b);
	{
		Action *a = action(QStringLiteral("editRowsSeparator"));
		a->setVisible(b);
	}
}

void TableView::setDirtyRowsMenuSectionEnabled(bool b)
{
	{
		Action *a = action(QStringLiteral("postRow"));
		a->setVisible(b);
	}
	{
		Action *a = action(QStringLiteral("revertRow"));
		a->setVisible(b);
	}
}

void TableView::setReadOnly(bool ro)
{
	if(ro == isReadOnly()) {
		return;
	}
	m_isReadOnly = ro;

	setEditRowsMenuSectionEnabled(!ro);
	setDirtyRowsMenuSectionEnabled(!ro);

	emit readOnlyChanged(ro);
}

void TableView::insertRow()
{
	qfLogFuncFrame();
	try {
		if(rowEditorMode() == EditRowsInline) {
			qfDebug() << "\t insert row in mode RowEditorInline";
			insertRowInline();
		}
		else {
			qfDebug() << "\t emit insertRowInExternalEditor()";
			emit editRowInExternalEditor(QVariant(), ModeInsert);
			//emit insertRowInExternalEditor();
		}
	}
	catch(qfc::Exception &e) {
		emit sqlException(e.message(), e.where(), e.stackTrace());
	}
	refreshActions();
}

void TableView::cloneRowInline()
{
	qfLogFuncFrame();
	try {
		QModelIndex ix = currentIndex();
		if(ix.row() < 0)
			return;
		int tri1 = toTableModelRowNo(ix.row());
		gui::model::TableModel *tm = tableModel();
		tm->cloneRow(tri1);
		m_proxyModel->sort();
		setCurrentIndex(m_proxyModel->index(ix.row() + 1, ix.column()));
	}
	catch(qfc::Exception &e) {
		emit sqlException(e.message(), e.where(), e.stackTrace());
	}
}

void TableView::cloneRow()
{
	qfLogFuncFrame();
	if(rowEditorMode() == EditRowsInline) {
		qfDebug() << "\t copy row in mode EditRowsInline";
		cloneRowInline();
	}
	else {
		QVariant id = selectedRow().value(tableModel()->idColumnName());
		qfDebug() << "\t emit editRowInExternalEditor(ModeCopy)";
		emit editRowInExternalEditor(id, ModeCopy);
		emit editSelectedRowsInExternalEditor(ModeCopy);
	}
	refreshActions();
}

void TableView::removeSelectedRows()
{
	qfLogFuncFrame();
	try {
		if(rowEditorMode() == EditRowsInline) {
			removeSelectedRowsInline();
		}
		else {
			QList<int> sel_rows = selectedRowsIndexes();
			if(sel_rows.count() == 1) {
				QVariant id = tableRow(sel_rows.value(0)).value(tableModel()->idColumnName());
				if(id.isValid())
					emit editRowInExternalEditor(id, ModeDelete);
			}
			emit editSelectedRowsInExternalEditor(ModeDelete);
		}
	}
	catch(qfc::Exception &e) {
		emit sqlException(e.message(), e.where(), e.stackTrace());
	}
}

bool TableView::postRowImpl(int row_no)
{
	qfLogFuncFrame() << row_no;
	if(row_no < 0)
		row_no = currentIndex().row();
	if(row_no < 0)
		return false;
	bool ret = false;
	auto *m = tableModel();
	if(m) {
		ret = m->postRow(toTableModelRowNo(row_no), true);
		if(ret)
			updateRow(row_no);
	}
	return ret;
}

bool TableView::postRow(int row_no)
{
	qfLogFuncFrame() << row_no;
	bool ret = false;
	try {
		ret = postRowImpl(row_no);
	}
	catch(qfc::Exception &e) {
		emit sqlException(e.message(), e.where(), e.stackTrace());
	}
	return ret;
}

void TableView::revertRow(int row_no)
{
	qfLogFuncFrame() << row_no;
	if(row_no < 0)
		row_no = currentIndex().row();
	if(row_no < 0)
		return;
	auto *m = tableModel();
	if(m) {
		m->revertRow(toTableModelRowNo(row_no));
	}
	refreshActions();
}

int TableView::reloadRow(int row_no)
{
	qfLogFuncFrame() << row_no;
	if(row_no < 0)
		row_no = currentIndex().row();
	if(row_no < 0)
		return 0;
	auto *m = tableModel();
	if(m) {
		return m->reloadRow(toTableModelRowNo(row_no));
	}
	return 0;
}

void TableView::copy()
{
	qfLogFuncFrame();
	copySelectionToClipboard();
}

void TableView::copySpecial()
{
	internal::DlgTableViewCopySpecial dlg(this);
	if(dlg.exec()) {
		copySpecial_helper(this, dlg.fieldsSeparator(), dlg.rowsSeparator(), dlg.fieldsQuotes(), dlg.replaceEscapes());
	}
}

void TableView::paste()
{
	//if(!f_contextMenuActions.contains(action("paste"))) return;
	//if(!action("paste")->isEnabled() || !action("paste")->isVisible()) return;
	QModelIndex origin_ix = currentIndex();
	if(!origin_ix.isValid())
		return;
	try {
		dialogs::Dialog dlg(this);
		dlg.setButtons(QDialogButtonBox::Ok);
		auto *w = new internal::TableViewCopyToDialogWidget();
		dlg.setCentralWidget(w);
		int col_cnt = 0;
		{
			QClipboard *clipboard = QApplication::clipboard();
			QString text = clipboard->text().trimmed();
			if(!text.isEmpty()) {
				QList<QStringList> table;
				QStringList lines = text.split('\n');
				foreach(QString line, lines) {
					//qfInfo() << line;
					QStringList cells = line.split('\t');
					col_cnt = qMax(col_cnt, cells.count());
					table.insert(table.count(), cells);
				}
				QStringList col_names;
				for(int i=0; i<col_cnt; i++)
					col_names << QString("col%1").arg(i+1);

				qfu::Table t(col_names);
				for(int row=0; row<table.count(); row++) {
					QStringList row_sl = table[row];
					if(row_sl.count() == 0) {
						/// invalidni radek, napr. excel nechava na konci jeden prazdnej radek
						qfInfo() << "invalid row:" << row_sl.join("|");
						qfInfo() << "cell count is:" << row_sl.count() << "should be:" << col_cnt;
						continue;
					}
					/*
					else if(row_sl.count() < col_cnt) {
						/// invalidni radek, napr. excel nechava na konci jeden prazdnej radek
						qfInfo() << "invalid row:" << row_sl.join("|");
						qfInfo() << "cell count is:" << row_sl.count() << "should be:" << col_cnt;
						continue;
					}
					*/
					qfu::TableRow &r = t.appendRow();
					for(int col=0; col<col_cnt; col++) {
						QString val_str = row_sl.value(col);
						r.setValue(col, val_str);
					}
					r.clearEditFlags();
				}
				TableView *tv = w->tableView();
				auto *tm = new qfm::TableModel(tv);
				tm->setTable(t);
				tv->setTableModel(tm);
				tv->setContextMenuActions(tv->contextMenuActionsForGroups(AllActions));
				//qfInfo() << "isEditRowsAllowed:" << m->isEditRowsAllowed();
				tm->reload();
			}
		}
		if(dlg.exec()) {
			TableView *tv = w->tableView();
			qfm::TableModel *src_tm = tv->tableModel();
			qfm::TableModel *dest_tm = tableModel();
			int origin_view_row = origin_ix.row();
			bool insert_rows = w->isInsert();
			int dest_row = toTableModelRowNo(origin_view_row);
			for(int src_row=0; src_row<src_tm->rowCount(); src_row++) {
				if(insert_rows) {
					qfDebug() << "insert row:" << dest_row << "model row count:" << dest_tm->rowCount();
					dest_tm->insertRow(++dest_row);
				}
				else {
					if((origin_view_row + src_row) >= dest_tm->rowCount())
						break;
					dest_row = toTableModelRowNo(origin_view_row + src_row);
				}
				int dest_col = origin_ix.column();
				for(int src_col=0; src_col<src_tm->columnCount(); src_col++) {
					if(tv->isColumnHidden(src_col))
						continue; /// preskakej skryty sloupce v tabulce dialogu
					while(isColumnHidden(dest_col))
						dest_col++; /// preskakej skryty sloupce v tabulce do ktery se vklada
					QModelIndex src_ix = src_tm->index(src_row, src_col); /// odsud se to bere
					QModelIndex dest_ix = dest_tm->index(dest_row, dest_col); ///sem se to vklada
					//qfInfo() << "ix:" << ix.row() << '\t' << ix.column();
					//qfInfo() << "my ix:" << my_ix.row() << '\t' << my_ix.column();
					if(!dest_ix.isValid())
						break;
					if(dest_ix.flags().testFlag(Qt::ItemIsEditable)) {
						QVariant v = src_tm->data(src_ix, Qt::DisplayRole);
						qfDebug() << "copy cell:" << src_ix << "->" << dest_ix << "val:" << v;
						//qfInfo() << my_ix.row() << '-' << my_ix.column() << "<=" << v.toString();
						dest_tm->setData(dest_ix, v);
					}
					dest_col++;
				}
				//dest_tm->postRow(origin_view_row, true);
				//origin_view_row++;
			}
			dest_tm->postAll(qf::core::Exception::Throw);
			//tv->updateAll();
			QModelIndex bottom_right = model()->index(origin_view_row - 1, origin_ix.column() + col_cnt - 1);
			QItemSelection sel(origin_ix, bottom_right);
			QItemSelectionModel *sm = selectionModel();
			sm->select(sel, QItemSelectionModel::Select);
		}
	}
	catch(qfc::Exception &e) {
		emit sqlException(e.message(), e.where(), e.stackTrace());
	}
}

void TableView::setValueInSelection_helper(const QVariant &new_val)
{
	QModelIndexList lst = selectedIndexes();
	QMap<int, QModelIndexList> row_selections;
	foreach(const QModelIndex &ix, lst) {
		row_selections[ix.row()] << ix;
	}
	QList<int> selected_row_indexes = row_selections.keys();
	if(selected_row_indexes.count() == 1) {
		int row_ix = selected_row_indexes.value(0);
		foreach(const QModelIndex &ix, row_selections.value(row_ix)) {
			model()->setData(ix, new_val);
		}
	}
	else if(selected_row_indexes.count() > 1) {
		qfc::sql::Connection conn;
		auto *sql_m = qobject_cast<qf::gui::model::SqlTableModel *>(tableModel());
		if(sql_m) {
			try {
				conn = sql_m->sqlConnection();
				qfc::sql::Transaction transaction(conn);
				QF_TIME_SCOPE(QString("Saving %1 rows").arg(selected_row_indexes.count()));
				foreach(int row_ix, selected_row_indexes) {
					foreach(const QModelIndex &ix, row_selections.value(row_ix)) {
						model()->setData(ix, new_val);
					}
					sql_m->postRow(toTableModelRowNo(row_ix), qf::core::Exception::Throw);
				}
				transaction.commit();
			}
			catch(qfc::Exception &e) {
				dialogs::MessageBox::showException(this, e);
			}
		}
		update();
	}
}

void TableView::setValueInSelection()
{
	QString new_val_str;
	Q_FOREACH(auto ix, selectedIndexes()) {
		if(new_val_str.isEmpty()) {
			new_val_str = model()->data(ix, Qt::DisplayRole).toString();
		}
	}
	bool ok;
	new_val_str = QInputDialog::getText(this, tr("Enter value"), tr("new value:"), QLineEdit::Normal, new_val_str, &ok);
	if(!ok)
		return;
	setValueInSelection_helper(new_val_str);
}



void TableView::editCellContentInEditor()
{
	QModelIndex ix = currentIndex();
	if(ix.isValid()) {
		//qfInfo() << "model()->data(currentIndex(), Qt::EditRole)";
		QVariant cell_value = model()->data(ix, Qt::EditRole);
		QString cell_text;
#if QT_VERSION_MAJOR >= 6
		if(cell_value.typeId() == QMetaType::QByteArray)
#else
		if(cell_value.type() == QVariant::ByteArray)
#endif
			cell_text = QString::fromUtf8(cell_value.toByteArray());
		else
			cell_text = cell_value.toString();
		auto *w = new TextEditWidget(this);
		w->setText(cell_text);
		w->setSuggestedFileName("new_file.txt");
		/*
		if(!persistentSettingsPath().isEmpty()) {
			w->setPersistentOptionsPath(persistentSettingsPath() + "/exportReport");
			w->loadPersistentOptions();
		}
		*/
		dialogs::Dialog dlg(this);
		auto *bb = new DialogButtonBox(QDialogButtonBox::Cancel, this);
		QAbstractButton *bt_save = bb->addButton(QDialogButtonBox::Save);
		connect(bt_save, &QAbstractButton::clicked, &dlg, &QDialog::accept);
		dlg.setButtonBox(bb);
		dlg.setCentralWidget(w);
		dlg.setPersistentSettingsId("cellEditor");
		dlg.loadPersistentSettingsRecursively();
		//connect(w, &reports::PrintTableViewWidget::printRequest, this, &TableView::exportReport_helper);
		if(dlg.exec()) {
#if QT_VERSION_MAJOR >= 6
			auto cell_type = cell_value.metaType().id();
#else
			int cell_type = cell_value.type();
#endif
			if(model()->flags(ix) & Qt::ItemIsEditable) {
				cell_value = w->text();
				if(cell_type == QMetaType::QByteArray)
					cell_value = QByteArray(cell_value.toString().toUtf8());
				//qfInfo() << "text:" << v.toString();
				model()->setData(ix, cell_value);
			}
		}
	}
	else {
		qfWarning() << QF_FUNC_NAME << "invalid model index.";
	}
}

void TableView::exportCSV()
{
	gui::model::TableModel *m = tableModel();
	if(!m)
		return;

	auto *w = new qf::gui::ExportCsvTableViewWidget(this, this);
	if(!persistentSettingsPath().isEmpty()) {
		w->setPersistentOptionsPath(persistentSettingsPath() + "/exportCSV");
		w->loadPersistentOptions();
	}
	dialogs::Dialog dlg(this);
	auto *bb = new DialogButtonBox(QDialogButtonBox::Cancel, this);
	QAbstractButton *bt_apply = bb->addButton(QDialogButtonBox::Apply);
	connect(bt_apply, &QAbstractButton::clicked, w, &qf::gui::ExportCsvTableViewWidget::applyOptions, Qt::QueuedConnection);
	dlg.setButtonBox(bb);
	dlg.setCentralWidget(w);
	dlg.setPersistentSettingsId("exportCSV");
	dlg.loadPersistentSettingsRecursively();
	connect(w, &qf::gui::ExportCsvTableViewWidget::exportRequest, this, &TableView::exportCSV_helper);
	dlg.exec();
}

void TableView::exportReport()
{
	qfLogFuncFrame();
	auto *w = new reports::PrintTableViewWidget(this);
	if(!persistentSettingsPath().isEmpty()) {
		w->setPersistentOptionsPath(persistentSettingsPath() + "/exportReport");
		w->loadPersistentOptions();
	}
	dialogs::Dialog dlg(this);
	auto *bb = new DialogButtonBox(QDialogButtonBox::Cancel, this);
	QAbstractButton *bt_apply = bb->addButton(QDialogButtonBox::Apply);
	connect(bt_apply, &QAbstractButton::clicked, w, &reports::PrintTableViewWidget::applyOptions, Qt::QueuedConnection);
	dlg.setButtonBox(bb);
	dlg.setCentralWidget(w);
	dlg.setPersistentSettingsId("exportReport");
	dlg.loadPersistentSettingsRecursively();
	connect(w, &reports::PrintTableViewWidget::printRequest, this, &TableView::exportReport_helper);
	dlg.exec();
}

void TableView::saveCurrentCellBlob()
{
	QVariant v = model()->data(currentIndex(), Qt::EditRole);
	QString fn = QFileDialog::getSaveFileName(this, tr("Save File"));
	if(!fn.isEmpty()) {
		QFile f(fn);
		if(f.open(QIODevice::WriteOnly)) {
			f.write(v.toByteArray());
		}
	}
}

void TableView::loadCurrentCellBlob()
{
	qfLogFuncFrame();
	QString fn = QFileDialog::getOpenFileName(this, tr("Open File"));
	if(!fn.isEmpty()) {
		QFile f(fn);
		if(f.open(QIODevice::ReadOnly)) {
			QByteArray ba =  f.readAll();
			model()->setData(currentIndex(), ba);
		}
	}
}

void TableView::selectCurrentColumn()
{
	QModelIndex ix = currentIndex();
	if(ix.isValid())
		selectColumn(ix.column());
}

void TableView::selectCurrentRow()
{
	QModelIndex ix = currentIndex();
	if(ix.isValid())
		selectRow(ix.row());
}

qf::core::utils::TreeTable TableView::toTreeTable(const QString &table_name, const QVariantList &_exported_columns, const qf::gui::model::TableModel::TreeTableExportOptions &opts) const
{
	qfu::TreeTable ret(table_name);
	QVariantList exported_columns = _exported_columns;
	QAbstractItemModel *proxy_model = model();
	if(!proxy_model)
		return ret;
	gui::model::TableModel *table_model = tableModel();
	if(!table_model)
		return ret;
	const core::utils::Table &table = table_model->table();
	if(exported_columns.isEmpty()) {
		for(int ix=0; ix<table.columnCount(); ix++) {
			QVariantMap col;
			col[QStringLiteral("index")] = ix;
			exported_columns << col;
		}
	}
	bool raw_values = opts.isExportRawValues();
	for(int i=0; i<exported_columns.count(); i++) {
		QVariantMap col = exported_columns[i].toMap();
		QString cap = col.value("caption").toString();
		int ix = col.value("index").toInt();
		qfu::TreeTableColumn cd;
		if(col.value("origin") == QLatin1String("table")) {
			auto t = table.field(ix).type().id();
			cd.setName(table.field(ix).name());
			cd.setType(t);
			cd.setHeader(cap);
			//ret.appendColumn(table.field(ix).name(), t, cap);
		}
		else {
			int t;
			if(raw_values) {
				qfu::Table::Field fld = table_model->tableField(ix);
				t = fld.type().id();
				//qfWarning() << fld.toString();
			}
			else {
				t = proxy_model->headerData(ix, Qt::Horizontal, gui::model::TableModel::FieldTypeRole).toInt();
			}
			cd.setName(proxy_model->headerData(ix, Qt::Horizontal, gui::model::TableModel::FieldNameRole).toString());
			cd.setType(t);
			cd.setHeader(cap);
			//ret.appendColumn(proxy_model->headerData(ix, Qt::Horizontal, gui::model::TableModel::FieldNameRole).toString(), t, cap);
		}
		cd.setWidth(col.value("width"));
		ret.appendColumn(cd);
	}

	/// export data
	{
		for(int i=0; i<proxy_model->rowCount(); i++) {
			QVariantList row;
			core::utils::TableRow tbl_row = tableRow(i);
			for(int j=0; j<exported_columns.count(); j++) {
				QVariantMap col = exported_columns[j].toMap();
				QVariant val;
				int ix = col.value("index").toInt();
				if(col.value("origin") == QLatin1String("table")) {
					val = tbl_row.value(ix);
					//qfWarning() << col << val.typeName() << "val:" << val.toString();
				}
				else {
					QModelIndex mix = proxy_model->index(i, ix);
					if(raw_values) {
						val = proxy_model->data(mix, gui::model::TableModel::RawValueRole);
						//qfWarning() << col << val.typeName() << "val:" << val.toString();
					}
					else {
						val = proxy_model->data(mix, Qt::DisplayRole);
						//qfWarning() << col << val.typeName() << "val:" << val.toString();
					}
				}
				row << val;
			}
			ret.appendRow(row);
		}
	}
	return ret;
}

void TableView::exportReport_helper(const QVariant& _options)
{
	try {
		QVariantMap options = _options.toMap();
		qfu::Table::TextExportOptions opts(options.value("options").toMap());
		QVariantList exported_columns = options.value("columns").toList();

		qfu::TreeTable ttable;

		{
			qf::gui::model::TableModel::TreeTableExportOptions opts;
			//opts.setExportRawValues(true);
			ttable = toTreeTable("data", exported_columns, opts);
		}

		{
			bool report_banner = false;
			QString report_title = opts.value("report").toMap().value("title").toString();
			if(!report_title.isEmpty())
				report_banner = true;
			QString report_note = opts.value("report").toMap().value("note").toString();
			if(!report_note.trimmed().isEmpty())
				report_banner = true;
			if(report_banner) {
				ttable.setValue("title", report_title);
				ttable.setValue("note", report_note);
			}
		}

		//qfInfo() << ttable.toString();

		auto *rw = new reports::ReportViewWidget(nullptr);
		rw->setTableData(QString(), ttable);
		QString report_fn = opts.value("report").toMap().value("fileName").toString();
		rw->setReport(report_fn);
		dialogs::Dialog dlg(this);
		dlg.setCentralWidget(rw);
		dlg.setPersistentSettingsId("exportReportDialog");
		dlg.loadPersistentSettingsRecursively();
		dlg.exec();
	}
	catch(qfc::Exception &e) {
		dialogs::MessageBox::showException(this, e);
	}
}

void TableView::exportCSV_helper(const QVariant &export_options)
{
	QAbstractItemModel *m = model();
	if(!m)
		return;
	QString fn = qf::gui::dialogs::FileDialog::getSaveFileName(this, tr("Save as ..."), "data.csv", "Coma Separated Values *.csv (*.csv)");
	if(fn.isEmpty())
		return;

	QFile f(fn);
	if(!f.open(QFile::WriteOnly)) {
		qf::gui::dialogs::MessageBox::showError(this, tr("Cannot open file '%1' for writing.").arg(f.fileName()));
		return;
	}
	try {
		QVariantMap export_opts = export_options.toMap();
		qf::core::utils::Table::TextExportOptions text_export_opts(export_opts);

		QTextStream ts(&f);
#if QT_VERSION_MAJOR >= 6
		ts.setEncoding(QStringConverter::encodingForName(text_export_opts.codecName().toLatin1().constData()).value());
#else
		ts.setCodec(text_export_opts.codecName().toLatin1().constData());
#endif

		QVector<int> exported_columns;
		for(const auto &v : export_opts.value("columns").toList()) {
			exported_columns << v.toInt();
		}
		if(text_export_opts.isExportColumnNames()) {
			for (int i = 0; i < exported_columns.count(); ++i) {
				if(i > 0)
					ts << text_export_opts.fieldSeparator();
				int ix = exported_columns[i];
				QString cap = m->headerData(ix, Qt::Horizontal).toString();
				//if(!opts.isFullColumnNames())
				//	qf::core::Utils::parseFieldName(cap, &cap);
				ts << qf::core::utils::Table::quoteCSV(cap, text_export_opts);
			}
			ts << '\n';
		}
		//qfInfo() << "header exported";
		// export data
		//int n = 0, cnt = rowCount(), steps = 100, progress_step = cnt / steps + 1;
		int n1 = text_export_opts.fromLine();
		int n2 = text_export_opts.toLine();
		if(n2 < 0)
			n2 = std::numeric_limits<int>::max();
		for(int row_ix=n1; row_ix<m->rowCount() && row_ix<=n2; row_ix++) {
			//if(cnt) if(n++ % progress_step) emit progressValue(1.*n/cnt, tr("Probiha export"));
			for(int j=0; j<exported_columns.count(); j++) {
				int col_ix = exported_columns[j];
				QModelIndex ix = m->index(row_ix, col_ix);
				QVariant val = m->data(ix, Qt::EditRole);
#if QT_VERSION_MAJOR >= 6
				if(val.typeId() != QMetaType::Bool)
#else
				if(val.type() != QVariant::Bool)
#endif
					val = m->data(ix, Qt::DisplayRole);
				if(j > 0)
					ts << text_export_opts.fieldSeparator();
				ts << qf::core::utils::Table::quoteCSV(val, text_export_opts);
			}
			ts << '\n';
		}
	}
	catch(qf::core::Exception &e) {
		qf::gui::dialogs::MessageBox::showException(this, e);
	}
}

void TableView::updateRow(int row)
{
	QModelIndex ix = currentIndex();
	if(row < 0)
		row = ix.row();
	if(row < 0)
		return;
	if(ix.row() != row)
		ix = ix.sibling(row, ix.column());
	QRect r = visualRect(ix);
	// expand rect to whole row
	if(r.isEmpty()) return;
	r.setX(0);
	r.setWidth(width());
	viewport()->update(r);
	// update header
	QHeaderView *vh = verticalHeader();
	if(vh) {
		//r = QRect(0, vh->sectionViewportPosition(row), vh->viewport()->width(), vh->sectionSize(row));
		verticalHeader()->viewport()->update();
	}
}

void TableView::updateDataArea()
{
	viewport()->update();
}

void TableView::updateAll()
{
	updateDataArea();
	verticalHeader()->viewport()->update();
	horizontalHeader()->viewport()->update();
}

qf::core::utils::TableRow &TableView::tableRowRef(int row_no)
{
	int ri = row_no;
	if(row_no < 0)
		ri = currentIndex().row();
	auto m = tableModel();
	QF_ASSERT_EX(m != nullptr, "Table model is NULL");
	ri = toTableModelRowNo(ri);
	return m->tableRef().rowRef(ri);
}

qf::core::utils::TableRow TableView::tableRow(int row_no) const
{
	qf::core::utils::TableRow ret;
	int ri = row_no;
	if(row_no < 0)
		ri = currentIndex().row();
	auto m = tableModel();
	if(m != nullptr) {
		ri = toTableModelRowNo(ri);
		if(m->table().isValidRowIndex(ri))
			ret = m->table().row(ri);
	}
	return ret;
}

int TableView::logicalColumnIndex(const QString &field_name) const
{
	auto *m = model();
	if(m) for (int i = 0; i < m->columnCount(); ++i) {
		QString fldn = m->headerData(i, Qt::Horizontal, qf::gui::model::TableModel::FieldNameRole).toString();
		if(qf::core::Utils::fieldNameEndsWith(fldn, field_name))
			return i;
	}
	return -1;
}

QList<int> TableView::selectedRowsIndexes() const
{
	const QModelIndexList lst = selectedIndexes();
	QSet<int> set;
	for(const QModelIndex &ix : lst) {
		if(ix.row() >= 0)
			set << ix.row();
	}
	QList<int> ret = set.values();
	std::sort(ret.begin(), ret.end());
	return ret;
}

QList<int> TableView::selectedColumnsIndexes() const
{
	const QModelIndexList lst = selectedIndexes();
	QSet<int> set;
	for(const QModelIndex &ix : lst) {
		if(ix.column() >= 0)
			set << ix.column();
	}
	QList<int> ret = set.values();
	std::sort(ret.begin(), ret.end());
	return ret;
}

void TableView::rowExternallySaved(const QVariant &id, int mode)
{
	qfLogFuncFrame() << "id:" << id.toString() << "mode:" << mode;
	auto table_model = qobject_cast<qf::gui::model::SqlTableModel*>(tableModel());
	if (!table_model) {
		qfWarning() << "Not SqlTableModel.";
		return;
	}
	if(table_model) {
		std::optional<int> row_with_id_index;
		for(int ri = 0; ri < table_model->rowCount(); ri++) {
			auto v = table_model->value(ri, tableModel()->idColumnName());
			//qfDebug() << "\t row" << ri << "id:" << v.toString();
			if(v == id) {
				row_with_id_index = ri;
				break;
			}
		}
		auto query_str = table_model->reloadRowQuery(id);
		if (query_str.isEmpty()) {
			return;
		}
		qf::core::sql::Query q;
		bool ok = q.exec(query_str);
		if (!ok) {
			qfInfo() << "Query:" << query_str;
			qfWarning() << "SQL error:" << q.lastErrorText();
			return;
		}
		int row_cnt = 0;
		while (q.next()) {
			row_cnt++;
		}
		if (row_cnt > 1) {
			qfWarning() << "More rows returned for id:" << id;
			return;
		}
		auto row_exists_in_db = row_cnt == 1;

		auto curr_col = currentIndex().column();
		auto curr_row = std::max(0, toTableModelRowNo(currentIndex().row()));
		QModelIndex new_ix;
		if (row_with_id_index && row_exists_in_db) {
			// edit
			table_model->reloadRow(row_with_id_index.value());
		}
		else if (!row_with_id_index && row_exists_in_db) {
			// insert
			table_model->insertRow(curr_row);
			table_model->setValue(curr_row, tableModel()->idColumnName(), id);
			table_model->reloadRow(curr_row);
			new_ix = table_model->index(curr_row, curr_col);
			setCurrentIndex(m_proxyModel->mapFromSource(new_ix));
		}
		else if (row_with_id_index && !row_exists_in_db) {
			// delete
			table_model->qfm::TableModel::removeRowNoOverload(row_with_id_index.value(), !qf::core::Exception::Throw);
			auto row = currentIndex().row();
			if (row >= model()->rowCount()) {
				row = model()->rowCount() - 1;
			}
			setCurrentIndex(model()->index(row, curr_col));
		}
		else {
			// nor in table neither in database
		}
	}
	else {
		qfError() << "Feature not defined for this model type:" << model();
	}
}

int TableView::seekColumn() const
{
	int ret = -1;
	QHeaderView *h = horizontalHeader();
	if(h) {
		if(h->isSortIndicatorShown() && h->sortIndicatorOrder() == Qt::AscendingOrder) {
			ret = h->sortIndicatorSection();
		}
	}
	return ret;
}

void TableView::seek(const QString &prefix_str)
{
	qfLogFuncFrame() << prefix_str;
	if(prefix_str.isEmpty())
		return;
	if(!model())
		return;
	int col = seekColumn();
	if(col >= 0) {
		qf::core::Collator sort_collator = tableModel()->table().sortCollator();
		sort_collator.setCaseSensitivity(Qt::CaseInsensitive);
		sort_collator.setIgnorePunctuation(true);
		//qfWarning() << sort_collator.compare(QString::fromUtf8("s"), QString::fromUtf8("š")) << QString::fromUtf8("š").toUpper();
		//qfWarning() << "collator CS:" << (sort_collator.caseSensitivity() == Qt::CaseSensitive);
		for(int i=0; i<model()->rowCount(); i++) {
			QModelIndex ix = model()->index(i, col, QModelIndex());
			QString data_str = model()->data(ix, Qt::DisplayRole).toString();//.mid(0, prefix_str.length()).toLower();
			/// QTBUG-37689 QCollator allways sorts case sensitive
			/// workarounded by own implementation of qf::core::Collator
			QString ds = data_str.left(qMin(prefix_str.length(), data_str.length()));
			//QString ps = prefix_str.toLower();
			//QString ds = data_str.mid(0, ps.length()).toLower();
			int cmp = sort_collator.compare(prefix_str, ds);
			//qfInfo() << ps << "cmp" << ds << "->" << cmp;
			if(cmp <= 0) {
				setCurrentIndex(ix);
				break;
			}
		}
	}
}

void TableView::cancelSeek()
{
	if(!m_seekString.isEmpty()) {
		m_seekString = QString();
		emit seekStringChanged(m_seekString);
	}
}

QModelIndex TableView::toTableModelIndex(const QModelIndex &table_view_index) const
{
	QModelIndex ret = table_view_index;
	QAbstractProxyModel *pxm = m_proxyModel;
	while(pxm) {
		ret = pxm->mapToSource(ret);
		pxm = qobject_cast<QAbstractProxyModel*>(pxm->sourceModel());
	}
	return ret;
}

int TableView::toTableModelRowNo(int table_view_row_no) const
{
	//qfLogFuncFrame() << table_view_row_no;
	QModelIndex ix = m_proxyModel->index(table_view_row_no, 0);
	//qfDebug() << ix << "model:" << ix.model() << "proxy:" << m_proxyModel;
	ix = toTableModelIndex(ix);
	//qfDebug() << "RETURN:" << ix.row();
	return ix.row();
}

void TableView::loadPersistentSettings()
{
	QString path = persistentSettingsPath();
	qfLogFuncFrame() << path;
	if(!path.isEmpty()) {
		auto *horiz_header = qobject_cast<HeaderView*>(horizontalHeader());
		if(!horiz_header || horiz_header->count() == 0) {
			qfDebug() << path << "Cannot load persistent settings, horizontal header does not exist or it is empty.";
			return;
		}
		qf::gui::model::TableModel *mod = tableModel();
		if(!mod || mod->columnCount() == 0) {
			qfDebug() << path << "Cannot load persistent settings, table model does not exist or it is empty.";
			return;
		}

		QSettings settings;
		settings.beginGroup(path);

		QByteArray header_state = settings.value("horizontalheader").toString().toLatin1();
		header_state = QByteArray::fromBase64(header_state);
		//qfInfo() << path << "Restoring horizontal header state. State is empty:" << header_state.isEmpty();
		if(!header_state.isEmpty()) {
			horiz_header->restoreState(header_state);
		}
	}
}

void TableView::savePersistentSettings()
{
	QString path = persistentSettingsPath();
	//qfInfo() << Q_FUNC_INFO << this << path;
	qfLogFuncFrame() << path;
	if(!path.isEmpty()) {
		QSettings settings;
		settings.beginGroup(path);
		auto *horiz_header = qobject_cast<HeaderView*>(horizontalHeader());

		QByteArray header_state = horiz_header->saveState();
		settings.setValue("horizontalheader", QString::fromLatin1(header_state.toBase64()));
	}
}

void TableView::onSqlException(const QString &what, const QString &where, const QString &stack_trace)
{
	if(!isShowExceptionDialog())
		return;
	dialogs::MessageBox::showException(this, what, where, stack_trace);
}

void TableView::keyPressEvent(QKeyEvent *e)
{
	// qfLogFuncFrame() << "key:" << e->key() << "modifiers:" << e->modifiers();
	if(!model()) {
		e->ignore();
		return;
	}

	bool incremental_search = false;
	bool incremental_search_key_accepted = false;
	qfc::String old_seek_string = m_seekString;
	//bool modified = (e->modifiers() != Qt::NoModifier && e->modifiers() != Qt::KeypadModifier);
	bool key_enter = (e->key() == Qt::Key_Return && e->modifiers() == 0)
			|| (e->key() == Qt::Key_Enter && e->modifiers() == Qt::KeypadModifier);
	if(e->modifiers() == Qt::ControlModifier) {
		if(e->key() == Qt::Key_C) {
			copy();
			e->accept();
			return;
		}
		if(e->key() == Qt::Key_V) {
			paste();
			e->accept();
			return;
		}
		if(e->key() == Qt::Key_Enter || e->key() == Qt::Key_Return) {
			qfDebug() << "\tCTRL+ENTER";
			postRow();
			e->accept();
			return;
		}
	}
	else if(key_enter) {
		qfDebug() << "\t ENTER pressed";
	}
	else {
		//qfu::Table::SortDef sd = seekSortDefinition();
		if(seekColumn() >= 0 && (currentIndex().column() == seekColumn() || !currentIndex().isValid())) {
			incremental_search = true;
			if(!currentIndex().isValid())
				setCurrentIndex(model()->index(0, seekColumn(), QModelIndex()));
			//qfInfo() << "incremental search currentIndex row:" << currentIndex().row() << "col:" << currentIndex().column();
			/// Pokud je nektery sloupec serazen vzestupne zkusi se provest incremental search,
			/// pak se event dal nepropaguje
			QChar seek_char = qfc::String(e->text()).value(0);
			//bool is_valid_seek_char = true;
			if(e->key() == Qt::Key_Escape
			   || e->key() == Qt::Key_Enter
			   || e->key() == Qt::Key_Return
			   || e->key() == Qt::Key_Tab
			   || e->key() == Qt::Key_Home
			   || e->key() == Qt::Key_End
			   || e->key() == Qt::Key_Left
			   || e->key() == Qt::Key_Up
			   || e->key() == Qt::Key_Right
			   || e->key() == Qt::Key_Down
			   || e->key() == Qt::Key_PageUp
			   || e->key() == Qt::Key_PageDown) {
				incremental_search = false;
				seek_char = QChar();
			}
			else if(seek_char == '\n' || seek_char == '\r')
				seek_char = QChar();
			else if(seek_char.isSpace() && m_seekString.isEmpty())
				seek_char = QChar();
			qfDebug().nospace() << "\t incremental search seekChar unicode: 0x" << QString::number(seek_char.unicode(),16) << " key: 0x" << QString::number(e->key(),16) << "is space:" << seek_char.isSpace();
			bool shift_only = (e->key() == Qt::Key_Shift);
			//bool ctrl_only = (e->key() == Qt::Key_Control);
			qfDebug().nospace() << "\t incremental search seekChar unicode: 0x" << QString::number(seek_char.unicode(),16) << " key: 0x" << QString::number(e->key(),16) << " shift only: " << shift_only;
			//bool accept = false;
			if(incremental_search) {
				if(e->key() == Qt::Key_Backspace) {
					m_seekString = old_seek_string.slice(0, -1);
					incremental_search_key_accepted = true;
				}
				else if(e->key() == Qt::Key_Escape) {
					m_seekString = QString();
					incremental_search_key_accepted = true;
				}
				else if(seek_char.isNull() && !shift_only) {
					m_seekString = QString();
				}
				else if(!seek_char.isNull()) {
					m_seekString += seek_char;
					qfDebug() << "new seek text:" << m_seekString;
					incremental_search_key_accepted = true;
				}
				if(!m_seekString.isEmpty()) {
					if(m_seekString != old_seek_string) {
						seek(m_seekString);
					}
					incremental_search_key_accepted = true;
				}
			}
		}
	}
	if(m_seekString != old_seek_string)
		emit seekStringChanged(m_seekString);
	if(incremental_search && incremental_search_key_accepted) {
		qfDebug() << "\tUSED for incremental search";
		e->accept();
		return;
	}
	cancelSeek();
	//bool event_should_be_accepted = false;
	/// nejedna se o inkrementalni vyhledavani, zkusime editaci
	if(state() == EditingState) {
		qfDebug() << "\teditor exists";
		//QModelIndex current = currentIndex();
		//QModelIndex newCurrent;
		/// cursor keys moves selection, check editor data before
		/// some of switched keys should be filtered by editor
		qfDebug() << "\tHAVE EDITOR key:" << e->key();
		switch (e->key()) {
		case Qt::Key_Down:
		case Qt::Key_Up:
		case Qt::Key_Left:
		case Qt::Key_Right:
		case Qt::Key_Home:
		case Qt::Key_End:
		case Qt::Key_PageUp:
		case Qt::Key_PageDown:
		case Qt::Key_Tab:
		case Qt::Key_Backtab:
		case Qt::Key_Return:
		case Qt::Key_Enter:
			qfDebug() << "accepting event";
			/// je to trochu jedno, protoze to vypada, ze accept flag, kterej prijde dialogu je ten, jak ho nastavi editor (widget dodany delegatem) ve sve funkci keyPressEvent(...)
			e->accept();
			//event_should_be_accepted = true;
			//qfDebug().color(QFLog::Yellow) << "set focus to table view";
			//setFocus(); /// jinak se mi zavre delegat a focus skoci na jinej widget
			break;
		}
	}
	else {
		if(key_enter) {
			///timhle jsem zajistil, ze se editor otevira na enter a ne jen na F2, jak je defaultne v QT
			/// viz: QAbstractItemView::keyPressEvent(...)
			qfDebug() << "\tkey chci otevrit editor, state:" << state();
			if(edit(currentIndex(), EditKeyPressed, e)) {
				qfDebug() << "accepting event";
				e->accept();
			}
			else {
				qfDebug() << "ignoring event";
				e->ignore();
			}
			//qfDebug() << "accepting event";
			//e->accept();
			qfDebug() << "\t exiting after open editor try, event accepted:" << e->isAccepted() << "event:" << e;
			return;
		}
	}
	qfDebug() << "\tcalling parent implementation QTableView::keyPressEvent(e), state:" << state() << "event accepted:" << e->isAccepted();
	QTableView::keyPressEvent(e);
	/// parent implementace muze zmenit accepted() flag eventu
	qfDebug() << "\tcalled parent implementation QTableView::keyPressEvent(e), state:" << state() << "event accepted:" << e->isAccepted();
	//if(event_should_be_accepted)
	//	e->accept();
}

void TableView::mousePressEvent(QMouseEvent * e)
{
	qfLogFuncFrame();

	cancelSeek();
	QPoint pos = e->pos();
	QModelIndex ix = indexAt(pos);
	/*
	qfDebug() << "\trow:" << ix.row() << "col:" << ix.column();
	QFItemDelegate *it = qobject_cast<QFItemDelegate*>(itemDelegate());
	if(it) {
		//qfDebug() << "\teditor" << w;
		// pokud existuje editor, je pole rozeditovany a melo by se zkontrolovat
		if(!it->canCloseEditor()) return;
	}
	*/
	Super::mousePressEvent(e);
	if(ix.isValid() && currentIndex() != ix) {
		/// pokud je bunka typu bool a kliknu primo na zaskrtavatko, defaultni implementace nezpusobi, ze se tam presune i currentIndex()
		/// ja to ale potrebuju, aby se nova hodnota ulozila pri currentChanged(), tak si to takhle delam sam
		setCurrentIndex(ix);
	}
}

void TableView::contextMenuEvent(QContextMenuEvent *e)
{
	qfLogFuncFrame();
	QList<QAction*> lst;
	Q_FOREACH(auto a, contextMenuActions())
		lst << a;
	QMenu::exec(lst, viewport()->mapToGlobal(e->pos()));
}

void TableView::updateGeometries()
{
	Super::updateGeometries();
	QWidget *corner_widget = cornerWidget();
	if(corner_widget && corner_widget->isVisible()) {
		leftTopCornerButton()->setVisible(true);
		leftTopCornerButton()->setGeometry(cornerWidget()->geometry());
	}
	else {
		leftTopCornerButton()->setVisible(false);
	}
}

void TableView::createActions()
{
	auto *style = Style::instance();
	Action *a;
	{
		a = new Action(tr("Resize columns to contents"), this);
		a->setIcon(style->icon("zoom_fitwidth"));
		//a->setShortcut(QKeySequence(tr("Ctrl+R", "reload SQL table")));
		////a->setShortcutContext(Qt::WidgetShortcut);
		a->setOid("resizeColumnsToContents");
		m_actionGroups[SizeActions] << a->oid();
		m_actions[a->oid()] = a;
		connect(a, &Action::triggered, this, &TableView::resizeColumnsToContents);
	}
	{
		a = new Action(tr("Reset columns settings"), this);
		a->setToolTip(tr("Reset column widths and positions."));
		//a->setShortcut(QKeySequence(tr("Ctrl+R", "reload SQL table")));
		////a->setShortcutContext(Qt::WidgetShortcut);
		a->setOid("resetColumnsSettings");
		m_actionGroups[SizeActions] << a->oid();
		m_actions[a->oid()] = a;
		connect(a, &Action::triggered, this, &TableView::resetColumnsSettings);
	}
	{
		a = new Action(tr("Reload"), this);
		a->setIcon(style->icon("reload"));
		a->setShortcut(QKeySequence(tr("Ctrl+R", "reload SQL table")));
		//a->setShortcutContext(Qt::WidgetShortcut);
		a->setOid("reload");
		m_actionGroups[ViewActions] << a->oid();
		m_actions[a->oid()] = a;
		connect(a, &Action::triggered, this, &TableView::reload);
	}
	{
		a = new Action(tr("Copy"), this);
		a->setObjectName("TableView_Copy");
		a->setIcon(style->icon("copy"));
		a->setShortcut(QKeySequence(tr("Ctrl+C", "Copy selection")));
		a->setShortcutContext(Qt::WidgetShortcut);
		a->setOid("copy");
		m_actionGroups[ViewActions] << a->oid();
		m_actions[a->oid()] = a;
		connect(a, &Action::triggered, this, &TableView::copy);
	}
	{
		a = new Action(tr("Copy special"), this);
		a->setIcon(style->icon("copy"));
		//a->setShortcut(QKeySequence(tr("Ctrl+C", "Copy selection")));
		////a->setShortcutContext(Qt::WidgetShortcut);
		a->setOid("copySpecial");
		m_actionGroups[ViewActions] << a->oid();
		m_actions[a->oid()] = a;
		connect(a, &Action::triggered, this, &TableView::copySpecial);
	}
	{
		a = new Action(tr("Paste"), this);
		a->setIcon(style->icon("paste"));
		a->setShortcut(QKeySequence(tr("Ctrl+V", "Paste rows")));
		a->setShortcutContext(Qt::WidgetShortcut);
		a->setOid("paste");
		m_actionGroups[PasteActions] << a->oid();
		m_actions[a->oid()] = a;
		connect(a, &Action::triggered, this, &TableView::paste, Qt::QueuedConnection); /// hazelo mi to vyjjimky v evendloopu
	}
	{
		a = new Action(style->icon("insert-row"), tr("Insert row"), this);
		a->setShortcut(QKeySequence(tr("Ctrl+Ins", "insert row SQL table")));
		//a->setShortcutContext(Qt::WidgetShortcut);
		a->setOid("insertRow");
		m_actionGroups[RowActions] << a->oid();
		m_actions[a->oid()] = a;
		connect(a, &Action::triggered, this, &TableView::insertRow);
	}
	{
		a = new Action(style->icon("delete-row"), tr("Delete selected rows"), this);
		a->setShortcut(QKeySequence(tr("Ctrl+Del", "delete row SQL table")));
		//a->setShortcutContext(Qt::WidgetShortcut);
		a->setOid("removeSelectedRows");
		m_actionGroups[RowActions] << a->oid();
		m_actions[a->oid()] = a;
		connect(a, &Action::triggered, this, &TableView::removeSelectedRows);
	}
	{
		a = new Action(tr("Post row edits"), this);
		a->setIcon(style->icon("save"));
		a->setShortcut(QKeySequence(tr("Ctrl+Return", "post row SQL table")));
		//a->setShortcutContext(Qt::WidgetShortcut);
		a->setOid("postRow");
		m_actionGroups[RowActions] << a->oid();
		m_actions[a->oid()] = a;
		connect(a, &Action::triggered, this, &TableView::postRow);
	}
	{
		a = new Action(tr("Revert row edits"), this);
		a->setIcon(style->icon("revert"));
		a->setShortcut(QKeySequence(tr("Ctrl+Z", "revert edited row")));
		//a->setShortcutContext(Qt::WidgetShortcut);
		a->setOid("revertRow");
		m_actionGroups[RowActions] << a->oid();
		m_actions[a->oid()] = a;
		connect(a, &Action::triggered, this, &TableView::revertRow);
	}
	{
		a = new Action(tr("Clone row"), this);
		a->setIcon(style->icon("clone-row"));
		a->setOid("cloneRow");
		//a->setVisible(false);
		m_actionGroups[RowActions] << a->oid();
		a->setShortcut(QKeySequence(tr("Ctrl+D", "insert row copy")));
		//a->setShortcutContext(Qt::WidgetShortcut);
		m_actionGroups[RowActions] << a->oid();
		m_actions[a->oid()] = a;
		connect(a, &Action::triggered, this, &TableView::cloneRow);
	}
	{
		a = new Action("editRowsSeparator", this);
		a->setOid("editRowsSeparator");
		a->setSeparator(true);
		m_actionGroups[RowActions] << a->oid();
		m_actions[a->oid()] = a;
	}
	{
		a = new Action(tr("Zobrazit ve formulari"), this);
		a->setIcon(style->icon("view"));
		a->setToolTip(tr("Zobrazit radek v formulari pro cteni"));
		//a->setShortcutContext(Qt::WidgetShortcut);
		//connect(a, &Action::triggered, this, &TableView::emitViewRowInExternalEditor()));
		a->setOid("viewRowExternal");
		m_actions[a->oid()] = a;
	}
	{
		a = new Action(tr("Upravit ve formulari"), this);
		a->setIcon(style->icon("edit"));
		a->setToolTip(tr("Upravit radek ve formulari"));
		//a->setShortcutContext(Qt::WidgetShortcut);
		//connect(a, &Action::triggered, this, &TableView::emitEditRowInExternalEditor()));
		a->setOid("editRowExternal");
		m_actions[a->oid()] = a;
	}
	/*
	{
		a = new Action(style->icon("sort-asc"), tr("Sort ascending"), this);
		a->setOid("sortAsc");
		a->setCheckable(true);
		//a->setToolTip(tr("Upravit radek v externim editoru"));
		//a->setShortcutContext(Qt::WidgetShortcut);
		m_actionGroups[SortActions] << a->oid();
		//connect(a, SIGNAL(triggered(bool)), this, SLOT(sortAsc(bool)));
		m_actions[a->oid()] = a;
	}
	{
		a = new Action(style->icon("sort-desc"), tr("Sort descending"), this);
		a->setOid("sortDesc");
		a->setCheckable(true);
		//a->setToolTip(tr("Upravit radek v externim editoru"));
		////a->setShortcutContext(Qt::WidgetShortcut);
		m_actionGroups[SortActions] << a->oid();
		//connect(a, SIGNAL(triggered(bool)), this, SLOT(sortDesc(bool)));
		m_actions[a->oid()] = a;
	}
	*/
	{
		a = new Action(style->icon("find"), tr("Filter table"), this);
		a->setOid("filter");
		a->setCheckable(false);
		//a->setToolTip(tr("Upravit radek v externim editoru"));
		a->setShortcut(QKeySequence(tr("Ctrl+F", "filter table")));
		//a->setShortcutContext(Qt::WidgetShortcut);
		m_actionGroups[SortActions] << a->oid();
		connect(a, &Action::triggered, this, &TableView::filterDialogRequest);
		m_actions[a->oid()] = a;
	}
	{
		a = new Action(tr("Edit cell content"), this);
		//a->setToolTip(tr("Upravit radek v externim editoru"));
		a->setShortcut(QKeySequence(tr("Ctrl+Shift+T", "Edit cell content")));
		//a->setShortcutContext(Qt::WidgetShortcut);
		connect(a, &Action::triggered, this, &TableView::editCellContentInEditor);
		a->setOid("showCurrentCellText");
		m_actionGroups[CellActions] << a->oid();
		m_actions[a->oid()] = a;
	}
	{
		a = new Action(tr("Save BLOB"), this);
		//a->setToolTip(tr("Upravit radek v externim editoru"));
		//a->setShortcutContext(Qt::WidgetShortcut);
		connect(a, &Action::triggered, this, &TableView::saveCurrentCellBlob);
		a->setOid("saveCurrentCellBlob");
		m_actionGroups[BlobActions] << a->oid();
		m_actions[a->oid()] = a;
	}
	{
		a = new Action(tr("Load BLOB from file"), this);
		//a->setToolTip(tr("Upravit radek v externim editoru"));
		//a->setShortcutContext(Qt::WidgetShortcut);
		connect(a, &Action::triggered, this, &TableView::loadCurrentCellBlob);
		a->setOid("loadCurrentCellBlob");
		m_actionGroups[BlobActions] << a->oid();
		m_actions[a->oid()] = a;
	}
	{
		a = new Action(tr("Insert rows statement"), this);
		//a->setToolTip(tr("Upravit radek v externim editoru"));
		////a->setShortcutContext(Qt::WidgetShortcut);
		//connect(a, &Action::triggered, this, &TableView::insertRowsStatement()));
		a->setOid("insertRowsStatement");
		m_actionGroups[RowActions] << a->oid();
		m_actions[a->oid()] = a;
	}
	{
		a = new Action(tr("Set NULL in selection"), this);
		//a->setToolTip(tr("Upravit radek v externim editoru"));
		a->setShortcut(QKeySequence(tr("Ctrl+Shift+L", "Set NULL in selection")));
		a->setShortcutContext(Qt::WidgetWithChildrenShortcut);
		connect(a, &Action::triggered, this, &TableView::setNullInSelection);
		a->setOid("setNullInSelection");
		m_actionGroups[SetValueActions] << a->oid();
		m_actions[a->oid()] = a;
	}
	{
		a = new Action(tr("Set value in selection"), this);
		a->setShortcut(QKeySequence(tr("Ctrl+Shift+E", "Set value in selection")));
		//a->setShortcutContext(Qt::WidgetWithChildrenShortcut);
		connect(a, &Action::triggered, this, &TableView::setValueInSelection);
		a->setOid("setValueInSelection");
		m_actionGroups[SetValueActions] << a->oid();
		m_actions[a->oid()] = a;
	}
	{
		a = new Action(tr("Generate sequence in selection"), this);
		//a->setShortcut(QKeySequence(tr("Ctrl+Shift+E", "Set value in selection")));
		////a->setShortcutContext(Qt::WidgetShortcut);
		connect(a, &Action::triggered, this, &TableView::generateSequenceInSelection);
		a->setOid("generateSequenceInSelection");
		m_actionGroups[SetValueActions] << a->oid();
		m_actions[a->oid()] = a;
	}
	{
		a = new Action(tr("Select"), this);
		a->setOid("select");
		m_actionGroups[SelectActions] << a->oid();
		m_actions[a->oid()] = a;
		auto *m = new QMenu(this);
		a->setMenu(m);
		{
			a = new Action(tr("Select current column"), this);
			a->setShortcut(QKeySequence(tr("Ctrl+Shift+C", "Select current column")));
			a->setShortcutContext(Qt::WidgetShortcut);
			connect(a, &Action::triggered, this, &TableView::selectCurrentColumn);
			a->setOid("selectCurrentColumn");
			m->addAction(a);
			//m_actions[a->oid()] = a;
			//m_actionGroups[SelectActions] << a->oid();
		}
		{
			a = new Action(tr("Select current row"), this);
			////a->setShortcutContext(Qt::WidgetShortcut);
			connect(a, &Action::triggered, this, &TableView::selectCurrentRow);
			a->setOid("selectCurrentRow");
			//m_actions[a->oid()] = a;
			//m_actionGroups[SelectActions] << a->oid();
		}
		m->addAction(a);
	}
	{
		a = new Action(tr("Calculate"), this);
		a->setOid("calculate");
		m_actionGroups[CalculateActions] << a->oid();
		m_actions[a->oid()] = a;
		auto *m = new QMenu(this);
		a->setMenu(m);
		{
			a = new Action(tr("Sum column"), this);
			//connect(a, &Action::triggered, this, &TableView::sumColumn()));
			a->setOid("sumColumn");
			m->addAction(a);
		}
		{
			a = new Action(tr("Sum selection"), this);
			//connect(a, &Action::triggered, this, &TableView::sumSelection()));
			a->setOid("sumSelection");
			m->addAction(a);
		}
	}
	{
		a = new Action(tr("Export"), this);
		a->setOid("export");
		m_actionGroups[ExportActions] << a->oid();
		m_actions[a->oid()] = a;
		auto *m = new QMenu(this);
		a->setMenu(m);
		{
			a = new Action(tr("Report"), this);
			connect(a, &Action::triggered, this, &TableView::exportReport);
			a->setOid("exportReport");
			m_actions[a->oid()] = a;
			m->addAction(a);
		}
		{
			a = new Action(tr("CSV"), this);
			connect(a, &Action::triggered, this, &TableView::exportCSV);
			a->setOid("exportCSV");
			m_actions[a->oid()] = a;
			m->addAction(a);
		}
		{
			a = new Action(tr("HTML"), this);
			//connect(a, &Action::triggered, this, &TableView::exportHTML()));
			a->setOid("exportHTML");
			m_actions[a->oid()] = a;
			m->addAction(a);
		}
#ifdef QF_XLSLIB
		{
			a = new Action(tr("XLS - MS Excel"), this);
			//connect(a, &Action::triggered, this, &TableView::exportXLS()));
			a->setOid("exportXLS");
			m_actions[a->oid()] = a;
			m->addAction(a);
		}
#endif
		{
			a = new Action(tr("XML (MS Excel 2003)"), this);
			//connect(a, &Action::triggered, this, &TableView::exportXML()));
			a->setOid("exportXML");
			m_actions[a->oid()] = a;
			m->addAction(a);
		}
	}
	{
		a = new Action(tr("Import"), this);
		a->setOid("import");
		m_actionGroups[ImportActions] << a->oid();
		m_actions[a->oid()] = a;
		auto *m = new QMenu(this);
		a->setMenu(m);
		{
			a = new Action(tr("CSV"), this);
			//connect(a, &Action::triggered, this, &TableView::importCSV()));
			a->setOid("importCSV");
			m_actions[a->oid()] = a;
			m->addAction(a);
		}
	}

	m_toolBarActions << action("insertRow");
	m_toolBarActions << action("cloneRow");
	m_toolBarActions << action("removeSelectedRows");
	m_toolBarActions << action("postRow");
	m_toolBarActions << action("revertRow");
	//m_toolBarActions << action("viewRowExternal");
	//m_toolBarActions << action("editRowExternal");
	m_toolBarActions << action("editRowsSeparator");
	m_toolBarActions << action("reload");
	m_toolBarActions << action("resizeColumnsToContents");
	a = new Action(this); a->setSeparator(true);
	m_toolBarActions << a;
	//m_toolBarActions << action("sortAsc");
	//m_toolBarActions << action("sortDesc");

	//f_contextMenuActions = standardContextMenuActions();

	{
		Q_FOREACH(Action *a, m_actions) {
			if(!a->shortcut().isEmpty()) {
				//qfInfo() << "\t inserting action" << a->text() << a->shortcut().toString();
				addAction(a); /// aby chodily shortcuty, musi byt akce pridany widgetu
			}
		}
		//qfDebug() << "\t default actions inserted";
	}
}

void TableView::copySpecial_helper(const QTableView *table_view, const QString &fields_separator, const QString &rows_separator, const QString &field_quotes, ReplaceEscapes replace_escapes)
{
	qfLogFuncFrame();
	auto *m = table_view->model();
	if(!m)
		return;
	auto selection_union = [](const QItemSelection &selection) {
		QMap<int, QList<int>> res;
		foreach(QItemSelectionRange sel, selection) {
			for(int row = sel.top(); row <= sel.bottom(); row++) {
				for(int col = sel.left(); col <= sel.right(); col++) {
					auto &cols = res[row];
					if (auto lower = std::lower_bound(cols.begin(), cols.end(), col); lower == cols.end()) {
						cols << col;
					}
					else {
						if (*lower != col) {
							cols.insert((lower - cols.begin()), col);
						}
					}
				}
			}
		}
		return res;
	};
	QString rows;
	int row_cnt = 0;
	auto sel = selection_union(table_view->selectionModel()->selection());
	for (const auto &[row, cols] : sel.asKeyValueRange()) {
		int col_cnt = 0;
		QString cells;
		for (auto col : cols) {
			QModelIndex ix = m->index(row, col);
			QString s;
			if(!ix.data(qf::gui::model::TableModel::ValueIsNullRole).toBool()) {
				s = ix.data(Qt::DisplayRole).toString();
				if(s.isEmpty()) {
					QVariant v = ix.data(Qt::CheckStateRole);
					if(v.isValid()) {
						s = (v.toInt() == Qt::Checked)? QStringLiteral("True"): QStringLiteral("False");
					}
				}
			}
			if(replace_escapes == ReplaceEscapes::Always) {
				s.replace('\r', QStringLiteral("\\r"));
				s.replace('\n', QStringLiteral("\\n"));
				s.replace('\t', QStringLiteral("\\t"));
			}
			if(replace_escapes == ReplaceEscapes::QuoteIfNeeded) {
				if (s.indexOf('\n') >= 0
						|| s.indexOf('\r') >= 0
						|| s.indexOf('\t') >= 0
				) {
					s = (field_quotes + s + field_quotes);
				}
			}
			else {
				s = (field_quotes + s + field_quotes);
			}
			//qfInfo() << "row:" << row << "col:" << col << "index:" << ix.row() << ',' << ix.column() << '\t' << s;
			if(col_cnt++ > 0) {
				cells += fields_separator;
			}
			cells += s;
		}
		if(row_cnt++ > 0)
			rows += rows_separator;
		rows += cells;
	}
	if(!rows.isEmpty()) {
		qfDebug() << "\tSetting clipboard:" << rows;
		QClipboard *clipboard = QApplication::clipboard();
		clipboard->setText(rows);
	}
}

void TableView::generateSequenceInSelection()
{
	QVariant new_val;
	QModelIndexList lst = selectedIndexes();
	/// serad indexy podle radku a pak podle sloupcu
	typedef QMap<int, QList<int> > SortedIndexes; /// row->cols
	SortedIndexes sorted_indexes;
	foreach(const QModelIndex &ix, lst) {
		sorted_indexes[ix.row()] << ix.column();
		if(!new_val.isValid()) {
			new_val = model()->data(ix, Qt::DisplayRole);
		}
	}
	bool ok;
	QString new_val_str = QInputDialog::getText(this, tr("Enter start value"), tr("start value:"), QLineEdit::Normal, new_val.toString(), &ok);
	if(!ok) return;
	int n = new_val_str.toInt();
	foreach(int row_ix, sorted_indexes.keys()) {
		QList<int> lst = sorted_indexes.value(row_ix);
		std::sort(lst.begin(), lst.end());
		foreach(int col_ix, lst) {
			QModelIndex ix = model()->index(row_ix, col_ix);
			model()->setData(ix, n++);
		}
		if(sorted_indexes.count() > 1) if(!postRow(row_ix)) break;
	}
}

QList<Action *> TableView::contextMenuActionsForGroups(int action_groups)
{
	qfLogFuncFrame();
	//static Action *act_separator = nullptr;
	QList<Action*> ret;
	QList<int> grps;
	grps << SizeActions << SortActions << FilterActions << ViewActions << PasteActions << RowActions << BlobActions << SetValueActions << CellActions << SelectActions << CalculateActions << ExportActions << ImportActions;
	int cnt = 0;
	foreach(int grp, grps) {
		if(action_groups & grp) {
			QStringList sl = m_actionGroups.value(grp);
			if(!sl.isEmpty() && (cnt++ > 0)) {
				Action *act_separator = m_separatorsForGroup.value((ActionGroup)grp);
				if(!act_separator) {
					act_separator = new Action(this);
					act_separator->setSeparator(true);
					m_separatorsForGroup[(ActionGroup)grp] = act_separator;
				}
				act_separator = m_separatorsForGroup.value((ActionGroup)grp);
				ret << act_separator;
			}
			Q_FOREACH(auto oid, sl) {
				Action *a = m_actions.value(oid);
				if(a == nullptr)
					qfWarning() << QString("Cannot find action for oid: '%1'").arg(oid);
				else
					ret << a;
			}
		}
	}
	return ret;
}

Action *TableView::action(const QString &act_oid)
{
	Action *ret = m_actions.value(act_oid);
	QF_ASSERT_EX(ret != nullptr, QString("Invalid action id: '%1'").arg(act_oid));
	return ret;
}

void TableView::currentChanged(const QModelIndex& current, const QModelIndex& previous)
{
	qfLogFuncFrame() << "row" << previous.row() << "->" << current.row();
	setFocus(); /// pokud nekdo neco resi s widgety jako reakci na signal currentChanging(), muze prijit o fokus a prestane chodit kurzorova navigace
	Super::currentChanged(current, previous);
	bool row_changed = (current.row() != previous.row() && previous.row() >= 0);
	if(row_changed) {
		// save even if inlineEditStrategy() == OnEditedValueCommit, because row can be just inserted or clonned without edits
		int row_to_save = previous.row();
		qfDebug() << "\tsaving row:" << row_to_save;

		bool ok = false;
		if(inlineEditSaveStrategy() == OnManualSubmit)
			ok = true;
		else
			ok = postRow(row_to_save);
		if(!ok)
			setCurrentIndex(previous);
		//qfDebug() << "\t" << __LINE__;

		updateRow(previous.row());
		updateRow(current.row());
	}
	refreshActions();
	if(current.row() != previous.row())
		emit currentRowChanged(current.row());
	/// na selected() muze prijit table o fokus
	setFocus();
}

void TableView::insertRowInline()
{
	qfLogFuncFrame();
	int ri = model()->rowCount();
	int tri = ri;
	QModelIndex ix = currentIndex();
	ix = ix.sibling(ix.row() + 1, ix.column());
	if(ix.isValid()) {
		ri = ix.row();
		tri = toTableModelRowNo(ri);
		if(tri < 0) {
			qfWarning() << "Valid proxy model index has invalid table model index!";
			/// this can happen when one inserts to empty table ???? why ????
			tri = 0;
		}
	}
	tableModel()->insertRow(tri);
	ix = model()->index(ix.row(), ix.column());
	if(ix.isValid())
		setCurrentIndex(ix);
	else
		setCurrentIndex(model()->index(tri, 0));
}

void TableView::removeSelectedRowsInline()
{
	qfLogFuncFrame();
	typedef QList<int> RowList;
	RowList rows_to_delete = selectedRowsIndexes();
	if(rows_to_delete.isEmpty())
		return;
	clearSelection();
	QList<RowList> continuous_sections;
	/// create continuous sections
	RowList continuous_section;
	int sections_length = 0;
	for(int i=0; i<rows_to_delete.count(); i++) {
		int row_ix = rows_to_delete[i];
		if(continuous_section.isEmpty()) {
			continuous_section << (row_ix - sections_length);
		}
		else {
			int last_ix = continuous_section.last();
			if(row_ix - last_ix > 1) {
				continuous_sections << continuous_section;
				sections_length += continuous_section.size();
				continuous_section.clear();
			}
			continuous_section << (row_ix - sections_length);
		}
	}
	if(!continuous_section.isEmpty()) {
		continuous_sections << continuous_section;
	}
	if(rows_to_delete.count() == 1) {
		if(!qf::gui::dialogs::MessageBox::askYesNo(this, tr("Do you really want to remove row?"), true)) return;
	}
	else {
		if(!qf::gui::dialogs::MessageBox::askYesNo(this, tr("Do you really want to remove all selected rows?"), true)) return;
	}
	QModelIndex ix = currentIndex();
	//ignoreCurrentChanged = true; /// na false ho nastavi currentChanged()
	Q_FOREACH(const RowList &rl, continuous_sections) {
		model()->removeRows(rl[0], rl.count());
	}
	setCurrentIndex(ix);
	updateAll();
	refreshActions();
}

bool TableView::edit(const QModelIndex& index, EditTrigger trigger, QEvent* event)
{
	qfLogFuncFrame() << "trigger:" << trigger << "event type:" << ((event)? event->type(): 0) << "editTriggers:" << editTriggers();
	bool ret = false;
	//ignoreFocusOutEventWhenOpenExternalEditor = true;

	do {
		if(event && (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease)) {
			/// pokud je mousePressEvent, pak v pripade, ze chci preklopit check u bool hodnoty, musim pustit dal
		}
		else if(trigger && !(trigger & editTriggers())) {
			/// ignoruji triggery, ktere neprijmam
			qfDebug() << "\t trigger ignored";
			ret = false;
			break;
		}

		if(trigger == QTableView::DoubleClicked || trigger == QTableView::EditKeyPressed) {
			qfDebug() << "\t emitting activated";
			emit activated(currentIndex());
			//activated_emited = true;
			event->accept();
			//ret = true;
			//return ret;
		}
		bool read_only = isReadOnly();
		/*
		if(!read_only) {
			auto *m = tableModel();
			read_only = (!m || m->isReadOnly());
		}
		*/
		try {
			ret = false;
			bool inline_editor_called = false;
			int orig_check_state = index.data(Qt::CheckStateRole).toInt();
			if(rowEditorMode() == EditRowsInline) {
				if(!read_only) {
					qfDebug() << "\t RowEditorInline";
					inline_editor_called = QTableView::edit(index, trigger, event);
					qfDebug() << "\t QTableView::edit() returned:" << ret;
				}
			}
			else  if(rowEditorMode() == EditRowsExternal || rowEditorMode() == EditRowsMixed) {
				if(rowEditorMode() == EditRowsMixed) {
					inline_editor_called = QTableView::edit(index, trigger, event);
					qfDebug() << "\t RowEditorMixed QTableView::edit() returned:" << ret;
				}
			}
			if(inline_editor_called) {
				int new_check_state = index.data(Qt::CheckStateRole).toInt();
				qfDebug() << "check state changed:" << orig_check_state << "->" << new_check_state;
				if(orig_check_state != new_check_state) {
					/// handle bool values changes
					/// booleans are not using editor created by ItemDelegate, but set model data directly using Qt::CheckStateRole
					if(inlineEditSaveStrategy() == OnEditedValueCommit) {
						postRow(currentIndex().row());
					}
					refreshActions();
				}
				ret = true;
			}
			else {
				if(trigger == QTableView::DoubleClicked || trigger == QTableView::EditKeyPressed) {
					if(!read_only) {
						emit editCellRequest(index);
						QVariant id = selectedRow().value(tableModel()->idColumnName());
						if(id.isValid()) {
							emit editRowInExternalEditor(id, ModeEdit);
						}
						emit editSelectedRowsInExternalEditor(ModeEdit);
					}
					ret = false;
					event->accept();
				}
			}
		}
		catch(qfc::Exception &e) {
			emit sqlException(e.message(), e.where(), e.stackTrace());
		}
	} while(false);

	//ignoreFocusOutEventWhenOpenExternalEditor = false;
	qfDebug() << "\t return:" << ret;
	return ret;
}

void TableView::commitData(QWidget *editor)
{
	qfLogFuncFrame() << editor;
	Super::commitData(editor);
	if(inlineEditSaveStrategy() == OnEditedValueCommit) {
		postRow(currentIndex().row());
	}
}

void TableView::filterByString(const QString &s)
{
	m_proxyModel->setRowFilterString(s);
	refreshActions();
}

QString TableView::filterString() const
{
	return m_proxyModel->rowFilterString();
}

