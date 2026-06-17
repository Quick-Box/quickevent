#ifndef LATEENTRYDIALOG_H
#define LATEENTRYDIALOG_H

#include "runchange.h"

#include <QDialog>

class QNetworkReply;

namespace Event::services::qx {

namespace Ui {
class LateEntryDialog;
}

struct LateEntryRecord;
class QxEventService;

class LateEntryDialog : public QDialog
{
	Q_OBJECT

public:
	explicit LateEntryDialog(int change_id, int lock_number, const LateEntry &late_entry, QWidget *parent = nullptr);
	~LateEntryDialog() override;

private:
	QxEventService* service();
	void setMessage(const QString &msg, bool error);

	void loadOrigValues();
	void loadClassId();

	void lockChange();

	void resolveChanges(bool is_accepted);
private:
	Ui::LateEntryDialog *ui;
	int m_changeId = 0;
	int m_runId = 0;
	int m_competitorId = 0;
	int m_classId = 0;
	int m_lockNumber = 0;
	OrigRunRecord m_origValues;
};


} // namespace Event::services::qx


#endif // LATEENTRYDIALOG_H
