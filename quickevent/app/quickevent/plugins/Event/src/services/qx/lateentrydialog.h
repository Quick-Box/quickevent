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

enum class LateEntryStatus {Pending, Accepted, Rejected};

class LateEntryDialog : public QDialog
{
	Q_OBJECT
	using Super = QDialog;
public:
	explicit LateEntryDialog(int change_id, const LateEntry &late_entry, const QString &status, QWidget *parent = nullptr);
	~LateEntryDialog() override;

	void done(int result) override;
private:
	std::optional<int> runId() const;
	QxEventService* service();
	void setMessage(const QString &msg, bool error);

	void loadOrigValues(int run_id);
	// void loadClassId();

	void lockChange();
	void unlockChange() const;
	void updateButtonsEnabled();

	void resolveChangesAndClose(bool is_accepted);
private:
	Ui::LateEntryDialog *ui;
	int m_changeId = 0;
	LateEntryId m_lateEntryId = RunId{};
	int m_competitorId = 0;
	int m_lockNumber = 0;
	LateEntryStatus m_status = LateEntryStatus::Rejected;
	OrigRunRecord m_origValues;
};


} // namespace Event::services::qx


#endif // LATEENTRYDIALOG_H
