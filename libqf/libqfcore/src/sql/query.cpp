#include "query.h"
#include "connection.h"
#include "querybuilder.h"

#include "../core/log.h"

#include <QSqlRecord>
#include <QSqlField>
#include <QVariant>

namespace qf::core::sql {

Query::Query(const QSqlDatabase &db)
	: Super(db)
{
}

Query::Query(const QString &connection_name)
	: Super(Connection::forName(connection_name))
{
}

bool Query::prepare(const QString &query, bool throw_exc)
{
	qfLogFuncFrame() << query;
	bool ret = Super::prepare(query);
	if(!ret) {
		if(throw_exc)
			QF_EXCEPTION(lastError().text());
		qfWarning() << lastError().text();
	}
	return ret;
}

Query Query::fromExec(const QString &query, bool throw_exc)
{
	Query q;
	q.exec(query, throw_exc);
	return q;
}

bool Query::exec(const QString &query, bool throw_exc)
{
	qfLogFuncFrame() << query;
	//qfWarning() << query;
	bool ret = Super::exec(query);
	if(!ret) {
		if(throw_exc)
			QF_EXCEPTION(query + '\n' + lastError().text());
		qfError() << query << '\n' << lastError().text();
	}
	if(isSelect())
		m_demangledRecord = QSqlRecord();
	return ret;
}

bool Query::exec(const QueryBuilder &query_builder, bool throw_exc)
{
	return exec(query_builder.toString(), throw_exc);
}

bool Query::exec(bool throw_exc)
{
	//qfLogFuncFrame();
	bool ret = Super::exec();
	if(!ret) {
		if(throw_exc)
			QF_EXCEPTION(lastError().text());
		//qfWarning() << lastError().text();
	}
	if(isSelect())
		m_demangledRecord = QSqlRecord();
	return ret;
}

QSqlRecord Query::record() const
{
	if(m_demangledRecord.isEmpty()) {
		m_demangledRecord = Super::record();
		for(int i=0; i<m_demangledRecord.count(); i++) {
			QSqlField fld = m_demangledRecord.field(i);
			QString n = QueryBuilder::unmangleLongFieldName(fld.name());
			if(n != fld.name()) {
				fld.setName(n);
				m_demangledRecord.replace(i, fld);
			}
		}
	}
	return m_demangledRecord;
}

int Query::fieldIndex(const QString &field_name) const
{
	QSqlRecord rc = record();
	int ix;
	for(ix=0; ix<rc.count(); ix++) {
		QString fld_mn = rc.fieldName(ix);
		if(fld_mn.endsWith(field_name, Qt::CaseInsensitive)) {
			if(fld_mn.length() == field_name.length())
				break;
			//qfWarning() << fld_mn << field_name << fld_mn.mid(0, fld_mn.length() - field_name.length());
			if(fld_mn[fld_mn.length() - field_name.length() - 1] == '.')
				break;
		}
	}
	if(ix >= rc.count())
		ix = -1;
	return ix;
}

QVariant Query::value(const QString &field_name) const
{
	QVariant ret;
	int ix = fieldIndex(field_name);
	if(ix < 0) {
		QSqlRecord rc = record();
		QStringList sl;
		for(ix=0; ix<rc.count(); ix++)
			sl << rc.fieldName(ix);
		qfError() << "Field" << field_name << "not found in the record!";
		qfInfo() << "Available fields:" << sl.join(", ");
	}
	else {
		ret = value(ix);
	}
	return ret;
}

QVariant Query::value(int ix) const
{
	return Super::value(ix);
}

QVariantMap Query::values() const
{
	QVariantMap ret;
	auto rec = record();
	for (int i=0; i<rec.count(); ++i) {
		QString fld_name;
		qf::core::Utils::parseFieldName(rec.fieldName(i), &fld_name);
		// SQL is case insensitive
		ret[fld_name.toLower()] = value(i);
	}
	return ret;
}

QString Query::lastErrorText() const
{
	QString ret;
	QSqlError err = lastError();
	if(err.isValid()) {
		ret = err.text();
	}
	return ret;
}

bool Query::execCommands(const QStringList &commands, const QMap<QString, QString> &replacements)
{
	for(QString qs : commands) {
		QMapIterator<QString, QString> it(replacements);
		if(it.hasNext()) {
			it.next();
			QString key = "{{" + it.key() + "}}";
			qs.replace(key, it.value());
		}
		if(!exec(qs, !qf::core::Exception::Throw)) {
			return false;
		}
	}
	return true;
}

void Query::execCommandsThrow(const QStringList &commands, const QMap<QString, QString> &replacements)
{
	for(QString qs : commands) {
		QMapIterator<QString, QString> it(replacements);
		if(it.hasNext()) {
			it.next();
			QString key = "{{" + it.key() + "}}";
			qs.replace(key, it.value());
		}
		exec(qs, qf::core::Exception::Throw);
	}
}

QVariantMap recordToMap(const QSqlRecord &rec)
{
	QVariantMap record;
	for (int i = 0; i < rec.count(); ++i) {
		QSqlField fld = rec.field(i);
		auto fld_name = fld.name();
		fld_name.replace("__", ".");
		record[fld_name] = fld.value();
	}
	return record;
}

QVariantMap recordDiff(const QVariantMap &from, const QVariantMap &to)
{
	QVariantMap ret;
	for (const auto &[k2, v2] : to.asKeyValueRange()) {
		auto v1 = from.value(k2);
		if (v1 == v2)
			continue;
		ret[k2] = v2;
	}
	return ret;
}

}
