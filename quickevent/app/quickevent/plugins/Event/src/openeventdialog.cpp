#include "openeventdialog.h"
#include "ui_openeventdialog.h"
#include "eventconfig.h"
#include "eventdialogwidget.h"

#include <qf/core/collator.h>
#include <qf/core/utils.h>
#include <qf/gui/model/tablemodel.h>
#include <qf/gui/style.h>

#include <QApplication>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLineEdit>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QStandardItemModel>
#include <QStyleOptionButton>
#include <QStyledItemDelegate>

namespace Event {

namespace {

enum Col { ColId = 0, ColDate, ColName, ColSport, ColDiscipline, ColDbVersion, ColAction, ColCount };

// Roles above TableModel::FirstUnusedRole to avoid conflicts with libqf roles.
constexpr int EventIdRole = qf::gui::model::TableModel::FirstUnusedRole;
constexpr int IsOlderRole = qf::gui::model::TableModel::FirstUnusedRole + 1;

} // namespace

// Extends QSortFilterProxyModel with per-cell yellow highlighting for matching text.
class EventFilterProxy : public QSortFilterProxyModel
{
public:
	using QSortFilterProxyModel::QSortFilterProxyModel;

	void setFilter(const QString &text)
	{
		m_filter = qf::core::Collator::toAscii7(QLocale::Czech, text, true);
		setFilterFixedString(text);
	}

	QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override
	{
		if (role == Qt::BackgroundRole && !m_filter.isEmpty()) {
			const QString cell = Super::data(index, Qt::DisplayRole).toString();
			const QByteArray hay = qf::core::Collator::toAscii7(QLocale::Czech, cell, true);
			if (hay.contains(m_filter)) {
				static const QBrush highlight = qf::gui::isDarkTheme() ? QColor("olive") : QColor(Qt::yellow);
				return highlight;
			}
		}
		return Super::data(index, role);
	}

private:
	using Super = QSortFilterProxyModel;
	QByteArray m_filter;
};

// Draws Open/Convert + Delete buttons inside the action column cell; emits signals on click.
class ActionDelegate : public QStyledItemDelegate
{
	Q_OBJECT
public:
	explicit ActionDelegate(const QIcon &delete_icon, QObject *parent)
		: QStyledItemDelegate(parent), m_deleteIcon(delete_icon) {}

	QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override
	{
		return {110, 26};
	}

	void paint(QPainter *painter, const QStyleOptionViewItem &option,
	           const QModelIndex &index) const override
	{
		const QVariant bg = index.data(Qt::BackgroundRole);
		painter->fillRect(option.rect, bg.isValid() ? bg.value<QBrush>() : option.backgroundBrush);

		const bool is_older = index.data(IsOlderRole).toBool();
		const auto [open_rect, del_rect] = buttonRects(option.rect);

		QStyleOptionButton open_opt;
		open_opt.rect  = open_rect;
		open_opt.state = QStyle::State_Enabled | QStyle::State_Active;
		open_opt.text  = is_older
			? QCoreApplication::translate("OpenEventDialog", "Convert")
			: QCoreApplication::translate("OpenEventDialog", "Open");
		QApplication::style()->drawControl(QStyle::CE_PushButton, &open_opt, painter);

		QStyleOptionButton del_opt;
		del_opt.rect     = del_rect;
		del_opt.state    = QStyle::State_Enabled | QStyle::State_Active;
		del_opt.features = QStyleOptionButton::Flat;
		del_opt.icon     = m_deleteIcon;
		del_opt.iconSize = QSize(16, 16);
		QApplication::style()->drawControl(QStyle::CE_PushButton, &del_opt, painter);
	}

	bool editorEvent(QEvent *event, QAbstractItemModel *,
	                 const QStyleOptionViewItem &option, const QModelIndex &index) override
	{
		if (event->type() != QEvent::MouseButtonRelease)
			return false;
		const auto *me = static_cast<const QMouseEvent *>(event);
		const auto [open_rect, del_rect] = buttonRects(option.rect);
		const QString event_id = index.data(EventIdRole).toString();
		if (open_rect.contains(me->pos())) {
			if (index.data(IsOlderRole).toBool())
				emit convertRequested(event_id);
			else
				emit openRequested(event_id);
			return true;
		}
		if (del_rect.contains(me->pos())) {
			emit deleteRequested(event_id);
			return true;
		}
		return false;
	}

signals:
	void openRequested(const QString &event_id);
	void convertRequested(const QString &event_id);
	void deleteRequested(const QString &event_id);

private:
	static std::pair<QRect, QRect> buttonRects(const QRect &cell)
	{
		constexpr int del_w  = 26;
		constexpr int margin = 3;
		const int open_w = cell.width() - del_w - margin * 3;
		const QRect open_rect(cell.left() + margin,        cell.top() + 2, open_w, cell.height() - 4);
		const QRect del_rect (open_rect.right() + margin,  cell.top() + 2, del_w,  cell.height() - 4);
		return {open_rect, del_rect};
	}

	QIcon m_deleteIcon;
};

// --- OpenEventDialog ---

OpenEventDialog::OpenEventDialog(const QList<EventInfo> &events, int appDbVersion,
                                 const QStringList &existing_names, QWidget *parent)
	: QDialog(parent), m_appDbVersion(appDbVersion), m_existingNames(existing_names)
{
	ui = new Ui::OpenEventDialog();
	ui->setupUi(this);

	const bool dark = qf::gui::isDarkTheme();
	const QColor compatible_bg = dark ? QColor(30,  90, 30) : QColor(200, 245, 200);
	const QColor older_bg      = dark ? QColor(90,  25, 25) : QColor(255, 190, 190);

	// Build source model
	m_model = new QStandardItemModel(events.count(), ColCount, this);
	m_model->setHorizontalHeaderLabels({
		tr("ID"), tr("Date"), tr("Name"),
		tr("Sport"), tr("Discipline"), tr("DB version"), tr("Action")
	});

	for (int row = 0; row < events.count(); ++row) {
		const EventInfo &info = events[row];
		const bool older = (info.dbVersion > 0 && info.dbVersion < appDbVersion);
		const QBrush bg(older ? older_bg : compatible_bg);

		// TableViewProxyModel sorts via TableModel::SortRole; store text there so
		// variantCmp() uses collator-aware string comparison for all string columns.
		auto makeItem = [&](const QString &text, Qt::AlignmentFlag align = Qt::AlignLeft) {
			auto *item = new QStandardItem(text);
			item->setTextAlignment(align | Qt::AlignVCenter);
			item->setBackground(bg);
			item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
			item->setData(text, qf::gui::model::TableModel::SortRole);
			return item;
		};

		const QString db_ver_str = info.dbVersion > 0
			? qf::core::Utils::intToVersionString(info.dbVersion)
			: tr("?");

		m_model->setItem(row, ColId, makeItem(info.id));
		m_model->setItem(row, ColDate, makeItem(
			info.date.isValid() ? info.date.toString(QStringLiteral("yyyy-MM-dd")) : QString(),
			Qt::AlignCenter));
		m_model->setItem(row, ColName, makeItem(info.name));
		m_model->setItem(row, ColSport, makeItem(EventDialogWidget::sportName(info.sportId), Qt::AlignCenter));
		m_model->setItem(row, ColDiscipline, makeItem(EventDialogWidget::disciplineName(info.disciplineId), Qt::AlignCenter));

		// Store integer in SortRole so variantCmp() uses QMetaType::Int comparison.
		auto *ver_item = makeItem(db_ver_str, Qt::AlignCenter);
		ver_item->setData(info.dbVersion, qf::gui::model::TableModel::SortRole);
		m_model->setItem(row, ColDbVersion, ver_item);

		auto *action_item = new QStandardItem();
		action_item->setBackground(bg);
		action_item->setFlags(Qt::ItemIsEnabled);
		action_item->setData(info.id, EventIdRole);
		action_item->setData(older, IsOlderRole);
		m_model->setItem(row, ColAction, action_item);
	}

	auto *proxy = new EventFilterProxy(this);
	proxy->setSourceModel(m_model);
	proxy->setSortRole(qf::gui::model::TableModel::SortRole);
	proxy->setFilterKeyColumn(-1);
	proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
	m_proxy = proxy;
	ui->tableView->setModel(m_proxy);
	ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
	ui->tableView->setSelectionMode(QAbstractItemView::SingleSelection);
	ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
	ui->tableView->setAlternatingRowColors(false);
	ui->tableView->setSortingEnabled(true);
	ui->tableView->verticalHeader()->setVisible(false);
	ui->tableView->setShowGrid(false);

	auto *action_delegate = new ActionDelegate(qf::gui::Style::icon(QStringLiteral("delete")), ui->tableView);
	ui->tableView->setItemDelegateForColumn(ColAction, action_delegate);
	connect(action_delegate, &ActionDelegate::openRequested, this, &OpenEventDialog::onOpenClicked);
	connect(action_delegate, &ActionDelegate::convertRequested, this, &OpenEventDialog::onConvertClicked);
	connect(action_delegate, &ActionDelegate::deleteRequested, this, &OpenEventDialog::onDeleteClicked);

	auto *hdr = ui->tableView->horizontalHeader();
	hdr->setSectionResizeMode(ColId, QHeaderView::Interactive);
	hdr->setSectionResizeMode(ColDate, QHeaderView::Interactive);
	hdr->setSectionResizeMode(ColName, QHeaderView::Stretch);
	hdr->setSectionResizeMode(ColSport, QHeaderView::Interactive);
	hdr->setSectionResizeMode(ColDiscipline, QHeaderView::Interactive);
	hdr->setSectionResizeMode(ColDbVersion, QHeaderView::Interactive);
	hdr->setSectionResizeMode(ColAction, QHeaderView::ResizeToContents);
	hdr->setStretchLastSection(false);

	hdr->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(hdr, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
		const int col = ui->tableView->horizontalHeader()->logicalIndexAt(pos);
		QMenu menu(this);
		if (col >= 0 && col != ColAction) {
			menu.addAction(tr("Resize section to contents"), [this, col]() {
				ui->tableView->resizeColumnToContents(col);
			});
		}
		menu.addAction(tr("Resize all sections to contents"), [this]() {
			ui->tableView->resizeColumnsToContents();
		});
		menu.exec(ui->tableView->horizontalHeader()->viewport()->mapToGlobal(pos));
	});

	ui->tableView->resizeColumnsToContents();
	ui->tableView->sortByColumn(ColDate, Qt::DescendingOrder);

	connect(ui->searchEdit, &QLineEdit::textChanged, proxy, &EventFilterProxy::setFilter);

	ui->legend->setText(
		QStringLiteral("<span style='background:%1;border:1px solid gray'>&nbsp;&nbsp;&nbsp;</span> %2"
		               "&nbsp;&nbsp;&nbsp;"
		               "<span style='background:%3;border:1px solid gray'>&nbsp;&nbsp;&nbsp;</span> %4")
		.arg(compatible_bg.name(), tr("Compatible"),
		     older_bg.name(),      tr("Older version (convert required)"))
	);
}

OpenEventDialog::~OpenEventDialog()
{
	delete ui;
}

void OpenEventDialog::onOpenClicked(const QString &event_id)
{
	m_selectedEventId = event_id;
	m_selectedAction  = RowAction::Open;
	accept();
}

void OpenEventDialog::onConvertClicked(const QString &event_id)
{
	const QString suffix = QStringLiteral("_db%1").arg(m_appDbVersion);
	QString suggested = event_id + suffix;
	for (int n = 2; m_existingNames.contains(suggested); ++n)
		suggested = event_id + suffix + QString::number(n);

	QDialog dlg(this);
	dlg.setWindowTitle(tr("Convert event"));
	auto *ly = new QVBoxLayout(&dlg);
	ly->setSpacing(8);

	auto *info_label = new QLabel(
		tr("Convert event <b>%1</b> to the current version.<br>"
		   "A new event will be created with the ID below.").arg(event_id), &dlg);
	info_label->setWordWrap(true);
	ly->addWidget(info_label);

	ly->addWidget(new QLabel(tr("New event ID:"), &dlg));
	auto *id_edit = new QLineEdit(suggested, &dlg);
	id_edit->setMinimumWidth(350);
	id_edit->selectAll();
	ly->addWidget(id_edit);

	auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
	auto *ok_btn = btns->button(QDialogButtonBox::Ok);
	connect(id_edit, &QLineEdit::textChanged, ok_btn, [ok_btn](const QString &t) {
		ok_btn->setEnabled(!t.trimmed().isEmpty());
	});
	connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
	connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
	ly->addWidget(btns);

	if (dlg.exec() != QDialog::Accepted)
		return;

	const QString final_id = id_edit->text().trimmed();
	if (final_id.isEmpty())
		return;

	m_selectedEventId  = event_id;
	m_convertedEventId = final_id;
	m_selectedAction   = RowAction::Convert;
	accept();
}

void OpenEventDialog::onDeleteClicked(const QString &event_id)
{
	const auto res = QMessageBox::warning(this, tr("Delete event"),
		tr("Permanently delete event <b>%1</b>?<br>This action cannot be undone.").arg(event_id),
		QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

	if (res == QMessageBox::Yes) {
		m_selectedEventId = event_id;
		m_selectedAction  = RowAction::Delete;
		accept();
	}
}

} // namespace Event

#include "openeventdialog.moc"
