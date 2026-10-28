#ifndef CARDREADER_CARDREADERPLUGIN_H
#define CARDREADER_CARDREADERPLUGIN_H

#include <qf/core/utils.h>
#include <qf/gui/framework/plugin.h>


namespace quickevent { namespace core { namespace si { class PunchRecord; class ReadCard; class CheckedCard; }}}

namespace siut { class SIMessageData; }

namespace CardReader {

class CardChecker;

class CardReaderPlugin : public qf::gui::framework::Plugin
{
	Q_OBJECT
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
	std::pair<quickevent::core::si::ReadCard, quickevent::core::si::CheckedCard> checkCard(int card_id, int run_id = 0);
	quickevent::core::si::CheckedCard checkCard(const quickevent::core::si::ReadCard &read_card);
	int saveCardToSql(const quickevent::core::si::ReadCard &read_card);
	int savePunchRecordToSql(const quickevent::core::si::PunchRecord &punch_record);

	bool reloadTimesFromCard(int card_id, int run_id);
	void assignCardToRun(int card_id, int run_id);
	bool processCardToRunAssignment(int card_id, int run_id);

	static int resolveAltCode(int maybe_alt_code, int stage_id);

	void emitSiTaskFinished(int task_type, QVariant result, QString reader_message = QString()) { emit siTaskFinished(task_type, result, reader_message); }
	Q_SIGNAL void siTaskFinished(int task_type, QVariant result, QString reader_message = QString());
private:
	void onInstalled();

	void updateCardToRunAssignmentInPunches(int stage_id, int card_id, int run_id);
	bool saveCardAssignedRunnerIdSql(int card_id, int run_id);
	void updateCheckedCardValuesSql(int card_id,
									const quickevent::core::si::ReadCard &read_card,
									const quickevent::core::si::CheckedCard &checked_card);
	void setStartTime(int relay_id, int leg, int start_time);
private:
	QList<CardChecker*> m_cardCheckers;
};

}

#endif
