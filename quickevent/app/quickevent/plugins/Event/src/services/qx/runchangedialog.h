#ifndef RUNCHANGEDIALOG_H
#define RUNCHANGEDIALOG_H

#include "runchange.h"

#include <QDialog>

class QNetworkReply;

namespace Event::services::qx {

namespace Ui {
class RunChangeDialog;
}

struct RunChange;
class QxEventService;

class RunChangeDialog : public QDialog
{
	Q_OBJECT

public:
	explicit RunChangeDialog(int change_id, int run_id, int lock_number, const RunChange &run_change, QWidget *parent = nullptr);
	~RunChangeDialog() override;

private:
	QxEventService* service();
	void setMessage(const QString &msg, bool error);

	void loadOrigValues();
	void loadClassId();

	void lockChange();

	void resolveChanges(bool is_accepted);
	void applyLocalChanges(bool is_accepted);

	bool checkHttpError(QNetworkReply *reply);
private:
	Ui::RunChangeDialog *ui;
	int m_changeId = 0;
	int m_runId = 0;
	int m_competitorId = 0;
	int m_classId = 0;
	int m_lockNumber = 0;
	OrigRunRecord m_origValues;
};


} // namespace Event::services::qx


#endif // RUNCHANGEDIALOG_H
