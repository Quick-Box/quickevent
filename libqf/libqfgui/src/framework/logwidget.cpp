#include "logwidget.h"
#include "ui_logwidget.h"
#include "../tableview.h"

#include "../style.h"

#include <qf/core/log.h>
#include <qf/core/assert.h>
#include <qf/core/logentrymap.h>
#include <qf/gui/model/logtablemodel.h>

#include <QSortFilterProxyModel>
#include <QActionGroup>
#include <QDateTime>
#include <QClipboard>
#include <QKeyEvent>
#include <QMenu>
#include <QInputDialog>
#include <QScrollBar>

namespace qfm = qf::gui::model;

namespace qf::gui::framework {

LogWidgetTableView::LogWidgetTableView(QWidget *parent)
	: Super(parent)
{
	{
		m_copySelectionToClipboardAction = new QAction(tr("Copy"));
		m_copySelectionToClipboardAction->setObjectName("LogWidgetTableView copy action");
		m_copySelectionToClipboardAction->setIcon(qf::gui::Style::icon("copy"));
		m_copySelectionToClipboardAction->setShortcut(QKeySequence(tr("Ctrl+C", "Copy selection")));
		m_copySelectionToClipboardAction->setShortcutContext(Qt::WidgetShortcut);
		connect(m_copySelectionToClipboardAction, &QAction::triggered, this, &LogWidgetTableView::copy);
		addAction(m_copySelectionToClipboardAction);
	}
	setContextMenuPolicy(Qt::ActionsContextMenu);
}

void LogWidgetTableView::copy()
{
	qfLogFuncFrame();
	TableView::copySelectionToClipboard(this);
}

class LogFilterProxyModel : public QSortFilterProxyModel
{
	typedef QSortFilterProxyModel Super;
public:
	LogFilterProxyModel(QObject* parent)
		: Super(parent) {

	}
	void setThreshold(int level) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
		beginFilterChange();
#endif
		m_treshold = level;
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
		endFilterChange(QSortFilterProxyModel::Direction::Rows);
#else
		invalidateFilter();
#endif
	}
	void setFilterString(const QString &filter_string) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
		beginFilterChange();
#endif
		m_filterString = filter_string;
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
		endFilterChange(QSortFilterProxyModel::Direction::Rows);
#else
		invalidateFilter();
#endif
	}
	bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const Q_DECL_OVERRIDE;
private:
	int m_treshold = static_cast<int>(NecroLog::Level::Info);
	QString m_filterString;
};

bool LogFilterProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
	auto *log_model = qobject_cast<qfm::LogTableModel*>(sourceModel());
	QModelIndex index = log_model->index(source_row, qfm::LogTableModel::Cols::Severity, source_parent);
	bool ok = (log_model->data(index, Qt::EditRole).toInt() <= m_treshold);
	//qfDebug() << log_model->data(index, Qt::EditRole).toInt() << m_treshold << ok;
	if(ok && !m_filterString.isEmpty()) {
		ok = false;
		index = log_model->index(source_row, 0, source_parent);
		for (int i = 0; i < qfm::LogTableModel::Cols::Count; ++i) {
			index = index.sibling(index.row(), i);
			QString s = log_model->data(index, Qt::DisplayRole).toString();
			if(s.contains(m_filterString, Qt::CaseInsensitive)) {
				ok = true;
				break;
			}
		}
	}
	return ok;
}

LogWidget::LogWidget(QWidget *parent)
	: Super(parent)
	, ui(new Ui::LogWidget)
{
	ui->setupUi(this);
	//setPersistentSettingsId();
	ui->btCopyToClipboard->setDefaultAction(ui->tableView->copySelectionToClipboardAction());

	{
		auto *a = new QAction(tr("Maximal log length"), this);
		connect(a, &QAction::triggered, this, [this]() {
			qf::gui::model::LogTableModel *m = this->logTableModel();
			int max_rows = m->maximumRowCount();
			bool ok;
			max_rows = QInputDialog::getInt(this, tr("Get number"), tr("Maximal log row count:"), max_rows, 0, std::numeric_limits<int>::max(), 100, &ok);
			if (ok)
				m->setMaximumRowCount(max_rows);
		});
		tableMenuButton()->addAction(a);
	}
	{
		auto *a = new QAction(this);
		a->setSeparator(true);
		tableMenuButton()->addAction(a);
	}

	//ui->tableView->horizontalHeader()->setSectionHidden(0, true);
	ui->tableView->horizontalHeader()->setSectionsMovable(true);
	ui->tableView->verticalHeader()->setDefaultSectionSize((int)(fontMetrics().lineSpacing() * 1.3));
	m_filterModel = new LogFilterProxyModel(this);
	m_filterModel->setDynamicSortFilter(false);
	ui->tableView->setModel(m_filterModel);

	int info_index = -1;
	for (int i = static_cast<int>(NecroLog::Level::Error); i <= static_cast<int>(NecroLog::Level::Debug); i++) {
		if(i == static_cast<int>(NecroLog::Level::Info))
			info_index = ui->severityTreshold->count();
		ui->severityTreshold->addItem(NecroLog::levelToString(static_cast<NecroLog::Level>(i)), QVariant::fromValue(i));
	}
	ui->severityTreshold->setCurrentIndex(info_index);
	connect(ui->severityTreshold, qOverload<int>(&QComboBox::currentIndexChanged), this, &LogWidget::onSeverityTresholdIndexChanged);

	connect(ui->edFilter, &QLineEdit::textChanged, this, &LogWidget::filterStringChanged);
	connect(ui->btClearLog, &QToolButton::clicked, this, &LogWidget::on_btClearLog_clicked);
	connect(ui->btResizeColumns, &QToolButton::clicked, this, &LogWidget::on_btResizeColumns_clicked);
}

LogWidget::~LogWidget()
{
	delete ui;
}

void LogWidget::clear()
{
	if(m_logTableModel)
		m_logTableModel->clear();
}

void LogWidget::setLogTableModel(gui::model::LogTableModel *m)
{
	if(m_logTableModel != m) {
		m_logTableModel = m;
		m_filterModel->setSourceModel(m_logTableModel);
		if(m_logTableModel) {
			connect(m_logTableModel, &gui::model::LogTableModel::logEntryInserted, this, &LogWidget::checkScrollToLastEntry, Qt::UniqueConnection);
			QScrollBar *sb = ui->tableView->verticalScrollBar();
			if(sb)
				connect(sb, &QScrollBar::valueChanged, this, &LogWidget::onVerticalScrollBarValueChanged, Qt::UniqueConnection);
		}
	}
}

void LogWidget::addLog(NecroLog::Level severity, const QString &category, const QString &file, int line, const QString &msg, const QDateTime &time_stamp, const QString &function, const QVariant &user_data)
{
	//fprintf(stderr, "%s:%d(%s) %s\n", qPrintable(file), line, qPrintable(category), qPrintable(msg));
	logTableModel()->addLog(severity, category, file, line, msg, time_stamp, function, user_data);
}

void LogWidget::addLogEntry(const qf::core::LogEntryMap &le)
{
	addLog(le.level(), le.category(), le.file(), le.line(), le.message(), le.timeStamp(), le.function());
}

qf::gui::model::LogTableModel *LogWidget::logTableModel()
{
	if(!m_logTableModel) {
		auto *m = new qfm::LogTableModel(this);
		setLogTableModel(m);
	}
	return m_logTableModel;
}

void LogWidget::setSeverityTreshold(NecroLog::Level lvl)
{
	int ci = ui->severityTreshold->findData(static_cast<int>(lvl));
	ui->severityTreshold->setCurrentIndex(ci);
}

NecroLog::Level LogWidget::severityTreshold() const
{
	return static_cast<NecroLog::Level>(ui->severityTreshold->currentData().toInt());
}

void LogWidget::onSeverityTresholdIndexChanged(int index)
{
	Q_UNUSED(index)
	m_filterModel->setThreshold(ui->severityTreshold->currentData().toInt());
	emit severityTresholdChanged(static_cast<NecroLog::Level>(ui->severityTreshold->currentData().toInt()));
}

QAbstractButton *LogWidget::clearButton()
{
	return ui->btClearLog;
}

void LogWidget::filterStringChanged(const QString &filter_string)
{
	m_filterModel->setFilterString(filter_string);
}

void LogWidget::on_btClearLog_clicked()
{
	clear();
}

void LogWidget::on_btResizeColumns_clicked()
{
	ui->tableView->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);
}

bool LogWidget::isAutoScroll()
{
	QScrollBar *sb = ui->tableView->verticalScrollBar();
	if(sb) {
		if(logTableModel()->direction() == qf::gui::model::LogTableModel::Direction::AppendToBottom) {
			//fprintf(stderr, "BOTTOM scrollbar min: %d max: %d value: %d\n", sb->minimum(), sb->maximum(), sb->value());
			return (sb->value() == sb->maximum());
		}
					//fprintf(stderr, "TOP scrollbar min: %d max: %d value: %d\n", sb->minimum(), sb->maximum(), sb->value());
			return (sb->value() == sb->minimum());

	}
	return false;
}

QAbstractButton *LogWidget::tableMenuButton()
{
	return ui->btTableMenu;
}

QTableView *LogWidget::tableView() const
{
	return ui->tableView;
}

void LogWidget::clearCategoryActions()
{
	m_logLevelActions.clear();
	qDeleteAll(m_loggingCategoriesMenus);
	m_loggingCategoriesMenus.clear();
}

void LogWidget::addCategoryActions(const QString &caption, const QString &id, NecroLog::Level level)
{
	QString menu_caption = "[%1] " + caption;
	auto *m = new QMenu(this);
	m_loggingCategoriesMenus << m;
	auto *a = new QAction(caption, m);
	a->setData(id);
	a->setMenu(m);
	tableMenuButton()->addAction(a);
	auto *ag_loglevel = new QActionGroup(a);
	for (int i = static_cast<int>(NecroLog::Level::Invalid); i <= static_cast<int>(NecroLog::Level::Debug); i++) {
		if(i == static_cast<int>(NecroLog::Level::Fatal))
			continue;
		QString cap = NecroLog::levelToString(static_cast<NecroLog::Level>(i));
		auto *a2 = new QAction(cap, a);
		ag_loglevel->addAction(a2);
		m->addAction(a2);
		a2->setCheckable(true);
		a2->setChecked(static_cast<NecroLog::Level>(i) == level);
		connect(a2, &QAction::triggered, this, &LogWidget::registerLogCategories);
		QChar level_c = (i == static_cast<int>(NecroLog::Level::Invalid))? ' ': NecroLog::levelToString(static_cast<NecroLog::Level>(i))[0];
		a2->setData(i);
		if(a2->isChecked())
			menu_caption = menu_caption.arg(level_c);
		m_logLevelActions << a2;
	}
	m->setTitle(menu_caption);
}

static QPair<QString, NecroLog::Level> parseCategoryLevel(const QString &category)
{
	int ix = category.indexOf(':');
	NecroLog::Level level = NecroLog::Level::Debug;
	QString cat = category;
	if(ix >= 0) {
		QString s = category.mid(ix + 1, 1);
		QChar l = s.isEmpty()? QChar(): s[0].toUpper();
		cat = category.mid(0, ix).trimmed();
		if(l == 'D')
			level = NecroLog::Level::Debug;
		else if(l == 'I')
			level = NecroLog::Level::Info;
		else if(l == 'W')
			level = NecroLog::Level::Warning;
		else if(l == 'E')
			level = NecroLog::Level::Error;
		else
			level = NecroLog::Level::Invalid;
	}
	return QPair<QString, NecroLog::Level>(cat, level);
}

QMap<QString, NecroLog::Level> LogWidget::selectedLogCategories() const
{
	QMap<QString, NecroLog::Level> categories;
	for(QAction *a : m_logLevelActions) {
		if(a->isChecked()) {
			auto *pa = qobject_cast<QAction*>(a->parent());
			QF_ASSERT(pa != nullptr, "Bad parent", continue);
			NecroLog::Level level = static_cast<NecroLog::Level>(a->data().toInt());
			QChar level_c = (level == NecroLog::Level::Invalid)? ' ': NecroLog::levelToString(level)[0];
			{
				QMenu *m = pa->menu();
				QString cap = m->title();
				cap[1] = level_c;
				m->setTitle(cap);
			}
			QString category = pa->data().toString();
			QPair<QString, NecroLog::Level> lev = parseCategoryLevel(category);
			//auto level = static_cast<NecroLog::Level>(pa->data().toInt());
			if(level != NecroLog::Level::Invalid)
				categories[category] = level;
		}
	}
	return categories;
}

void LogWidget::registerLogCategories()
{
	m_logCategoriesRegistered = true;
}

void LogWidget::onDockWidgetVisibleChanged(bool visible)
{
	if(visible) {
		if(!m_logCategoriesRegistered) {
			registerLogCategories();
		}
		checkScrollToLastEntry();
	}
}

void LogWidget::onVerticalScrollBarValueChanged()
{
}

void LogWidget::checkScrollToLastEntry()
{
	if(isVisible()) {
		if(!m_columnsResized) {
			m_columnsResized = true;
			ui->tableView->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);
		}
		if(isAutoScroll()) {
			if(logTableModel()->direction() == qf::gui::model::LogTableModel::Direction::AppendToBottom) {
				ui->tableView->scrollToBottom();
			}
			else {
				ui->tableView->scrollToTop();
			}
		}
	}
}

} // namespace qf::gui::framework
// namespace qmlwiggets

