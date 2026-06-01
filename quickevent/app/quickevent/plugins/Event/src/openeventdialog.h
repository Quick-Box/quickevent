#pragma once
#include <QDialog>
#include <QDate>
#include <QList>
#include <QString>
#include <QStringList>

class QSortFilterProxyModel;
class QStandardItemModel;

namespace Ui { class OpenEventDialog; }

namespace Event {

class OpenEventDialog : public QDialog
{
	Q_OBJECT
public:
	struct EventInfo {
		QString id;
		QDate date;
		QString name;
		int sportId = 0;
		int disciplineId = 0;
		int dbVersion = 0;
	};

	enum class RowAction { Open, Convert, Delete };

	OpenEventDialog(const QList<EventInfo> &events, int appDbVersion,
	                const QStringList &existing_names, QWidget *parent = nullptr);
	~OpenEventDialog() override;

	QString selectedEventId() const { return m_selectedEventId; }
	QString convertedEventId() const { return m_convertedEventId; }
	RowAction selectedAction() const { return m_selectedAction; }

private:
	void onOpenClicked(const QString &event_id);
	void onConvertClicked(const QString &event_id);
	void onDeleteClicked(const QString &event_id);

	Ui::OpenEventDialog *ui = nullptr;
	QStandardItemModel *m_model = nullptr;
	QSortFilterProxyModel *m_proxy = nullptr;
	QString m_selectedEventId;
	QString m_convertedEventId;
	RowAction m_selectedAction = RowAction::Open;
	int m_appDbVersion = 0;
	QStringList m_existingNames;
};

} // namespace Event
