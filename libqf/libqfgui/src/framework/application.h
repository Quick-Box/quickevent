#ifndef QF_GUI_FRAMEWORK_APPLICATION_H
#define QF_GUI_FRAMEWORK_APPLICATION_H

#include "../guiglobal.h"

#include <qf/core/sql/qxrecchng.h>

#include <QApplication>
#include <QJsonDocument>
#include <QObject>
#include <QQmlError>

class QQmlEngine;

namespace qf {
namespace gui {
namespace framework {

class MainWindow;

class QFGUI_DECL_EXPORT Application : public QApplication
{
	Q_OBJECT
	friend class MainWindow;
private:
	typedef QApplication Super;
public:
	explicit Application(int & argc, char ** argv);
	~Application() override = default;

	Q_SIGNAL void qxRecChng(const qf::core::sql::QxRecChng &recchng, QObject *source);
	qint64 createDbRecord(const QString &table, const QVariantMap &record, QObject *source);
	std::optional<QVariantMap> readDbRecord(const QString &table, qint64 id, const std::optional<QStringList> &fields = std::nullopt) const;
	bool updateDbRecord(const QString &table, qint64 id, const QVariantMap &record, QObject *source);
	bool deleteDbRecord(const QString &table, qint64 id, QObject *source);
	void emitDbRecInserted(const QString &table, qint64 id, const QVariantMap &record, QObject *source);
	void emitDbRecUpdated(const QString &table, qint64 id, const QVariantMap &record, QObject *source);
	void emitDbRecDeleted(const QString &table, qint64 id, QObject *source);
	void emitQxRecChng(const qf::core::sql::QxRecChng &recchng, QObject *source);

	Q_INVOKABLE QString versionString() const;
public:
	static Application* instance(bool must_exist = true);
	MainWindow* frameWork();

	void loadStyleSheet(const QString &file = QString());

	QString applicationDirPath();
	QString applicationName();
	QStringList arguments();

	static QUuid uuid();
	static QString uuidString();
protected:
	MainWindow* m_frameWork = nullptr;
};

}}}

#endif // QF_GUI_FRAMEWORK_APPLICATION_H
