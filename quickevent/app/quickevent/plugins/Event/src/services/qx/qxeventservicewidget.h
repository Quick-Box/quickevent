#pragma once

#include <qf/gui/framework/dialogwidget.h>

#include <functional>
#include <memory>

namespace shv::chainpack { class RpcValue; }

namespace Event::services::qx {

namespace Ui {
class QxEventServiceWidget;
}

class QxEventService;

class QxEventServiceWidget : public qf::gui::framework::DialogWidget
{
	Q_OBJECT

	using Super = qf::gui::framework::DialogWidget;
public:
	explicit QxEventServiceWidget(QWidget *parent = nullptr);
	~QxEventServiceWidget() override;

	bool acceptDialogDone(int result) override;
private:
	enum class MessageType { Ok, Error, Progress };
	void setMessage(const QString &msg = {}, MessageType msg_type = MessageType::Ok);
	QxEventService* service();
	bool saveSettings();
	void startStopService();
	void updateServiceStatus();
	void setConnectionType();
	QString brokerUrl() const;
	struct DbTransfer;
	std::shared_ptr<DbTransfer> createDbTransfer(const char *node_name);
	void setDbTransferRunning(bool is_running);
	void callDbFileMethod(const std::shared_ptr<DbTransfer> &transfer, const char *method, const shv::chainpack::RpcValue &param, const std::function<void (const shv::chainpack::RpcValue &)> &on_success, int timeout_msec = 0);

	void pushEventDb();
	void pushEventDbChunk(const std::shared_ptr<DbTransfer> &upload);

	void pullEventDb();
	void pullEventDbChunk(const std::shared_ptr<DbTransfer> &download);
	void verifyPulledEventDb(const std::shared_ptr<DbTransfer> &download);
	void savePulledEventDb(const std::shared_ptr<DbTransfer> &download);
private:
	Ui::QxEventServiceWidget *ui;
	bool m_dbTransferRunning = false;
};

}
