#include "findregistrationedit.h"

#include "eventplugin.h"

#include <qf/qmlwidgets/framework/mainwindow.h>
#include <qf/qmlwidgets/log.h>

#include <qf/core/model/sqltablemodel.h>
#include <qf/core/assert.h>

#include <QCompleter>
#include <QAbstractTableModel>
#include <QAbstractProxyModel>
#include <QKeyEvent>

using qf::qmlwidgets::framework::getPlugin;
using Event::EventPlugin;

class FindRegistrationsModel : public QAbstractTableModel
{
private:
	typedef QAbstractTableModel Super;
public:
	FindRegistrationsModel(QObject *parent);

	int columnCount(const QModelIndex &parent = QModelIndex()) const Q_DECL_OVERRIDE {Q_UNUSED(parent) return 1;}
	int rowCount(const QModelIndex &parent = QModelIndex()) const Q_DECL_OVERRIDE;
	QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const Q_DECL_OVERRIDE;
	const qf::core::utils::Table& registrationsTable() const;
private:
	mutable qf::core::utils::Table m_registrationsTable;
};

FindRegistrationsModel::FindRegistrationsModel(QObject *parent)
	: Super(parent)
{
	m_registrationsTable = getPlugin<EventPlugin>()->registrationsTable();
}

const qf::core::utils::Table &FindRegistrationsModel::registrationsTable() const
{
	return m_registrationsTable;
}

int FindRegistrationsModel::rowCount(const QModelIndex &parent) const
{
	Q_UNUSED(parent)
	const qf::core::utils::Table &table = registrationsTable();
	return table.rowCount();
}

QVariant FindRegistrationsModel::data(const QModelIndex &index, int role) const
{
	const qf::core::utils::Table &table = registrationsTable();
	int row_no = index.row();
	if(row_no >= 0 && row_no < table.rowCount()) {
		qf::core::utils::TableRow table_row = table.row(row_no);
		const qf::core::utils::Table::FieldList &fields = table.fields();
		static auto col_name = fields.fieldIndex(QStringLiteral("competitorName"));
		static auto col_searchkey = fields.fieldIndex(QStringLiteral("competitorNameAscii7"));
		static int col_registration = fields.fieldIndex(QStringLiteral("registration"));
		static auto col_siid = fields.fieldIndex(QStringLiteral("siid"));
		static auto SI = QStringLiteral("SI:");
		if(role == Qt::DisplayRole || role == Qt::EditRole) {
			return table_row.value(col_name).toString() + ' '
					+ table_row.value(col_registration).toString() + ' '
					+ SI + table_row.value(col_siid).toString();
		}
		if(role == FindRegistrationEdit::CompletionRole) {
			return table_row.value(col_searchkey).toString() + ' '
					+ table_row.value(col_registration).toString() + ' '
					+ SI + table_row.value(col_siid).toString() + ' '
					+ table_row.value(col_name).toString();
		}
	}
	return QVariant();
}

FindRegistrationEdit::FindRegistrationEdit(QWidget *parent)
	: Super(parent)
{	
	m_findRegistrationsModel = new FindRegistrationsModel(this);
	auto *cmpl = new QCompleter(m_findRegistrationsModel, this);
	cmpl->setCompletionRole(CompletionRole);
	cmpl->setCaseSensitivity(Qt::CaseInsensitive);
	cmpl->setFilterMode(Qt::MatchContains);
	setCompleter(cmpl);
}

void FindRegistrationEdit::focusInEvent(QFocusEvent *event)
{
	Super::focusInEvent(event);
	if(event->gotFocus()) {
		//qfInfo() << event->FocusIn;
		/// don't know why, but QCompleter::activated is not connected any more when FindRegistrationEdit loose its focus
		/// nasty hack is to reconnect signal every time when FindRegistrationEdit get focus again
		connect(completer(), qOverload<const QModelIndex &>(&QCompleter::activated), this, &FindRegistrationEdit::onCompleterActivated, Qt::UniqueConnection);
	}
}

void FindRegistrationEdit::onCompleterActivated(const QModelIndex &index)
{
	// qfLogFuncFrame() << index << index.data();
	auto *proxy_model = qobject_cast<QAbstractProxyModel*>(completer()->completionModel());
	Q_ASSERT(proxy_model != nullptr);
	QModelIndex ix = proxy_model->mapToSource(index);
	const qf::core::utils::Table &table = m_findRegistrationsModel->registrationsTable();
	int row_no = ix.row();
	if(row_no >= 0 && row_no < table.rowCount()) {
		qf::core::utils::TableRow table_row = table.row(row_no);
		emit registrationSelected(table_row.valuesMap(false));
	}
}
