#include "sqltablemodel.h"

#include <qf/core/assert.h>
#include <qf/core/utils.h>
#include <qf/core/exception.h>
#include <qf/core/sql/connection.h>
#include <qf/core/sql/dbenum.h>
#include <qf/core/sql/dbenumcache.h>
#include <qf/core/sql/query.h>
#include <qf/core/sql/qxrecchng.h>

#include <QSqlRecord>
#include <QSqlIndex>
#include <QSqlField>
#include <QSqlDriver>
#include <QJSValue>
#include <QColor>
#include <QSet>

namespace qfs = qf::core::sql;
namespace qfu = qf::core::utils;

namespace qf::gui::model {

SqlTableModel::SqlTableModel(QObject *parent)
	: Super(parent)
{
	m_connectionName = QSqlDatabase::defaultConnection;
}

SqlTableModel::~SqlTableModel() = default;

QVariant SqlTableModel::data(const QModelIndex &index, int role) const
{
	if(role == Qt::DisplayRole) {
		ColumnDefinition cd = columnDefinition(index.column());
		int cast_type = cd.castType();
		if(cast_type == qMetaTypeId<qf::core::sql::DbEnum>()) {
			DbEnumCastProperties props(cd.castProperties());
			QString group_name = props.groupName();
			if(!group_name.isEmpty()) {
				QVariant v = data(index, Qt::EditRole);
				qf::core::sql::DbEnumCache& db_enum_cache = qf::core::sql::DbEnumCache::instanceForConnection(connectionName());
				QString group_id = v.toString();
				qf::core::sql::DbEnum dbe = db_enum_cache.dbEnum(group_name, group_id);
				QString caption_format = props.captionFormat();
				QString caption = dbe.fillInPlaceholders(caption_format);
				return caption;
			}
		}
	}
	else if(role == Qt::BackgroundRole) {
		ColumnDefinition cd = columnDefinition(index.column());
		int cast_type = cd.castType();
		if(cast_type == qMetaTypeId<qf::core::sql::DbEnum>()) {
			DbEnumCastProperties props(cd.castProperties());
			QString group_name = props.groupName();
			if(!group_name.isEmpty()) {
				QVariant v = data(index, Qt::EditRole);
				QString group_id = v.toString();
				if(!group_id.isEmpty()) {
					qf::core::sql::DbEnumCache& db_enum_cache = qf::core::sql::DbEnumCache::instanceForConnection(connectionName());
					qf::core::sql::DbEnum dbe = db_enum_cache.dbEnum(group_name, group_id);
					QColor color = dbe.color();
					if(color.isValid())
						return color;
				}
			}
			return QVariant();
		}
	}
	else if(role == Qt::ForegroundRole) {
		ColumnDefinition cd = columnDefinition(index.column());
		int cast_type = cd.castType();
		if(cast_type == qMetaTypeId<qf::core::sql::DbEnum>()) {
			QVariant v = data(index, Qt::BackgroundRole);
			if(v.isValid()) {
				auto bgr_color = v.value<QColor>();
				if(bgr_color.isValid()) {
					return contrastTextColor(bgr_color);
				}
			}
		}
	}
	QVariant ret = Super::data(index, role);
	return ret;
}

void SqlTableModel::setQueryBuilder(const qf::core::sql::QueryBuilder &qb, bool clear_columns)
{
	m_queryBuilder = qb;
	if(clear_columns)
		clearColumns();
}

const qf::core::sql::QueryBuilder &SqlTableModel::queryBuilder() const
{
	return m_queryBuilder;
}

void SqlTableModel::addForeignKeyDependency(const QString &master_table_key, const QString &slave_table_key)
{
	m_foreignKeyDependencies[master_table_key] = slave_table_key;
}

QString SqlTableModel::effectiveQuery()
{
	QString qs = buildQuery();
	qs = replaceQueryParameters(qs);
	return qs;
}

bool SqlTableModel::reload()
{
	QString qs = effectiveQuery();
	return reloadQuery(qs);
}

bool SqlTableModel::reloadQuery(const QString &query_str)
{
	qfLogFuncFrame() << query_str;
	beginResetModel();
	bool ok = reloadTable(query_str);
	checkColumns();
	endResetModel();
	emit reloaded();
	return ok;
}

bool SqlTableModel::postRow(int row_no, bool throw_exc)
{
	qfLogFuncFrame() << row_no;

	QF_ASSERT(m_table.isValidRowIndex(row_no),
			  QString("row: %1 is out of range of rows (%2)").arg(row_no).arg(m_table.rowCount()),
			  return false);
	qfu::TableRow &row_ref = m_table.rowRef(row_no);
	bool ret = true;
	if(row_ref.isInsert()) {
		qfDebug() << "\tINSERT";

		//QSet<QString> referenced_foreign_tables = referencedForeignTables();
		//int tbl_cnt = 0;
		qf::core::sql::Connection sql_conn = sqlConnection();
		QSqlDriver *sqldrv = sql_conn.driver();
		const QStringList table_ids = tableIdsSortedAccordingToForeignKeys();
		for(const QString &table_id : table_ids) {
			qfDebug() << "\ttable:" << table_id;
			QSqlRecord rec;
			int i = -1;
			int serial_ix = -1;
			bool serial_ix_explicitly_set = false;
			int primary_ix = -1;
			QVariantMap qx_record;
			Q_FOREACH(const qf::core::utils::Table::Field &fld, row_ref.fields()) {
				i++;
				if(fld.tableId() != table_id)
					continue;
				//qfInfo() << table_id << "field:" << fld.name() << "is serial:" << fld.isSerial();
				bool is_field_dirty = row_ref.isDirty(i);
				if(fld.isSerial()) {
					/// always include serial fields, an empty line cannot be inserted in other case
					//is_field_dirty = true;
					serial_ix = i;
					serial_ix_explicitly_set = is_field_dirty;
					qfDebug() << "\t serial ix:" << serial_ix;
				}
				if(fld.isPriKey()) {
					/// always include prikey field, an empty line cannot be inserted in other case
					//is_field_dirty = true;
					primary_ix = i;
					qfDebug() << "\t primary ix:" << primary_ix;
				}
				if(!is_field_dirty)
					continue;
				QVariant v = row_ref.value(i);
				qfDebug() << "\tdirty field:" << fld.name() << "type:" << v.typeName() << "val:" << v.toString();
				if(is_field_dirty) {
					/// null hodnotu nema smysl insertovat, pokud to neni nutne kvuli necemu jinemu
					/// naopak se pri insertu dokumentu z vice join tabulek muze stat, ze se vlozi to not-null fieldu null hodnota
					/// pokud je insert, join radek neexistuje, takze hodnoty jsou null a mysql v takovem pripade v resultsetu nastavi
					/// i not-null field na nullable
					QSqlField new_fld(fld.shortName(), fld.type());
					//if(v.isNull() && fld.isSerial())
					//	v = 0;
					new_fld.setValue(v);
					//qfInfo() << "\t\t" << "val is QString:" << (v.metaType().id() == QMetaType::QString);
					rec.append(new_fld);
					qx_record[fld.shortName().toLower()] = v;
				}
			}

			qfDebug() << "updating table inserts" << table_id;
			QString qs;
			QString table = sql_conn.fullTableNameToQtDriverTableName(table_id);
			qfs::Query q(sql_conn);
			if(rec.isEmpty()) {
				if(serial_ix >= 0) {
					qs = "INSERT INTO %1 DEFAULT VALUES";
					qs = qs.arg(table);
				}
			}
			else {
				qs = sqldrv->sqlStatement(QSqlDriver::InsertStatement, table, rec, false);
			}
			if(qs.isEmpty()) {
				continue;
			}
			qfDebug() << "\texecuting query:" << qs;
			bool ok = q.exec(qs);
			if(ok) {
				qfDebug() << "\tnum rows affected:" << q.numRowsAffected();
				int num_rows_affected = q.numRowsAffected();
				QF_ASSERT(num_rows_affected == 1,
						  tr("numRowsAffected() = %1, should be 1\n%2").arg(num_rows_affected).arg(qs),
						  return false);
				auto insert_id = q.lastInsertId();
				if(serial_ix >= 0 && !serial_ix_explicitly_set) {
					qfDebug() << "\tsetting serial index:" << serial_ix << "to generated value:" << insert_id;
					if(insert_id.isValid()) {
						row_ref.setValue(serial_ix, insert_id);
						row_ref.setDirty(serial_ix, false);
					}
					else {
						qfWarning() << "Serial field will not be initialized properly. This can make current transaction aborted.";
					}
				}
				if(primary_ix >= 0) {
					/// update foreign keys in the slave tables
					qf::core::utils::Table::Field pri_ix_fld = row_ref.fields().value(primary_ix);
					QString master_key = pri_ix_fld.name();
					qfDebug() << "\t master_key:" << master_key;
					QString slave_key = m_foreignKeyDependencies.value(master_key);
					if(!slave_key.isEmpty()) {
						qfDebug() << "\tsetting value of foreign key" << slave_key << "to value of master key:" << row_ref.value(master_key).toString();
						row_ref.setValue(slave_key, row_ref.value(master_key));
					}
					if (!qx_record.isEmpty() && master_key == "id") {
						qf::core::sql::QxRecChng chng {
							.table = table_id,
							.id = insert_id.toInt(),
							.record = qx_record,
							.op = qf::core::sql::QxRecOp::Insert,
						};
						emit qxRecChng(chng);
					}
				}
			}
			else {
				QString errs = tr("Error executing query: %1\n %2").arg(qs).arg(q.lastError().text());
				if(throw_exc)
					QF_EXCEPTION(errs);
				else
					qfError() << errs;
			}
		}
	}
	else {
		qfDebug() << "\tEDIT";
		qf::core::sql::Connection sql_conn = sqlConnection();
		QSqlDriver *sqldrv = sql_conn.driver();
		Q_FOREACH(QString table_id, tableIds(m_table.fields())) {
			qfDebug() << "\ttableid:" << table_id;
			QVariantMap qx_record;
			QSqlRecord edit_rec;
			int i = -1;
			Q_FOREACH(qfu::Table::Field fld, row_ref.fields()) {
				i++;
				//qfDebug() << "\t\tfield:" << fld.toString();
				if(fld.tableId() != table_id)
					continue;
				if(!row_ref.isDirty(i))
					continue;
				if(!fld.canUpdate()) {
					qfWarning() << "field" << fld.name() << "is dirty, but it has not canUpdate flag";
					continue;
				}
				QVariant v = row_ref.value(i);
				//qfDebug() << "\ttableid:" << tableid << "fullTableName:" << fld.fullTableName();
				qfDebug() << "\tdirty field" << fld.name() << "type:" << fld.type().id() << "orig val:" << row_ref.origValue(i).toString() << "new val:" << v.toString();
				//qfDebug().noSpace() << "\tdirty value: '" << v.toString() << "' isNull(): " << v.isNull() << " type(): " << v.type();
				QSqlField sqlfld(fld.shortName().toLower(), fld.type());
				sqlfld.setValue(v);
				//if(sqlfld.type() == QVariant::ByteArray)
				//	has_blob_field = true;
				qfDebug() << "\tfield is null: " << sqlfld.isNull();
				edit_rec.append(sqlfld);
				qx_record[fld.shortName()] = v;
			}
			if(!edit_rec.isEmpty()) {
				qfDebug() << "updating table edits:" << table_id;
				QString query_str;
				query_str += sqldrv->sqlStatement(QSqlDriver::UpdateStatement, table_id, edit_rec, false);
				query_str += " ";
				QSqlRecord where_rec;
				qfDebug() << "looking for primary index of table:" << table_id;
				std::optional<int> id_pri_key_value;
				auto pri_keys = sql_conn.primaryIndexFieldNames(table_id);
				for (const auto &fld_name : pri_keys) {
					QString full_fld_name = table_id + '.' + fld_name;
					qfDebug() << "\t checking value of field:" << full_fld_name;
					int fld_ix = m_table.fields().fieldIndex(full_fld_name);
					QF_ASSERT(fld_ix >= 0,
							  QString("Cannot find field '%1'").arg(full_fld_name),
							  continue);
					qfu::Table::Field fld = m_table.fields().at(fld_ix);
					qfDebug() << "\t found field:" << fld.name() << "at index:" << fld_ix;
					QSqlField sqlfld(fld.shortName(), fld.type());
					sqlfld.setValue(row_ref.origValue(fld_ix));
					qfDebug() << "\tpri index field" << full_fld_name << "type:" << sqlfld.metaType().id() << "orig val:" << row_ref.origValue(fld_ix) << "current val:" << row_ref.value(fld_ix);
					where_rec.append(sqlfld);
					if (auto id = sqlfld.value().toInt(); id > 0) {
						id_pri_key_value = id;
					}
				}
				QF_ASSERT(!where_rec.isEmpty(),
						  QString("pri keys values not generated for table '%1'").arg(table_id),
						  continue);

				query_str += sqldrv->sqlStatement(QSqlDriver::WhereStatement, table_id, where_rec, false);
				qfDebug() << "save edit query:" << query_str;
				qfs::Query q(sql_conn);
				bool ok;
				ok = q.exec(query_str);
				if(!ok && throw_exc) {
					QF_EXCEPTION(q.lastError().text());
				}
				int num_rows_affected = q.numRowsAffected();
				qfDebug() << "\tnum rows affected:" << num_rows_affected;
				/// if update command does not really change data for ex. (UPDATE woffice.kontakty SET id=8 WHERE id = 8)
				/// numRowsAffected() returns 0.
				if(num_rows_affected > 1) {
					qfError() << QString("numRowsAffected() = %1, sholuld be 1 or 0\n%2").arg(num_rows_affected).arg(query_str);
					ret = false;
					break;
				}
				if (!qx_record.isEmpty() && pri_keys.size() == 1 && pri_keys[0] == "id" && id_pri_key_value.has_value()) {
					qf::core::sql::QxRecChng chng {
						.table = table_id,
						.id = id_pri_key_value.value(),
						.record = qx_record,
						.op = qf::core::sql::QxRecOp::Update,
					};
					emit qxRecChng(chng);
				}
			}
		}
	}
	if(ret) {
		ret = Super::postRow(row_no, throw_exc);
	}
	return ret;
}

bool SqlTableModel::removeTableRow(int row_no, bool throw_exc)
{
	qfLogFuncFrame();
	bool ret = false;
	qfu::TableRow row = m_table.row(row_no);
	if(!row.isInsert()) {
		qf::core::sql::Connection sql_conn = sqlConnection();
		qf::core::sql::Connection dbinfo(sql_conn);
		QSqlDriver *sqldrv = sql_conn.driver();

		if(row.isDirty())
			row.restoreOrigValues();
		QStringList table_ids = tableIdsSortedAccordingToForeignKeys();
		QSet<QString> referenced_foreign_tables = referencedForeignTables();
		int table_id_cnt = 0;
		Q_FOREACH(const QString &table_id, table_ids) {
			/// Allways delete in first table
			if(table_id_cnt++ > 0) {
				/// delete in rest of the tables only if they are implicitly referenced, see: addForeignKeyDependency(...)
				if(!referenced_foreign_tables.contains(table_id)) {
					qfDebug() << "\tskipping table" << table_id;
					continue;
				}
			}

			QString table = dbinfo.fullTableNameToQtDriverTableName(table_id);
			QSqlRecord rec;
			qfDebug() << "deleting in table" << table;
			QString query_str;
			query_str += sqldrv->sqlStatement(QSqlDriver::DeleteStatement, table, rec, false);
			query_str += " ";
			QSqlRecord where_rec;
			Q_FOREACH(QString fld_name, sql_conn.primaryIndexFieldNames(table_id)) {
				QString full_fld_name = table_id + '.' + fld_name;
				int fld_ix = m_table.fields().fieldIndex(full_fld_name);
				QF_ASSERT(fld_ix >= 0,
						  QString("Cannot find field '%1'").arg(full_fld_name),
						  continue);
				qfu::Table::Field fld = m_table.fields().at(fld_ix);
				QVariant id = row.value(fld_ix);
#if QT_VERSION_MAJOR >= 6
				QSqlField sqlfld(fld.shortName(), fld.type());
#else
				QSqlField sqlfld(fld.shortName(), static_cast<QVariant::Type>(fld.type().id()));
#endif
				sqlfld.setValue(id);
				bool invalid_id = false;
				if(id.isNull())
					invalid_id = true; // NOLINT(bugprone-branch-clone)
#if QT_VERSION_MAJOR >= 6
				else if(id.typeId() == QMetaType::Int && id.toInt() == 0)
#else
				else if(id.type() == QVariant::Int && id.toInt() == 0)
#endif
					invalid_id= true;
				if(!invalid_id) {
					sqlfld.setValue(id);
					qfDebug() << "\tpri index" << fld_name << ":" << sqlfld.value().toString();
					rec.append(sqlfld);
				}
				else {
					qfWarning() << "DELETE Invalid row ID:" << id.toString() << "type:" << id.typeName();
					break;
				}
				//qfDebug() << "\tpri index" << f.name() << ":" << f.value().toString() << "value type:" << QVariant::typeToName(f.value().type()) << "field type:" << QVariant::typeToName(f.type());
				where_rec.append(sqlfld);
			}
			QF_ASSERT(!where_rec.isEmpty(),
					  QString("pri keys values not generated for table '%1'").arg(table_id),
					  continue);

			query_str += sqldrv->sqlStatement(QSqlDriver::WhereStatement, table_id, where_rec, false);
			qfDebug() << "drop row query:" << query_str;
			qfs::Query q(sql_conn);
			ret = q.exec(query_str);
			if(!ret) {
				if(throw_exc) {
					QF_EXCEPTION(q.lastError().text());
				}
				else {
					qfError() << q.lastError().text();
					break;
				}
			}
			qfDebug() << "\tnum rows affected:" << q.numRowsAffected();
			int num_rows_affected = q.numRowsAffected();
			/// if update command does not really change data for ex. (UPDATE woffice.kontakty SET id=8 WHERE id = 8)
			/// numRowsAffected() returns 0.
			if(num_rows_affected != 1) {
				qfError() << QString("numRowsAffected() = %1, sholuld be 1\n%2").arg(num_rows_affected).arg(query_str);
				ret = false;
				break;
			}
			if (where_rec.count() == 1 && where_rec.field(0).name() == "id") {
				if (auto id = where_rec.field(0).value().toInt(); id < 0) {
					qf::core::sql::QxRecChng chng {
						.table = table_id,
						.id = id,
						.record = {},
						.op = qf::core::sql::QxRecOp::Delete,
					};
					emit qxRecChng(chng);
				}
			}
		}
	}
	if(ret) {
		ret = Super::removeTableRow(row_no, throw_exc);
	}
	return ret;
}

void SqlTableModel::revertRow(int row_no)
{
	qfLogFuncFrame() << row_no;
	Super::revertRow(row_no);
}

QString SqlTableModel::reloadRowQuery(const QVariant &record_id)
{
	qf::core::sql::QueryBuilder qb = m_queryBuilder;
	if(qb.isEmpty()) {
		qfWarning() << "Empty queryBuilder";
		return {};
	}
	auto sql_id_str = qMetaTypeId<QString>() == record_id.userType()? '\'' + record_id.toString() + '\'': QString::number(record_id.toInt());
	qb.where(idColumnName() + "=" + sql_id_str);
	qfs::QueryBuilder::BuildOptions opts;
	opts.setConnectionName(connectionName());
	QString query_str = qb.toString(opts);
	query_str = replaceQueryParameters(query_str);
	return query_str;
}

int SqlTableModel::reloadRow(int row_no)
{
	qfLogFuncFrame() << "row:" << row_no << "row count:" << rowCount();
#ifdef NO_RELOAD_ROW_QUERY
	qf::core::sql::QueryBuilder qb = m_queryBuilder;
	if(qb.isEmpty()) {
		qfWarning() << "Empty queryBuilder";
		return false;
	}
	qfu::TableRow &row_ref = m_table.rowRef(row_no);
	qf::core::sql::Connection sql_conn = sqlConnection();
	QSqlDriver *sqldrv = sql_conn.driver();
	for (const auto &table_id : tableIds(m_table.fields())) {
		qfDebug() << "\ttableid:" << table_id;
		for (const auto &fld_name : sql_conn.primaryIndexFieldNames(table_id)) {
			QString full_fld_name = table_id + '.' + fld_name;
			int fld_ix = m_table.fields().fieldIndex(full_fld_name);
			if(fld_ix < 0) {
				// result may not contain all the tables primary keys
				// for example SELECT competitors.*, classes.name FROM competitors LEFT JOIN classes ON competitors.classId=classes.id
				// doesn't contain classes.id and still can be reloaded
				continue;
			}
			QVariant pri_ix_val = row_ref.value(fld_ix);
			if(pri_ix_val.isNull()) {
				// LEFT JOIN-ed table with missing record, don't include this constrain in reload row query
				continue;
			}
			qfu::Table::Field fld = m_table.fields().at(fld_ix);
#if QT_VERSION_MAJOR >= 6
			QSqlField sqlfld(fld.shortName(), fld.type());
#else
			QSqlField sqlfld(fld.shortName(), static_cast<QVariant::Type>(fld.type().id()));
#endif
			// cannot use origValue() here, since reloadRow() must work for even edited primary keys
			sqlfld.setValue(pri_ix_val);
			QString formated_val = sqldrv->formatValue(sqlfld);
			qb.where(full_fld_name + "=" + formated_val);
		}
		if(!isIncludeJoinedTablesIdsToReloadRowQuery())
			break;
	}
	qfs::QueryBuilder::BuildOptions opts;
	opts.setConnectionName(connectionName());
	QString query_str = qb.toString(opts);
	query_str = replaceQueryParameters(query_str);
#else
	qfu::TableRow &row_ref = m_table.rowRef(row_no);
	auto id = row_ref.value(idColumnName());
	auto query_str = reloadRowQuery(id);
	qf::core::sql::Connection sql_conn = sqlConnection();
#endif
	qfDebug() << "\t reload row query:" << query_str;
	qfs::Query q = qfs::Query(sql_conn);
	bool ok = q.exec(query_str);
	QF_ASSERT(ok == true,
			  QString("SQL Error: %1\n%2").arg(q.lastError().text()).arg(query_str),
			  return false);
	int row_cnt = 0;
	while(q.next()) {
		if(row_cnt++) {
			qfWarning() << "More than 1 row returned by query.\n" + query_str;
			continue;
		}
		for(int i=0; i<row_ref.fields().count(); i++) {
			row_ref.setBareBoneValue(i, q.value(i));
			qfDebug() << "\t\t" << i << "->" << row_ref.value(i).toString();
		}
	}
	Super::reloadRow(row_no);
	return row_cnt;
}

int SqlTableModel::reloadInserts(const QString &id_column_name)
{
	qfLogFuncFrame();
	int id_ix = columnIndex(id_column_name);
	QF_ASSERT(id_ix >= 0, QString("ID column name '%1' not found").arg(id_column_name), return 0);
	int max_id = 0;
	for (int i = 0; i < rowCount(); ++i) {
		int id = value(i, id_ix).toInt();
		max_id = qMax(max_id, id);
	}

	qf::core::sql::QueryBuilder qb = m_queryBuilder;
	if(qb.isEmpty()) {
		qfWarning() << "Empty queryBuilder";
		return false;
	}

	qb.takeOrderBy();
	qb.orderBy(id_column_name);
	qb.where(id_column_name + " > " + QF_IARG(max_id));
	qfs::QueryBuilder::BuildOptions opts;
	opts.setConnectionName(connectionName());
	QString query_str = qb.toString(opts);
	query_str = replaceQueryParameters(query_str);
	qfDebug() << "\t reload row query:" << query_str;
	qf::core::sql::Connection sql_conn = sqlConnection();
	qfs::Query q = qfs::Query(sql_conn);
	bool ok = q.exec(query_str);
	QF_ASSERT(ok == true,
			  QString("SQL Error: %1\n%2").arg(q.lastError().text()).arg(query_str),
			  return 0);
	int ret = 0;
	while(q.next()) {
		qfu::TableRow &row = m_table.insertRow(0);
		row.setInsert(false);
		int fld_cnt = m_table.fields().count();
		for(int i=0; i<fld_cnt; i++) {
			row.setBareBoneValue(i, q.value(i));
		}
		ret++;
	}
	if(ret > 0) {
		beginInsertRows(QModelIndex(), 0, ret - 1);
		endInsertRows();
	}
	return ret;
}

QString SqlTableModel::buildQuery()
{
	QString ret = query();
	if(ret.isEmpty()) {
		qfs::QueryBuilder::BuildOptions opts;
		opts.setConnectionName(connectionName());
		ret = m_queryBuilder.toString(opts);
	}
	return ret;
}

namespace {
QString paramValueToString(const QVariant &v)
{
	QString ret;
	if(v.isValid())
		ret = v.toString();
	else
		ret = QStringLiteral("NULL");
	return ret;
}
}

QString SqlTableModel::replaceQueryParameters(const QString query_str)
{
	QString ret = query_str;
	QVariant par_v = queryParameters();
	if(par_v.userType() == qMetaTypeId<QJSValue>()) {
		auto jsv = par_v.value<QJSValue>();
		par_v = jsv.toVariant();
	}
#if QT_VERSION_MAJOR >= 6
	if(par_v.typeId() == QMetaType::QVariantMap) {
#else
	if(par_v.type() == QVariant::Map) {
#endif
		QVariantMap par_map = par_v.toMap();
		QMapIterator<QString, QVariant> it(par_map);
		while(it.hasNext()) {
			it.next();
			QString key = "{{" + it.key() + "}}";
			QString val_str = paramValueToString(it.value());
			ret.replace(key, val_str, Qt::CaseInsensitive);
		}
	}
	else {
		QString val_str = paramValueToString(par_v);
		ret.replace("{{id}}", val_str, Qt::CaseInsensitive);
	}
	return ret;
}

qf::core::sql::Connection SqlTableModel::sqlConnection() const
{
	QSqlDatabase db = QSqlDatabase::database(connectionName());
	qf::core::sql::Connection ret = qf::core::sql::Connection(db);
	QF_CHECK(ret.isValid(),
			 QString("Invalid sql connection for name '%1'").arg(connectionName()));
	return ret;
}

void SqlTableModel::setQuery(const QString &query_str)
{
	if (m_query != query_str) {
		m_query = query_str;
		if(m_columnsAutoGenerated)
			clearColumns();
		emit queryChanged(query_str);
	}
}

bool SqlTableModel::reloadTable(const QString &query_str)
{
	qfLogFuncFrame() << query_str;
	qf::core::sql::Connection sql_conn = sqlConnection();
	qfs::Query q(sql_conn);
	m_recentlyExecutedQueryString = query_str;
	m_recentlyExecutedQueryError = {};
	bool ok = q.exec(query_str);
	if(!ok) {
		qfError() << QString("SQL Error: %1\n%2").arg(q.lastError().text()).arg(query_str);
		m_recentlyExecutedQueryError = q.lastError().text();
		return false;
	}
	if(q.isSelect()) {
		bool retype_sqlite_null_values = sql_conn.driverName().endsWith(QLatin1String("SQLITE"), Qt::CaseInsensitive);
		qfu::Table::FieldList table_fields;
		QSqlRecord rec = q.record();
		int fld_cnt = rec.count();
		//qfInfo() << query_str;
		for(int i=0; i<fld_cnt; i++) {
			QSqlField rec_fld = rec.field(i);
			//qfInfo() << rec_fld.name() << rec_fld.type() << QVariant::typeToName(rec_fld.type());
#if QT_VERSION_MAJOR >= 6
			qfu::Table::Field fld(rec_fld.name(), rec_fld.metaType().id());
#else
			qfu::Table::Field fld(rec_fld.name(), rec_fld.type());
#endif
			table_fields << fld;
		}
		setSqlFlags(table_fields, query_str);
		m_table = qfu::Table(table_fields);
		while(q.next()) {
			qfu::TableRow &row = m_table.appendRow();
			row.setInsert(false);
			for(int i=0; i<fld_cnt; i++) {
				QVariant v = q.value(i);
				//qfInfo() << table_fields.value(i).name() << table_fields.value(i).type() << i << v << "null:" << v.isNull();
				if(retype_sqlite_null_values) {
					// SQLite driver reports NULL values as QString()
					if(v.isNull())
#if QT_VERSION_MAJOR >= 6
						v = QVariant(table_fields.value(i).type());
#else
						v = QVariant(static_cast<QVariant::Type>(table_fields.value(i).type().id()));
#endif
				}
				//qfWarning() << table_fields.value(i).name() << table_fields.value(i).type() << i << v << "null:" << v.isNull();
				row.setBareBoneValue(i, v);
			}
		}
	}
	return true;
}

namespace {
QString compose_table_id(const QString &table_name, const QString &schema_name)
{
	QString ret = table_name;
	if(!schema_name.isEmpty())
		ret = schema_name + '.' + ret;
	return ret;
}
}

QStringList SqlTableModel::tableIds(const core::utils::Table::FieldList &table_fields)
{
	QStringList ret;
	int fld_cnt = table_fields.count();
	for(int i=0; i<fld_cnt; i++) {
		const qfu::Table::Field &fld = table_fields[i];
		QString fs, ts, ds;
		qf::core::Utils::parseFieldName(fld.name(), &fs, &ts, &ds);
		ts = compose_table_id(ts, ds);
		if(!ts.isEmpty() && !ret.contains(ts))
			ret << ts;
	}
	return ret;
}

namespace {
QMap< QString, QSet<QString> > separateFields(const qf::core::utils::Table::FieldList &table_fields)
{
	QMap< QString, QSet<QString> > field_ids;
	Q_FOREACH(const qfu::Table::Field &fld, table_fields) {
		QString fn, tn;
		qf::core::Utils::parseFieldName(fld.name(), &fn, &tn);
		if(!tn.isEmpty())
			field_ids[tn] << fn;
	}
	return field_ids;
}
}

void SqlTableModel::setSqlFlags(qf::core::utils::Table::FieldList &table_fields, const QString &query_str) const
{
	//qfLogFuncFrame();
	//QSet<QString> table_ids = tableIds(table_fields);
	QMap< QString, QSet<QString> > field_ids = separateFields(table_fields);
	if(field_ids.isEmpty()) {
		/// SQL driver doesn't support table names in returned QSqlRecord
		/// try to guess it from select
		int ix1 = query_str.indexOf(QLatin1String(" JOIN "), Qt::CaseInsensitive);
		if(ix1 < 0) {
			QLatin1String from(" FROM ");
			ix1 = query_str.indexOf(from, Qt::CaseInsensitive);
			if(ix1 > 0) {
				ix1 += from.size();
				int ix2 = query_str.indexOf(' ', ix1);
				if(ix2 < 0)
					ix2 = query_str.length();
				if(ix2 > ix1) {
					QString table_id_from_query = query_str.mid(ix1, ix2 - ix1);
					qf::core::Utils::parseFieldName(table_id_from_query, &table_id_from_query);
					for(qfu::Table::Field &fld : table_fields) {
						fld.setName(table_id_from_query + '.' + fld.name());
					}
				}
				field_ids = separateFields(table_fields);
			}
		}
	}
	QMap<QString, QString> serial_field_names;
	QSet<QString> updateable_table_ids;
	Q_FOREACH(QString table_id, field_ids.keys()) {
		QString serial_field_name = sqlConnection().serialFieldName(table_id);
		//qfInfo() << "serial field for table id:" << table_id << "is:" << serial_field_name;
		serial_field_names[table_id] = serial_field_name;
		const QStringList prikey_fields = sqlConnection().primaryIndexFieldNames(table_id);
		bool ok = true;
		QSet<QString> flds = field_ids.value(table_id);
		Q_FOREACH(auto pk_f, prikey_fields) {
			//qDebug() << "\t checking if query contains primary key:" << pk_f << "in table id:" << table_id;
			if(!flds.contains(pk_f)) {
				ok = false;
				break;
			}
		}
		if(ok) {
			//qDebug() << "\t setting update flag on table id:" << table_id;
			updateable_table_ids << table_id;
		}
	}
	for(qfu::Table::Field &fld : table_fields) {
		QString table_id = fld.tableId();
		if(!table_id.isEmpty()) {
			fld.setCanUpdate(updateable_table_ids.contains(table_id));
			QStringList prikey_fields = sqlConnection().primaryIndexFieldNames(table_id);
			fld.setPriKey(prikey_fields.contains(fld.shortName()));
			//qfDebug() << "table_id;" << table_id << "fldname:" << fld.shortName() << "serial_field_names.value(table_id):" << serial_field_names.value(table_id);
			fld.setSerial(serial_field_names.value(table_id) == fld.shortName());
			//qfDebug() << fld.name() << "is serial:" << fld.isSerial();
		}
		//qfInfo() << fld.toString();
	}
}

QSet<QString> SqlTableModel::referencedForeignTables()
{
	qfLogFuncFrame();
	QSet<QString> ret;
	{
		const QStringList sl = m_foreignKeyDependencies.values();
		for(const auto &s : sl) {
			QString tbl_name;
			qf::core::Utils::parseFieldName(s, nullptr, &tbl_name);
			tbl_name = tbl_name.trimmed().toLower();
			if(!tbl_name.isEmpty()) {
				//qfDebug() << "\t referenced_foreign_tables <<" << tbl_name;
				ret << tbl_name;
			}
		}
	}
	//qfDebug() << "\t returning:" << QStringList(referenced_foreign_tables.toList()).join(", ");
	return ret;
}

QStringList SqlTableModel::tableIdsSortedAccordingToForeignKeys()
{
	qfLogFuncFrame();
	QStringList ret;
	QStringList table_ids = tableIds(m_table.fields());

	while(!table_ids.isEmpty()) {
		int table_ix;
		for(table_ix=0; table_ix<table_ids.count(); table_ix++) {
			QString table_id = table_ids[table_ix];
			bool dependency_satisfied = true;
			/// check if this table_id depends on some different table
			QMapIterator<QString, QString> it(m_foreignKeyDependencies);
			while(it.hasNext()) {
				it.next();
				QString master_key = it.key();
				QString slave_key = it.value();
				QString field, table, schema;
				qf::core::Utils::parseFieldName(slave_key, &field, &table, &schema);
				slave_key = qf::core::Utils::composeFieldName(table, schema); // NOLINT(readability-suspicious-call-argument)
				if(qf::core::Utils::fieldNameCmp(table_id, slave_key)) {
					dependency_satisfied = false;
					/// table_id is a slave, so check if master table is in return list already
					qf::core::Utils::parseFieldName(master_key, &field, &table, &schema);
					master_key = qf::core::Utils::composeFieldName(table, schema); // NOLINT(readability-suspicious-call-argument)
					for(const auto &included_table_id : ret) {
						if(qf::core::Utils::fieldNameCmp(included_table_id, master_key)) {
							ret << table_ids.takeFirst();
							dependency_satisfied = true;
							break;
						}
					}
				}
				if(dependency_satisfied)
					break;
			}
			if(dependency_satisfied)
				break;
		}
		if(table_ix < table_ids.count()) {
			ret << table_ids.takeAt(table_ix);
		}
		else {
			qfError() << "Cannot statisfy table dependencies";
			ret.clear();
			break;
		}
	}

	return ret;
}

}
