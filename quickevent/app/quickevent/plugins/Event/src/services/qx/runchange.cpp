#include "runchange.h"

#include <QJsonDocument>
#include <QVariantMap>

namespace Event::services::qx {

// RunChange RunChange::fromJsonString(const QString &json)
// {
// 	RunChange ret;
// 	return ret;
// }

LateEntry LateEntry::fromVariantMap(const QVariantMap &map)
{
	LateEntry ret;

	auto id_map = map.value("id").toMap();
	if (auto id = id_map.value("RunId").toInt(); id > 0) {
		ret.id = RunId{id};
	}
	if (auto id = id_map.value("ClassId").toInt(); id > 0) {
		ret.id = ClassId{id};
	}
	if (auto v = map.value("firstname"); v.isValid()) { ret.first_name = v.toString(); }
	if (auto v = map.value("lastname"); v.isValid()) { ret.last_name = v.toString(); }
	if (auto v = map.value("registration"); v.isValid()) { ret.registration = v.toString(); }
	if (auto v = map.value("starttimems"); v.isValid()) { ret.start_time_ms = v.toInt(); }
	if (auto v = map.value("siid"); v.isValid()) { ret.si_id = v.toInt(); }
	// if (auto v = map.value("si_id_rent"); v.isValid()) { ret.si_id_rent = v.toBool(); }
	if (auto v = map.value("note"); v.isValid()) { ret.note = v.toString(); }

	return ret;
}

QVariantMap OrigRunRecord::toVariantMap() const
{
	QVariantMap ret;
	ret["firstname"] = first_name;
	ret["lastname"] = last_name;
	ret["registration"] = registration;
	ret["siid"] = si_id;
	ret["starttimems"] = start_time_ms;
	return ret;
}

QxChangeData QxChangeData::fromJson(const QString &json)
{
	QxChangeData ret;
	auto map = QJsonDocument::fromJson(json.toUtf8()).toVariant().toMap();
	if (auto v = map.value(DATA_TYPE_LATE_ENTRY); v.isValid()) {
		ret.lateEntry = LateEntry::fromVariantMap(v.toMap());
	}
	return ret;
}

} // namespace Event::services::qx
