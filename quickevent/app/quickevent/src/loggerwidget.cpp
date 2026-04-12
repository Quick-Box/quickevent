#include "loggerwidget.h"
#include "application.h"

#include <qf/gui/log.h>

#include <qf/gui/model/logtablemodel.h>

LoggerWidget::LoggerWidget(QWidget *parent)
	: Super(parent)
{
	addCategoryActions(tr("<empty>"), QString(), NecroLog::Level::Info);

	m_logModel = new qf::gui::model::LogTableModel(this);
	m_logModel->setMaximumRowCount(50000);
	connect(Application::instance(), &Application::newLogEntry, m_logModel, &qf::gui::model::LogTableModel::addLogEntry, Qt::QueuedConnection);
	setLogTableModel(m_logModel);

	// m_logTableModel->addLog(NecroLogLevel::Error, "CATEGORY", "some.file", 123, "Error message", QDateTime::currentDateTime(), "func_name");
	// m_logTableModel->addLog(NecroLogLevel::Warning, "CATEGORY", "some.file", 123, "Warning message", QDateTime::currentDateTime(), "func_name");
	// m_logTableModel->addLog(NecroLogLevel::Info, "CATEGORY", "some.file", 123, "Info message", QDateTime::currentDateTime(), "func_name");
	// m_logTableModel->addLog(NecroLogLevel::Message, "CATEGORY", "some.file", 123, "Message message", QDateTime::currentDateTime(), "func_name");
	// m_logTableModel->addLog(NecroLogLevel::Debug, "CATEGORY", "some.file", 123, "Debug message", QDateTime::currentDateTime(), "func_name");
}

LoggerWidget::~LoggerWidget() = default;

void LoggerWidget::onDockWidgetVisibleChanged(bool visible)
{
	//qfWarning() << "onDockWidgetVisibleChanged" << visible;
	if(visible) {
		checkScrollToLastEntry();
	}
}

