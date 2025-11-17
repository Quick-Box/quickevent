#pragma once

#include "tablemodel.h"

#include <qf/core/sql/connection.h>
#include <qf/core/sql/querybuilder.h>
#include <qf/core/utils/table.h>
#include <qf/core/utils.h>

#include <QMap>
#include <QSqlError>

namespace qf::core::sql { struct QxRecChng; }
namespace qf::gui::sql { class Connection; }

namespace qf::gui::model {

class QFGUI_DECL_EXPORT SqlTableModel : public TableModel
{
	Q_OBJECT
	Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
	//Q_PROPERTY(QVariant queryParameters READ queryParameters WRITE setQueryParameters NOTIFY queryParametersChanged)
	Q_PROPERTY(QString connectionName READ connectionName WRITE setConnectionName NOTIFY connectionNameChanged)
private:
	typedef TableModel Super;
public:
	SqlTableModel(QObject *parent = nullptr);
	~SqlTableModel() Q_DECL_OVERRIDE;

	QF_PROPERTY_IMPL(QVariant, q, Q, ueryParameters)
	QF_PROPERTY_BOOL_IMPL(i, I, ncludeJoinedTablesIdsToReloadRowQuery)

public:
	class QFCORE_DECL_EXPORT DbEnumCastProperties : public QVariantMap
	{
		QF_VARIANTMAP_FIELD(QString, g, setG, roupName)
		QF_VARIANTMAP_FIELD2(QString, c, setC, aptionFormat, QStringLiteral("{{caption}}"))

	public:
		DbEnumCastProperties(const QVariantMap &m = QVariantMap()) : QVariantMap(m) {}
};
public:
	QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const Q_DECL_OVERRIDE;

	Q_INVOKABLE QString effectiveQuery();
	bool reload() Q_DECL_OVERRIDE;
	bool postRow(int row_no, bool throw_exc) Q_DECL_OVERRIDE;
	void revertRow(int row_no) Q_DECL_OVERRIDE;
	int reloadRow(int row_no) Q_DECL_OVERRIDE;
	int reloadInserts(const QString &id_column_name) Q_DECL_OVERRIDE;
	QString reloadRowQuery(const QVariant &record_id);

	Q_SIGNAL void qxRecChng(const qf::core::sql::QxRecChng &recchng);
public:
	void setQueryBuilder(const qf::core::sql::QueryBuilder &qb, bool clear_columns = false);
	const qf::core::sql::QueryBuilder& queryBuilder() const;

	QString connectionName() const { return m_connectionName; }
	void setConnectionName(QString arg)
	{
		if(m_connectionName != arg) {
			m_connectionName = arg;
			emit connectionNameChanged(arg);
		}
	}
	Q_SIGNAL void connectionNameChanged(QString arg);

	qf::core::sql::Connection sqlConnection() const;
	QString query() const { return m_query; }
	void setQuery(const QString &query_str);
	Q_SIGNAL void queryChanged(const QString &query_str);

	const QString& recentlyExecutedQueryString() const {return m_recentlyExecutedQueryString;}
	int recentlyExecutedQueryRowsAffected() const {return m_recentlyExecutedQueryRowsAffected;}
	const QSqlError& recentlyExecutedQueryError() const {return m_recentlyExecutedQueryError;}

	void addForeignKeyDependency(const QString &master_table_key, const QString &slave_table_key);
protected:
	virtual QString buildQuery();
	virtual QString replaceQueryParameters(const QString query_str);

	bool reloadQuery(const QString &query_str);

	virtual bool reloadTable(const QString &query_str);
	QStringList tableIds(const qf::core::utils::Table::FieldList &table_fields);
	void setSqlFlags(qf::core::utils::Table::FieldList &table_fields, const QString &query_str) const;

	QSet<QString> referencedForeignTables();
	QStringList tableIdsSortedAccordingToForeignKeys();

	bool removeTableRow(int row_no, bool throw_exc = false) Q_DECL_OVERRIDE;
protected:
	qf::core::sql::QueryBuilder m_queryBuilder;
	QString m_query;
	QString m_connectionName;
	QString m_recentlyExecutedQueryString;
	int m_recentlyExecutedQueryRowsAffected;
	QSqlError m_recentlyExecutedQueryError;
	/// INSERT needs to know dependency of tables in joined queries to insert particular tables in proper order
	QMap<QString, QString> m_foreignKeyDependencies;
};

}

