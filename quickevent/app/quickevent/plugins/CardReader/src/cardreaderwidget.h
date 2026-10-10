#ifndef CARDREADERWIDGET_H
#define CARDREADERWIDGET_H

#include "cardreadersettings.h"

#include <siut/sitask.h>

#include <qf/core/exception.h>
#include <qf/core/log.h>

#include <QFrame>

#include "partwidget.h"

namespace Ui {
	class CardReaderWidget;
}

namespace quickevent::gui::og { class SqlTableModel; }
namespace qf::core::sql { struct QxRecChng; }

namespace qf::gui {
class Action;
namespace framework { class PartWidget; class Plugin; }
}


namespace siut { class DeviceDriver; class CommPort; class BtSiDeviceDriver; struct SICard; struct SIPunch; }

namespace quickevent { namespace gui { namespace audio { class Player; }}}
namespace quickevent { namespace core { namespace si { class ReadCard; class CheckedCard; }}}

namespace CardReader {
class CardReaderPlugin;
class CardChecker;
}

namespace Event { class EventPlugin; }

class QTextStream;
class QFile;
class QComboBox;
class QCheckBox;
class QLabel;
class QPushButton;

class SIMessageTransmitPunch;
class SIMessageData;
class SIMessageCardReadOut;

class CardReaderPartWidget;

class CardReaderWidget : public QFrame
{
	Q_OBJECT
private:
	typedef QFrame Super;
public:
	explicit CardReaderWidget(QWidget *parent = nullptr);
	~CardReaderWidget() Q_DECL_OVERRIDE;

	Q_SIGNAL void logRequest(NecroLog::Level level, const QString &msg);
	void emitLogRequest(NecroLog::Level level, const QString &msg) {emit logRequest(level, msg);}

	void settleDownInPartWidget(::PartWidget *part_widget);

	Q_SLOT void reset();
	Q_SLOT void reload();

private:
	void onQxRecChng(const qf::core::sql::QxRecChng &recchng, QObject *source);
	void onDbEventNotify(const QString &domain, int connection_id, const QVariant &data);
	void appendLog(NecroLog::Level level, const QString &msg);
	void processDriverInfo(NecroLog::Level level, const QString &msg);
	void logDriverRawData(const QByteArray &data);
	void onOpenUsbTriggered(bool checked);
	void onOpenBtTriggered(bool checked);
	void updateButtonsEnabled();

	void updateConnectionInfoLabel();
	void setConnectionInfoLabel(const QString &info, NecroLog::Level level);

	void onSiTaskFinished(int task_type, QVariant result, QString reader_message = QString());

	void readStationBackupMemory();

	void importCards_lapsOnlyCsv();
	void importCards_SIReaderBackupMemoryCsv();
private:
	void createActions();

	siut::DeviceDriver *siDriver();
	siut::CommPort *commPort();
	void onComOpenChanged(bool comm_is_open);
	siut::BtSiDeviceDriver *btDriver();
	void onBtConnectionChanged(bool connected);

	void processSICard(const siut::SICard &card, const QString &reader_message = QString());
	void processSIPunch(const siut::SIPunch &rec);

	bool processReadCardInTransaction(const quickevent::core::si::ReadCard &read_card);
	void processReadCard(const quickevent::core::si::ReadCard &read_card);

	void updateTableView(int card_id);
	void updateStatistics();

	void onCustomContextMenuRequest(const QPoint &pos);
	void showSelectedReceipt();
	void showSelectedCard();
	void assignRunnerToSelectedCard();

	quickevent::gui::audio::Player* audioPlayer();
	void operatorAudioWakeUp();
	void operatorAudioNotify();
	CardReaderSettings::ReaderMode currentReaderMode() const;

	void onTestButtonClicked();
	void onTestButtonClicked2();
private:
	Ui::CardReaderWidget *ui;
	qf::gui::Action *m_actAssignCard = nullptr;
	quickevent::gui::og::SqlTableModel *m_cardsModel = nullptr;
	quickevent::gui::audio::Player *m_audioPlayer = nullptr;
	siut::DeviceDriver *f_siDriver = nullptr;
	siut::CommPort *m_commPort = nullptr;
	siut::BtSiDeviceDriver *m_btDriver = nullptr;
};

#endif // CARDREADERWIDGET_H
