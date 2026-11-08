#pragma once

#include <QString>
#include <QVariant>
#include <QDateTime>

#include <optional>
#include <variant>

namespace Event::services::qx {

constexpr auto DATA_TYPE_LATE_ENTRY = "LateEntry";

struct Update { int runId = 0; };
struct NewEntry { int classId = 0; };
struct DeleteEntry { int runId = 0; };

using LateEntryKind = std::variant<Update, NewEntry, DeleteEntry>;

struct LateEntry
{
	LateEntryKind kind = Update{};
	std::optional<QString> first_name;
	std::optional<QString> last_name;
	std::optional<QString> registration;
	std::optional<int> si_id;
	std::optional<int> start_time_ms;
	// std::optional<bool> si_id_rent;
	std::optional<QString> note;

	// static LateEntry fromJsonString(const QString &json);
	static std::optional<LateEntry> fromVariantMap(const QVariantMap &map);
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

} // namespace Event::services::qx
