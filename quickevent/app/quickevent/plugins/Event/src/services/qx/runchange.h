#pragma once

#include <QString>
#include <QVariant>
#include <QDateTime>

#include <optional>
#include <variant>

namespace Event::services::qx {

constexpr auto DATA_TYPE_LATE_ENTRY = "LateEntry";

struct RunId { int id = 0; };
struct ClassId { int id = 0; };

using LateEntryForeignId = std::variant<RunId, ClassId>;

struct LateEntry
{
	LateEntryForeignId id = RunId{};
	std::optional<QString> first_name;
	std::optional<QString> last_name;
	std::optional<QString> registration;
	std::optional<int> si_id;
	std::optional<int> start_time_ms;
	// std::optional<bool> si_id_rent;
	std::optional<QString> note;
	std::optional<bool> paid;

	// static LateEntry fromJsonString(const QString &json);
	static LateEntry fromVariantMap(const QVariantMap &map);
};

struct QxChangeData
{
	std::optional<LateEntry> lateEntry;

	static QxChangeData fromJson(const QString &json);
};

struct OrigRunRecord
{
	QString first_name;
	QString last_name;
	QString registration;
	int start_time_ms = 0;
	int si_id = 0;
	// bool si_id_rent;

	QVariantMap toVariantMap() const;
};

enum class DataType {
	OcChange,
	RunUpdateRequest,
	RunUpdated,
	RadioPunch,
	CardReadout,
};
enum class QxChangeStatus {
	Pending,
	Accepted,
	Rejected,
};
QxChangeStatus qxChangeStatusFromString(const QString &status);

struct EventChange
{
	int64_t id;
	QString source;
	DataType data_type;
	int64_t data_id;
	QVariant data;
	QString user_id;
	QxChangeStatus status;
	QString status_message;
	QDateTime created;
	int64_t lock_number;

	QVariantMap toVariantMap() const;
};

// struct ChangeRecord
// {
// 	int64_t id;
// 	QString source;
// 	QString data_type,
// 	int64_t data_id;
// 	data: ChangeData,
// 	user_id: Option<String>,
// 	status: Option<ChangeStatus>,
// 	created: QxDateTime,
// 	lock_number: Option<i64>,
// };

} // namespace Event::services::qx
