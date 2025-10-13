#ifndef ORISIMPORTER_H
#define ORISIMPORTER_H

#include <QObject>

#include <functional>

class QUrl;
class QJsonDocument;

namespace qf { namespace core { namespace network { class NetworkAccessManager; }}}

class OrisImporter : public QObject
{
	Q_OBJECT
public:
	explicit OrisImporter(QObject *parent = nullptr);

	static QString orisDomainName();

	void chooseAndImport();
	void importEvent(int event_id, std::function<void()> success_callback = nullptr);
	void syncCurrentEventEntries(std::function<void()> success_callback = nullptr);
	void importRegistrations(std::function<void()> success_callback = nullptr);
	void importClubs(std::function<void()> success_callback = nullptr);
	void importMissingOneTimeClubs();

	static void saveJsonBackup(const QString &fn, const QJsonDocument &jsd);
	void getJsonAndProcess(const QUrl &url, QObject *context, std::function<void (const QJsonDocument &data)> process_call_back);
	void getTextAndProcess(const QUrl &url, QObject *context, std::function<void (const QByteArray &)> process_call_back);
protected:
	void syncEventEntries(int event_id, std::function<void()> success_callback);
	void syncRelaysEntries(int event_id, std::function<void()> success_callback);

	void getAndImportClub(const QString &club, const QString &key);

private:
	qf::core::network::NetworkAccessManager *networkAccessManager();
private:
	qf::core::network::NetworkAccessManager *m_networkAccessManager = nullptr;
};

#endif // ORISIMPORTER_H
