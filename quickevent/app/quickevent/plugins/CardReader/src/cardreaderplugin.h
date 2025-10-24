#ifndef CARDREADER_CARDREADERPLUGIN_H
#define CARDREADER_CARDREADERPLUGIN_H

#include <qf/core/utils.h>
#include <qf/gui/framework/plugin.h>

#include <QQmlListProperty>

namespace quickevent { namespace core { namespace si { class PunchRecord; class ReadCard; class CheckedCard; }}}

namespace siut { class SIMessageData; }

namespace CardReader {

class CardChecker;
class ReadCard;
class PunchRecord;
class CheckedCard;

class CardReaderPlugin : public qf::gui::framework::Plugin
{
	Q_OBJECT
	//Q_PROPERTY(QQmlListProperty<CardReader::CardChecker> cardCheckers READ cardCheckersListProperty)
	//Q_PROPERTY(int currentCardCheckerIndex READ currentCardCheckerIndex WRITE setCurrentCardCheckerIndex NOTIFY currentCardCheckerIndexChanged)
private:
	typedef qf::gui::framework::Plugin Super;
public:
	CardReaderPlugin(QObject *parent = nullptr);

	const QList<CardReader::CardChecker*>& cardCheckers() {return m_cardCheckers;}
	CardReader::CardChecker* currentCardChecker();

	int currentStageId();
	int cardIdToSiId(int card_id);
	int findRunId(int si_id, int si_finish_time, QString *err_msg = nullptr);
	bool isCardLent(int si_id, int si_finish_time, int run_id);
	quickevent::core::si::ReadCard readCard(int card_id);
	quickevent::core::si::CheckedCard checkCard(int card_id, int run_id = 0);
	quickevent::core::si::CheckedCard checkCard(const quickevent::core::si::ReadCard &read_card);
	int saveCardToSql(const quickevent::core::si::ReadCard &read_card);
	int savePunchRecordToSql(const quickevent::core::si::PunchRecord &punch_record);

	Q_INVOKABLE bool reloadTimesFromCard(int card_id, int run_id = 0, bool in_transaction = true);
	void assignCardToRun(int card_id, int run_id);
	bool processCardToRunAssignment(int card_id, int run_id);

	static int resolveAltCode(int maybe_alt_code, int stage_id);

	void emitSiTaskFinished(int task_type, QVariant result) { emit siTaskFinished(task_type, result); }
	Q_SIGNAL void siTaskFinished(int task_type, QVariant result);
private:
	void onInstalled();
	QQmlListProperty<CardChecker> cardCheckersListProperty();

	void updateCardToRunAssignmentInPunches(int stage_id, int card_id, int run_id);
	bool saveCardAssignedRunnerIdSql(int card_id, int run_id);
	void updateCheckedCardValuesSql(const quickevent::core::si::CheckedCard &checked_card) noexcept(false);
	void setStartTime(int relay_id, int leg, int start_time);
private:
	QList<CardChecker*> m_cardCheckers;
};

}

#endif
