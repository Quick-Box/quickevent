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
	void testConnection();
	void setConnectionType();
	QString brokerUrl() const;
	void pushEventDb();
	struct PushDbUpload;
	void setPushDbRunning(bool is_running);
	void pushEventDbChunk(const std::shared_ptr<PushDbUpload> &upload);
	void callPushDbMethod(const std::shared_ptr<PushDbUpload> &upload, const char *method, const shv::chainpack::RpcValue &param, const std::function<void (const shv::chainpack::RpcValue &)> &on_success, int timeout_msec = 0);
private:
	Ui::QxEventServiceWidget *ui;
};

}

