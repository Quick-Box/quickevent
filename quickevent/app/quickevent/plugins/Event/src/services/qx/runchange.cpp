#include "runchange.h"

#include <QJsonDocument>
#include <QVariantMap>

namespace Event::services::qx {

// RunChange RunChange::fromJsonString(const QString &json)
// {
// 	RunChange ret;
// 	return ret;
// }

LateEntryRecord LateEntryRecord::fromVariantMap(const QVariantMap &map)
{
	LateEntryRecord ret;

	if (auto v = map.value("class_name"); v.isValid()) { ret.class_name = v.toString(); }
	if (auto v = map.value("firstname"); v.isValid()) { ret.first_name = v.toString(); }
	if (auto v = map.value("lastname"); v.isValid()) { ret.last_name = v.toString(); }
	if (auto v = map.value("registration"); v.isValid()) { ret.registration = v.toString(); }
	if (auto v = map.value("siid"); v.isValid()) { ret.si_id = v.toInt(); }
	// if (auto v = map.value("si_id_rent"); v.isValid()) { ret.si_id_rent = v.toBool(); }
	ret.note = map.value("note").toString();

	return ret;
}

LateEntry LateEntry::fromVariantMap(const QVariantMap &map)
{
	LateEntry ret;

	ret.record = LateEntryRecord::fromVariantMap(map.value("record").toMap());
	ret.run_id = map.value("run_id").toInt();

	return ret;
}

QVariantMap OrigRunRecord::toVariantMap() const
{
	QVariantMap ret;
	ret["first_name"] = first_name;
	ret["last_name"] = last_name;
	ret["registration"] = registration;
	ret["si_id"] = si_id;
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


