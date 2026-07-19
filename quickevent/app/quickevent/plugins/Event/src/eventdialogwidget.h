#ifndef EVENTDIALOGWIDGET_H
#define EVENTDIALOGWIDGET_H

#include "eventconfig.h"

#include <qdatetime.h>
#include <qf/gui/framework/dialogwidget.h>

namespace Ui {
class EventDialogWidget;
}

class EventDialogWidget : public qf::gui::framework::DialogWidget
{
	Q_OBJECT

	using Super = qf::gui::framework::DialogWidget;
public:
    struct Params {
        Event::EventConfig eventConfig;
        QList<QDateTime> stageStarts;
    };
public:
	explicit EventDialogWidget(QWidget *parent = nullptr);
	~EventDialogWidget() Q_DECL_OVERRIDE;

	void setEventId(const QString &event_id);
	QString eventId() const;
	void setEventIdEditable(bool b);

	void loadParams(const EventDialogWidget::Params &params);
	EventDialogWidget::Params saveParams();

	static QString disciplineName(int disc_id);
	static QString sportName(int sport_id);
private:
	void updateStageStartTimeEditors(const EventDialogWidget::Params &params);

	Ui::EventDialogWidget *ui;
};

#endif // EVENTDIALOGWIDGET_H
