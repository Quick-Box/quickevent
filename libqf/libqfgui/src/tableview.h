#ifndef QF_GUI_TABLEVIEW_H
#define QF_GUI_TABLEVIEW_H

#include "guiglobal.h"
#include "framework/ipersistentsettings.h"
#include "model/datadocument.h"

#include <qf/core/utils.h>
#include <qf/core/utils/table.h>

#include <QTableView>

class QAbstractProxyModel;
class QSortFilterProxyModel;
class QAbstractButton;

namespace qf {
namespace gui {

class Action;
class TableViewProxyModel;

class QFGUI_DECL_EXPORT TableView : public QTableView, public framework::IPersistentSettings
{
	Q_OBJECT

	Q_PROPERTY(QString persistentSettingsId READ persistentSettingsId WRITE setPersistentSettingsId)
	Q_PROPERTY(qf::gui::model::TableModel* model READ tableModel WRITE setTableModel)
	Q_PROPERTY(RowEditorMode rowEditorMode READ rowEditorMode WRITE setRowEditorMode NOTIFY rowEditorModeChanged)
	Q_PROPERTY(InlineEditSaveStrategy inlineEditSaveStrategy READ inlineEditSaveStrategy WRITE setInlineEditSaveStrategy NOTIFY inlineEditSaveStrategyChanged)
	Q_PROPERTY(bool showExceptionDialog READ isShowExceptionDialog WRITE setShowExceptionDialog NOTIFY showExceptionDialogChanged)
	Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly NOTIFY readOnlyChanged)
private:
	typedef QTableView Super;
public:
	enum ActionGroup {ViewActions = 1, BlobActions = 2, SetValueActions = 4, RowActions = 8, CellActions = 16, SizeActions = 32,
					  CalculateActions = 64, ExportActions = 128, ImportActions = 256, SortActions = 512, SelectActions = 1024,
					  FilterActions = 2048, PasteActions = 4096, AllActions = 65535};
public:
	explicit TableView(QWidget *parent = nullptr);
	~TableView() Q_DECL_OVERRIDE;
public:
	enum InlineEditSaveStrategy {
		OnCurrentRowChange,
		OnEditedValueCommit,
		OnManualSubmit
	};
	enum RowEditorMode {
		EditRowsInline,
		EditRowsExternal,
		EditRowsMixed
	};
	enum RecordEditMode {
		ModeView = qf::gui::model::DataDocument::ModeView,
		ModeEdit = qf::gui::model::DataDocument::ModeEdit,
		ModeInsert = qf::gui::model::DataDocument::ModeInsert,
		ModeCopy = qf::gui::model::DataDocument::ModeCopy,
		ModeDelete = qf::gui::model::DataDocument::ModeDelete
	};

	QF_PROPERTY_IMPL2(InlineEditSaveStrategy, i, I, nlineEditSaveStrategy, OnEditedValueCommit)
	QF_PROPERTY_IMPL2(RowEditorMode, r, R, owEditorMode, EditRowsInline)
	QF_PROPERTY_BOOL_IMPL2(s, S, howExceptionDialog, true)
	//QF_PROPERTY_BOOL_IMPL(r, R, eadOnly)

public:
	QSortFilterProxyModel* sortFilterProxyModel() const;
	qf::gui::model::TableModel* tableModel() const;
	void setTableModel(qf::gui::model::TableModel* m);
	//Q_SIGNAL void tableModelChanged();
	Q_SIGNAL void currentRowChanged(int current_row);

	Q_SLOT virtual void refreshActions();
	QList<Action*> toolBarActions() const {return m_toolBarActions;}

	bool isReadOnly() const {return m_isReadOnly;}
	void setReadOnly(bool ro);
	Q_SIGNAL void readOnlyChanged(bool b);

	void setInsertRowEnabled(bool b);
	void setRemoveRowEnabled(bool b);
	void setCloneRowEnabled(bool b);
	void setEditRowsMenuSectionEnabled(bool b);
	void setDirtyRowsMenuSectionEnabled(bool b);

	Q_SLOT void resetColumnsSettings();

	Q_SLOT virtual void reload(bool preserve_sorting = false);

	Q_SLOT virtual void insertRow();
	Q_SLOT virtual void removeSelectedRows();
	//! @param row_no if @a row_no < 0 than post current row.
	Q_SLOT virtual bool postRow(int row_no = -1);
	//! discard all the row data changes.
	Q_SLOT virtual void revertRow(int row_no = -1);
	Q_SLOT int reloadCurrentRow() {return reloadRow(-1);}
	Q_SLOT int reloadRow(int row_no = -1);

	Q_SLOT virtual void cloneRowInline();
	Q_SLOT virtual void cloneRow();

	Q_SLOT virtual void copy();
	Q_SLOT virtual void copySpecial();
	Q_SLOT virtual void paste();

	Q_SLOT void setValueInSelection();
	Q_SLOT void setNullInSelection() {setValueInSelection_helper(QVariant());}

	Q_SLOT void editCellContentInEditor();

	Q_SLOT void exportCSV();

	Q_SLOT void exportReport();

	Q_SLOT void saveCurrentCellBlob();
	Q_SLOT void loadCurrentCellBlob();

	Q_SLOT void selectCurrentColumn();
	Q_SLOT void selectCurrentRow();

	/**
	* calls update viewport with rect clipping row \a row.
	* @param row if lower than 0 current row is updated.
	*/
	Q_SLOT void updateRow(int row = -1);
	Q_SLOT void updateDataArea();
	Q_SLOT void updateAll();

	//! If \a row_no < 0 row_no = current row.
	qf::core::utils::TableRow& tableRowRef(int row_no = -1);
	qf::core::utils::TableRow tableRow(int row_no = -1) const;
	qf::core::utils::TableRow selectedRow() const {return tableRow();}

	int logicalColumnIndex(const QString &field_name) const;

	QList<int> selectedRowsIndexes() const;
	QList<int> selectedColumnsIndexes() const;

	Q_SIGNAL void editCellRequest(const QModelIndex &table_view_index);
	Q_SIGNAL void editRowInExternalEditor(const QVariant &id, int mode);
	Q_SIGNAL void editSelectedRowsInExternalEditor(int mode);
	Q_SLOT virtual void rowExternallySaved(const QVariant &id, int mode);

	Q_SIGNAL void filterDialogRequest();
	Q_SLOT void filterByString(const QString &s);
	QString filterString() const;

	QModelIndex toTableModelIndex(const QModelIndex &table_view_index) const;
	int toTableModelRowNo(int table_view_row_no) const;

	Q_SLOT void setItemDelegateForColumn(int column, QAbstractItemDelegate *delegate) {Super::setItemDelegateForColumn(column, delegate);}

	Q_SIGNAL void sqlException(const QString &what, const QString &where, const QString &stack_trace);

	enum class ReplaceEscapes {Never, Always, QuoteIfNeeded};
	static void copySelectionToClipboard(const QTableView *table_view);
	void copySelectionToClipboard() const { copySelectionToClipboard(this); }

	Q_SLOT void loadPersistentSettings();
	Q_SLOT void savePersistentSettings();
protected:
	virtual bool postRowImpl(int row_no = -1);
private:
	Q_SIGNAL void seekStringChanged(const QString &str);
	//qf::core::utils::Table::SortDef seekSortDefinition() const;
	int seekColumn() const;
	void seek(const QString &prefix_str);
	void cancelSeek();

	qf::core::utils::TreeTable toTreeTable(const QString& table_name = QString(), const QVariantList& exported_columns = QVariantList(), const qf::gui::model::TableModel::TreeTableExportOptions &opts = qf::gui::model::TableModel::TreeTableExportOptions()) const;
	void exportReport_helper(const QVariant& _options);
	void exportCSV_helper(const QVariant& export_options);
protected:
	Q_SLOT virtual void onSqlException(const QString &what, const QString &where, const QString &stack_trace);

	void keyPressEvent(QKeyEvent *e) Q_DECL_OVERRIDE;
	void mousePressEvent(QMouseEvent *e) Q_DECL_OVERRIDE;
	void contextMenuEvent(QContextMenuEvent *e) Q_DECL_OVERRIDE;

	void updateGeometries() Q_DECL_OVERRIDE;

	bool edit(const QModelIndex& index, EditTrigger trigger, QEvent* event) Q_DECL_OVERRIDE;
	void commitData(QWidget *editor) Q_DECL_OVERRIDE;
	void currentChanged(const QModelIndex& current, const QModelIndex& previous) Q_DECL_OVERRIDE;

	virtual void insertRowInline();
	virtual void removeSelectedRowsInline();

	virtual void createActions();

	void setValueInSelection_helper(const QVariant &new_val);
	static void copySpecial_helper(const QTableView *table_view, const QString &fields_separator, const QString &rows_separator, const QString &field_quotes, ReplaceEscapes replace_escapes);
	Q_SLOT void generateSequenceInSelection();

	//static const int StandardContextMenuActionsGroups = AllActions & ~(SetValueActions | BlobActions | PasteActions);
	QList<Action*> standardContextMenuActions() { return contextMenuActionsForGroups(AllActions);}
	QList<Action*> contextMenuActions() { return m_contextMenuActions; }
	void setContextMenuActions(QList<Action*> lst) { m_contextMenuActions = lst; }
	QList<Action*> contextMenuActionsForGroups(int action_groups = AllActions);

	Action* action(const QString &act_oid);
	void enableAllActions(bool on);

	QAbstractProxyModel* lastProxyModel() const;
private:
	/// hide this function do disable filter proxymodel bypassing
	void setModel(QAbstractItemModel *model) Q_DECL_OVERRIDE;

	QWidget* cornerWidget() const;
	QAbstractButton* leftTopCornerButton() const;
protected:
	QString m_seekString;
	QMap<QString, Action*> m_actions;
	QMap<ActionGroup, Action*> m_separatorsForGroup;
	QMap<int, QStringList> m_actionGroups;
	QList<Action*> m_toolBarActions;
	QList<Action*> m_contextMenuActions;
	TableViewProxyModel *m_proxyModel;
	QAbstractButton *m_leftTopCornerButton = nullptr;
private:
	bool m_isReadOnly = false;
};

}}

#endif // QF_GUI_TABLEVIEW_H
