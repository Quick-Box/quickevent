#include "runchange.h"

#include <qf/core/log.h>

#include <QJsonDocument>
#include <QVariantMap>

namespace Event::services::qx {

// RunChange RunChange::fromJsonString(const QString &json)
// {
// 	RunChange ret;
// 	return ret;
// }

std::optional<LateEntry> LateEntry::fromVariantMap(const QVariantMap &map)
{
	LateEntry ret;

	auto kind_map = map.value("kind").toMap();
	if (auto id = kind_map.value("Update").toMap().value("runId").toInt(); id > 0) {
		ret.kind = Update{.runId = id};
	}
	else if (auto id = kind_map.value("NewEntry").toMap().value("classId").toInt(); id > 0) {
		ret.kind = NewEntry{.classId = id};
	}
	else if (auto id = kind_map.value("DeleteEntry").toMap().value("runId").toInt(); id > 0) {
		ret.kind = DeleteEntry{.runId = id};
	} else {
		qfWarning() << "Invalid late entry kind";
		return {};
	}
	if (auto v = map.value("firstname"); v.isValid()) { ret.first_name = v.toString(); }
	if (auto v = map.value("lastname"); v.isValid()) { ret.last_name = v.toString(); }
	if (auto v = map.value("registration"); v.isValid()) { ret.registration = v.toString(); }
	if (auto v = map.value("starttimems"); v.isValid()) { ret.start_time_ms = v.toInt(); }
	if (auto v = map.value("siid"); v.isValid()) { ret.si_id = v.toInt(); }
	// if (auto v = map.value("si_id_rent"); v.isValid()) { ret.si_id_rent = v.toBool(); }
	if (auto v = map.value("note"); v.isValid()) { ret.note = v.toString(); }
	if (auto v = map.value("paid"); v.isValid()) { ret.paid = v.toBool(); }

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

QxChangeStatus qxChangeStatusFromString(const QString &status)
{
	if (status.compare(QStringLiteral("Pending"), Qt::CaseInsensitive) == 0) {
		return QxChangeStatus::Pending;
	}
	if (status.compare(QStringLiteral("Accepted"), Qt::CaseInsensitive) == 0) {
		return QxChangeStatus::Accepted;
	}
	if (status.compare(QStringLiteral("Rejected"), Qt::CaseInsensitive) == 0) {
		return QxChangeStatus::Rejected;
	}
	qfWarning() << "Unknown late entry status:" << status;
	return QxChangeStatus::Rejected;
}

} // namespace Event::services::qx
