#include "logentrymap.h"

#include <QDateTime>

#ifdef Q_CC_MSVC
template class QFCORE_DECL_EXPORT QMap<QString, QVariant>;
#endif

namespace qf::core {

//=========================================================
// LogEntryMap
//=========================================================
static const auto KEY_LEVEL = QStringLiteral("level");
static const auto KEY_CATEGORY = QStringLiteral("category");
static const auto KEY_MESSAGE = QStringLiteral("message");
static const auto KEY_FILE = QStringLiteral("file");
static const auto KEY_LINE = QStringLiteral("line");
static const auto KEY_FUNCTION = QStringLiteral("function");
static const auto KEY_TIME_STAMP = QStringLiteral("timestamp");

LogEntryMap::~LogEntryMap() = default;

LogEntryMap::LogEntryMap(NecroLog::Level level, const QString &category, const QString &message, const QString &file, int line, const QString &function)
{
	this->operator[](KEY_LEVEL) = (int)level;
	this->operator[](KEY_CATEGORY) = category;
	this->operator[](KEY_MESSAGE) = message;
	this->operator[](KEY_FILE) = file;
	this->operator[](KEY_LINE) = line;
	this->operator[](KEY_FUNCTION) = function;
	this->operator[](KEY_TIME_STAMP) = QDateTime::currentDateTime();
}

NecroLog::Level LogEntryMap::level() const
{
	return (NecroLog::Level)value(KEY_LEVEL).toInt();
}

LogEntryMap &LogEntryMap::setLevel(NecroLog::Level l)
{
	(*this)[KEY_LEVEL] = (int)l;
	return *this;
}

QString LogEntryMap::levelStr() const
{
	return NecroLog::levelToString(level());
}

QString LogEntryMap::message() const
{
	return value(KEY_MESSAGE).toString();
}

LogEntryMap &LogEntryMap::setMessage(const QString &m)
{
	(*this)[KEY_MESSAGE] = m;
	return *this;
}

QString LogEntryMap::category() const
{
	return value(KEY_CATEGORY).toString();
}

LogEntryMap &LogEntryMap::setCategory(const QString &c)
{
	(*this)[KEY_CATEGORY] = c;
	return *this;
}

QString LogEntryMap::file() const
{
	return value(KEY_FILE).toString();
}

LogEntryMap &LogEntryMap::setFile(const QString &f)
{
	(*this)[KEY_FILE] = f;
	return *this;
}

int LogEntryMap::line() const
{
	return value(KEY_LINE).toInt();
}

LogEntryMap &LogEntryMap::setLine(int l)
{
	(*this)[KEY_LINE] = l;
	return *this;
}

QString LogEntryMap::function() const
{
	return value(KEY_FUNCTION).toString();
}

LogEntryMap &LogEntryMap::setFunction(const QString &f)
{
	(*this)[KEY_FUNCTION] = f;
	return *this;
}

QDateTime LogEntryMap::timeStamp() const
{
	return value(KEY_TIME_STAMP).toDateTime();
}

LogEntryMap &LogEntryMap::setTimeStamp(const QDateTime &ts)
{
	(*this)[KEY_TIME_STAMP] = ts;
	return *this;
}

QString LogEntryMap::toString() const
{
	QString ret = "{";
	ret += R"("level":)" + QString::number((int)level()) + ", ";
	ret += R"("category":")" + category() + "\", ";
	ret += R"("message":")" + message() + "\", ";
	ret += R"("file":")" + file() + "\", ";
	ret += R"("line":)" + QString::number(line()) + ", ";
	ret += R"("time":")" + timeStamp().toString(Qt::ISODate) + "\", ";
	ret += R"("function":")" + function() + "\"}";
	return ret;
}

}
