#ifndef LATEENTRYDIALOG_H
#define LATEENTRYDIALOG_H

#include "runchange.h"

#include <QDialog>

class QNetworkReply;

namespace Event::services::qx {

namespace Ui {
class LateEntryDialog;
}

struct LateEntry;
class QxEventService;

class LateEntryDialog : public QDialog
{
	Q_OBJECT
	using Super = QDialog;
public:
	explicit LateEntryDialog(int change_id, int stage_id, const LateEntry &late_entry, const QString &status, const QString &status_message, QWidget *parent = nullptr);
	~LateEntryDialog() override;

	void done(int result) override;
private:
	std::optional<int> runId() const;
	std::optional<int> classId() const;
	QxEventService* service();
	void setMessage(const QString &msg, bool error);

	void loadOrigValues(int run_id);
	void loadClassName(int class_id);

	void lockChange();
	void unlockChange() const;
	void updateButtonsEnabled();

	void resolveChangesAndClose(bool is_accepted);
	void updateQxChangeMessage();

	void checkDuplicitRegistration();
	void checkDuplicitName();
	void checkStartTimeIsValid();
	void changeEventEntryToStageEntry(int competitor_id);
private:
	Ui::LateEntryDialog *ui;
	int m_changeId = 0;
	int m_stageId = 0;
	LateEntryForeignId m_lateEntryId = RunId{};
	int m_competitorId = 0;
	int m_lockNumber = 0;
	QxChangeStatus m_status = QxChangeStatus::Rejected;
	OrigRunRecord m_origValues;
	bool m_setIsRunning = false;
};


} // namespace Event::services::qx


#endif // LATEENTRYDIALOG_H
