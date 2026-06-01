#include "application.h"

#include <qf/core/log.h>
#include <qf/core/assert.h>
#include <qf/core/utils/fileutils.h>
#include <qf/core/sql/qxsql.h>

#include <QQmlEngine>
#include <QQmlContext>
#include <QFile>
#include <QJsonParseError>
#include <QUuid>

namespace qfu = qf::core::utils;
using namespace qf::gui::framework;

Application::Application(int &argc, char **argv) :
	Super(argc, argv)
{
}

qint64 Application::createDbRecord(const QString &table, const QVariantMap &record, QObject *source)
{
	using namespace qf::core::sql;
	QxSql sql;
	connect(&sql, &QxSql::dbRecChng, this, [this, source](const qf::core::sql::QxRecChng &recchng) { emit qxRecChng(recchng, source); });
	return sql.createRecord(table, record, uuidString());
}

std::optional<QVariantMap> Application::readDbRecord(const QString &table, qint64 id, const std::optional<QStringList> &fields) const
{
	using namespace qf::core::sql;
	QxSql sql;
	return sql.readRecord(table, id, fields);
}

bool Application::updateDbRecord(const QString &table, qint64 id, const QVariantMap &record, QObject *source)
{
	using namespace qf::core::sql;
	QxSql sql;
	connect(&sql, &QxSql::dbRecChng, this, [this, source](const qf::core::sql::QxRecChng &recchng) { emit qxRecChng(recchng, source); });
	return sql.updateRecord(table, id, record, uuidString());
}

bool Application::deleteDbRecord(const QString &table, qint64 id, QObject *source)
{
	using namespace qf::core::sql;
	QxSql sql;
	connect(&sql, &QxSql::dbRecChng, this, [this, source](const qf::core::sql::QxRecChng &recchng) { emit qxRecChng(recchng, source); });
	return sql.deleteRecord(table, id, uuidString());
}

QString Application::versionString() const
{
	return QCoreApplication::applicationVersion();
}

void Application::emitDbRecInserted(const QString &table, qint64 id, const QVariantMap &record, QObject *source)
{
	emit qxRecChng(qf::core::sql::QxRecChng{
		.table = table,
		.id = id,
		.record = record,
		.op = qf::core::sql::RecOp::Insert,
		.issuer = uuidString()
	}, source);
}

void Application::emitDbRecUpdated(const QString &table, qint64 id, const QVariantMap &record, QObject *source)
{
	emit qxRecChng(qf::core::sql::QxRecChng{
		.table = table,
		.id = id,
		.record = record,
		.op = qf::core::sql::RecOp::Update,
		.issuer = uuidString()
	}, source);
}

void Application::emitDbRecDeleted(const QString &table, qint64 id, QObject *source)
{
	emit qxRecChng(qf::core::sql::QxRecChng{
		.table = table,
		.id = id,
		.record = {},
		.op = qf::core::sql::RecOp::Delete,
		.issuer = uuidString()
	}, source);
}

void Application::emitQxRecChng(const core::sql::QxRecChng &recchng, QObject *source)
{
	emit qxRecChng(recchng, source);
}

Application *Application::instance(bool must_exist)
{
	auto *ret = qobject_cast<Application*>(Super::instance());
	if(!ret && must_exist) {
		throw std::runtime_error("qf::gui::framework::Application instance MUST exist.");
	}
	return ret;
}

MainWindow *Application::frameWork()
{
	QF_ASSERT_EX(m_frameWork != nullptr, "FrameWork is not set.");
	return m_frameWork;
}

QString Application::applicationDirPath()
{
	return QCoreApplication::applicationDirPath();
}

QString Application::applicationName()
{
	return QCoreApplication::applicationName();
}

QStringList Application::arguments()
{
	return QCoreApplication::arguments();
}

QUuid Application::uuid()
{
	static auto uuid = QUuid::createUuid();
	return uuid;
}

QString Application::uuidString()
{
	static auto s = uuid().toString(QUuid::WithoutBraces);
	return s;
}

void Application::loadStyleSheet(const QString &file)
{
	QString css_file_name = file;
	if(css_file_name.isEmpty()) {
		QString app_name = Application::applicationName().toLower();
		css_file_name = qfu::FileUtils::joinPath(Application::applicationDirPath(), "/" + app_name + "-data/style/default.css");
		if(!QFile::exists(css_file_name))
			css_file_name = ":/" + app_name + "/style/default.css";
	}
	qfInfo() << "Opening style sheet:" << css_file_name;
	QFile f(css_file_name);
	if(f.open(QFile::ReadOnly)) {
		QByteArray ba = f.readAll();
		QString ss = QString::fromUtf8(ba);
		setStyleSheet(ss);
	}
	else {
		qfWarning() << "Cannot open style sheet:" << css_file_name;
	}
}
