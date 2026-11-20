#include "ochecklistimportdialog.h"
#include "runsplugin.h"

#include <plugins/Competitors/src/competitordocument.h>
#include <plugins/Event/src/eventplugin.h>

#include <qf/core/collator.h>
#include <qf/core/exception.h>
#include <qf/core/log.h>
#include <qf/core/sql/query.h>
#include <qf/core/sql/qxsql.h>
#include <qf/core/utils/table.h>
#include <qf/gui/dialogs/messagebox.h>
#include <qf/gui/framework/application.h>
#include <qf/gui/framework/mainwindow.h>
#include <qf/gui/model/tablemodel.h>
#include <qf/gui/tableview.h>
#include <qf/gui/tableviewtoolbar.h>

#include <QCheckBox>
#include <QColor>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimeZone>
#include <QVBoxLayout>
#include <QVersionNumber>

#include <algorithm>
#include <functional>

using qf::gui::framework::getPlugin;

namespace {

/// report version the import is implemented for
const QVersionNumber k_supported_version(1, 6);
const auto k_source = QStringLiteral("OChecklist");

const auto k_status_dns = QStringLiteral("DNS");
const auto k_status_late_start = QStringLiteral("Late start");

enum Column { col_apply = 0, col_state, col_data_type, col_runner, col_class, col_card, col_orig_data, col_data, col_created, col_match };

/// the Apply check box is disabled for changes that cannot be applied, their Matched by is red
class ChangesModel : public qf::gui::model::TableModel
{
public:
	using TableModel::TableModel;

	std::function<bool (int row)> isRowCheckable;
	std::function<bool (int row)> isRowWarning;

	Qt::ItemFlags flags(const QModelIndex &index) const override
	{
		Qt::ItemFlags ret = TableModel::flags(index);
		if (index.column() == col_apply && !isRowCheckable(index.row())) {
			ret &= ~(Qt::ItemIsUserCheckable | Qt::ItemIsEditable | Qt::ItemIsEnabled);
		}
		return ret;
	}
	QVariant data(const QModelIndex &index, int role) const override
	{
		if (role == Qt::ForegroundRole && index.column() == col_match && isRowWarning(index.row())) {
			return QColor(Qt::red);
		}
		return TableModel::data(index, role);
	}
};

QVariant yamlScalar(const QString &s)
{
	if (s.isEmpty() || s == QLatin1String("null") || s == QLatin1String("~")) {
		return {};
	}
	if (s.size() >= 2 && s.startsWith('\'') && s.endsWith('\'')) {
		return s.mid(1, s.size() - 2).replace(QLatin1String("''"), QLatin1String("'"));
	}
	if (s.size() >= 2 && s.startsWith('"') && s.endsWith('"')) {
		QString ret;
		const int end = s.size() - 1;
		for (int i = 1; i < end; ++i) {
			if (s[i] != '\\' || i + 1 >= end) {
				ret += s[i];
				continue;
			}
			const QChar esc = s[++i];
			if (esc == 'n') {
				ret += '\n';
			}
			else if (esc == 't') {
				ret += '\t';
			}
			else if (esc == 'u' && i + 4 < end) {
				ret += QChar(s.mid(i + 1, 4).toUShort(nullptr, 16));
				i += 4;
			}
			else {
				// \" \\ \/
				ret += esc;
			}
		}
		return ret;
	}
	return s;
}

QString normalizedName(const QString &s)
{
	return QString::fromLatin1(qf::core::Collator::toAscii7(QLocale::Czech, s, true)).simplified();
}

struct DbRun
{
	int runId = 0;
	int competitorId = 0;
	bool notStart = false;
	QString siId;
	QString firstName;
	QString lastName;
	QString registration;
	QString iofId;
	QString note;
	QString className;
	QString corridorTime;

	QString name() const { return (firstName + ' ' + lastName).simplified(); }
	bool nameMatches(const QString &normalized_name) const
	{
		return normalized_name == normalizedName(firstName + ' ' + lastName)
			|| normalized_name == normalizedName(lastName + ' ' + firstName);
	}
};

}

// ponytail: line based parser of the flat O Checklist report only (key: value, indentation, "- " items),
// it does not support YAML in general (flow style, multi-line scalars, anchors)
OChecklistImportDialog::Report OChecklistImportDialog::parseReport(const QString &text)
{
	Report ret;
	QStringList path;
	QList<int> indents;
	const auto lines = text.split('\n');
	for (QString line : lines) {
		if (line.endsWith('\r')) {
			line.chop(1);
		}
		const QString trimmed = line.trimmed();
		if (trimmed.isEmpty() || trimmed.startsWith('#')) {
			continue;
		}
		ret.dataLineCount++;
		int indent = 0;
		while (indent < line.size() && line[indent] == ' ') {
			indent++;
		}
		QString content = line.mid(indent);
		if (content.startsWith(QLatin1String("- "))) {
			// new item of the Data list
			ret.runners.append(QVariantMap());
			content = content.mid(2).trimmed();
			indent += 2;
		}
		while (!indents.isEmpty() && indents.last() >= indent) {
			indents.removeLast();
			path.removeLast();
		}
		// keys never contain a colon, values can (times)
		const int colon = content.indexOf(':');
		if (colon <= 0) {
			qfWarning() << "O Checklist report, invalid line skipped:" << line;
			continue;
		}
		const QString key = content.left(colon).trimmed();
		const QString value = content.mid(colon + 1).trimmed();
		if (value.isEmpty()) {
			indents << indent;
			path << key;
			continue;
		}
		const QVariant v = yamlScalar(value);
		if (path.isEmpty()) {
			ret.meta[key] = v;
		}
		else if (path.first() == QLatin1String("EndOfData")) {
			ret.endOfData[key] = v;
		}
		else if (path.first() == QLatin1String("Data") && path.size() == 2 && !ret.runners.isEmpty()) {
			// path[1] is Runner or ChangeLog
			QVariantMap section = ret.runners.last().value(path[1]).toMap();
			section[key] = v;
			ret.runners.last()[path[1]] = section;
		}
	}
	return ret;
}

OChecklistImportDialog::OChecklistImportDialog(const Report &report, int stage_id, QWidget *parent)
	: Super(parent)
	, m_stageId(stage_id)
{
	setWindowTitle(tr("O Checklist changes"));
	resize(1100, 650);

	QStringList info {
		tr("Event: %1").arg(report.meta.value("Event").toString()),
		tr("Created: %1").arg(report.meta.value("Created").toString()),
		tr("Creator: %1").arg(report.meta.value("Creator").toString()),
		tr("Version: %1").arg(report.meta.value("Version").toString()),
		tr("Runners: %1").arg(report.runners.count()),
	};
	QStringList warnings;
	const auto version = QVersionNumber::fromString(report.meta.value("Version").toString());
	if (version > k_supported_version) {
		warnings << tr("Report version %1 is newer than supported version %2.").arg(version.toString(), k_supported_version.toString());
	}
	if (const auto n = report.endOfData.value("NumRunners"); n.isNull() || n.toInt() != report.runners.count()) {
		warnings << tr("Report declares %1 runners, %2 found, the file may be incomplete.").arg(n.toString()).arg(report.runners.count());
	}
	if (const auto n = report.endOfData.value("NumLines"); n.isNull() || n.toInt() != report.dataLineCount) {
		warnings << tr("Report declares %1 lines, %2 found, the file may be incomplete.").arg(n.toString()).arg(report.dataLineCount);
	}
	for (const auto &w : warnings) {
		qfWarning() << "O Checklist import:" << w;
	}

	auto *lbl_info = new QLabel(info.join(QStringLiteral(", ")));
	auto *lbl_warnings = new QLabel(warnings.join('\n'));
	lbl_warnings->setStyleSheet(QStringLiteral("color: red"));
	lbl_warnings->setVisible(!warnings.isEmpty());

	auto *model = new ChangesModel(this);
	model->addColumn("apply", tr("Apply"));
	model->addColumn("state", tr("State")).setReadOnly(true);
	model->addColumn("dataType", tr("Change")).setReadOnly(true);
	model->addColumn("runner", tr("Runner")).setReadOnly(true);
	model->addColumn("className", tr("Class")).setReadOnly(true);
	model->addColumn("card", tr("Card")).setReadOnly(true);
	model->addColumn("origData", tr("Original value")).setReadOnly(true);
	model->addColumn("data", tr("New value")).setReadOnly(true);
	model->addColumn("created", tr("Changed")).setReadOnly(true).setFormat(QStringLiteral("yyyy-MM-dd hh:mm:ss"));
	model->addColumn("match", tr("Matched by")).setReadOnly(true);
	model->isRowCheckable = [this](int row) { return changeForRow(row).isCheckable(); };
	model->isRowWarning = [this](int row) { return !changeForRow(row).isCheckable() || changeForRow(row).nameMismatch; };
	m_model = model;

	m_tableView = new qf::gui::TableView();
	m_tableView->setInsertRowEnabled(false);
	m_tableView->setRemoveRowEnabled(false);
	m_tableView->setCloneRowEnabled(false);
	m_tableView->setDirtyRowsMenuSectionEnabled(false);
	m_tableView->setTableModel(m_model);
	auto *tool_bar = new qf::gui::TableViewToolBar();
	tool_bar->setTableView(m_tableView);

	loadChanges(report);
	reloadTable();

	auto *chk_only_new = new QCheckBox(tr("Only new changes"));
	chk_only_new->setChecked(m_onlyNew);
	connect(chk_only_new, &QCheckBox::toggled, this, [this](bool on) {
		saveApplyFlags();
		m_onlyNew = on;
		reloadTable();
	});

	auto check_visible = [this](bool on) {
		auto *view_model = m_tableView->model();
		for (int row = 0; row < view_model->rowCount(); ++row) {
			const int model_row = m_tableView->toTableModelIndex(view_model->index(row, col_apply)).row();
			if (changeForRow(model_row).isCheckable()) {
				m_model->setData(m_model->index(model_row, col_apply), on? Qt::Checked: Qt::Unchecked, Qt::CheckStateRole);
				m_model->postRow(model_row, !qf::core::Exception::Throw);
			}
		}
	};
	auto *bt_all = new QPushButton(tr("Check visible"));
	connect(bt_all, &QPushButton::clicked, this, [check_visible]() { check_visible(true); });
	auto *bt_none = new QPushButton(tr("Uncheck visible"));
	connect(bt_none, &QPushButton::clicked, this, [check_visible]() { check_visible(false); });

	auto *filter_layout = new QHBoxLayout();
	filter_layout->addWidget(tool_bar, 1);
	filter_layout->addWidget(chk_only_new);
	filter_layout->addWidget(bt_all);
	filter_layout->addWidget(bt_none);

	auto *button_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	button_box->button(QDialogButtonBox::Ok)->setText(tr("Apply checked changes"));
	connect(button_box, &QDialogButtonBox::accepted, this, &OChecklistImportDialog::accept);
	connect(button_box, &QDialogButtonBox::rejected, this, &OChecklistImportDialog::reject);

	auto *layout = new QVBoxLayout(this);
	layout->addWidget(lbl_info);
	layout->addWidget(lbl_warnings);
	layout->addLayout(filter_layout);
	layout->addWidget(m_tableView, 1);
	layout->addWidget(new QLabel(tr("Unchecked new changes are stored as rejected and they are not offered by the next import.")));
	layout->addWidget(button_box);
}

void OChecklistImportDialog::loadChanges(const Report &report)
{
	QHash<int, DbRun> runs;
	qf::core::sql::Query q;
	// only running ones, the exported start list contains them only
	q.execThrow(QStringLiteral("SELECT runs.id, runs.siId, runs.notStart, runs.corridorTime,"
				" competitors.id AS competitorId, competitors.firstName, competitors.lastName, competitors.registration, competitors.iofId, competitors.note,"
				" classes.name AS className"
				" FROM runs"
				" JOIN competitors ON competitors.id = runs.competitorId"
				" LEFT JOIN classes ON classes.id = competitors.classId"
				" WHERE runs.stageId=%1 AND runs.isRunning").arg(m_stageId));
	while (q.next()) {
		DbRun r;
		r.runId = q.value("id").toInt();
		r.competitorId = q.value("competitorId").toInt();
		r.notStart = q.value("notStart").toBool();
		r.siId = q.value("siId").toInt() > 0? q.value("siId").toString(): QString();
		r.firstName = q.value("firstName").toString();
		r.lastName = q.value("lastName").toString();
		r.registration = q.value("registration").toString();
		r.iofId = q.value("iofId").toString();
		r.note = q.value("note").toString();
		r.className = q.value("className").toString();
		r.corridorTime = q.value("corridorTime").toString();
		runs.insert(r.runId, r);
	}

	// changes imported already, note and corridor time are never applied twice,
	// status and card change only when they differ from the last imported one
	QHash<QString, State> imported;
	QHash<QString, QPair<QString, State>> last_imported;
	// created runners by their data
	QHash<QString, State> imported_new_runners;
	q.execThrow(QStringLiteral("SELECT foreign_id, data_type, data, status FROM qxchanges WHERE source='%1' AND stage_id=%2 AND foreign_table='runs' ORDER BY id")
				.arg(k_source).arg(m_stageId));
	while (q.next()) {
		const QString run_type = q.value(0).toString() + '|' + q.value(1).toString();
		const QString data = q.value(2).toString();
		const State state = q.value(3).toString() == QLatin1String("Accepted")? State::Accepted: State::Rejected;
		imported[run_type + '|' + data] = state;
		last_imported[run_type] = {data, state};
		if (q.value(1).toString() == dataTypeName(DataType::NewRunner)) {
			imported_new_runners[data] = state;
		}
	}

	QHash<QString, int> class_ids;
	q.execThrow(QStringLiteral("SELECT id, name FROM classes"));
	while (q.next()) {
		class_ids[q.value(1).toString()] = q.value(0).toInt();
	}

	auto find_in_stage = [&runs](const std::function<bool (const DbRun &)> &pred) -> QList<const DbRun*> {
		QList<const DbRun*> ret;
		for (const auto &r : runs) {
			if (pred(r)) {
				ret << &r;
			}
		}
		return ret;
	};

	for (const auto &item : report.runners) {
		const QVariantMap runner = item.value("Runner").toMap();
		const QVariantMap changelog = item.value("ChangeLog").toMap();
		const QString name = runner.value("Name").toString();
		const QString normalized_name = normalizedName(name);
		const QString yaml_id = runner.value("Id").toString().trimmed();
		const QString yaml_card = runner.value("Card").toString();
		const QString yaml_class = runner.value("ClassName").toString();

		const DbRun *run = nullptr;
		QString match;
		bool name_mismatch = false;
		// the Id of the IOF XML start list is runs.id, IOF ID or registration, IOF IDs are much higher than runs.id
		if (bool ok; yaml_id.toInt(&ok) > 0 && ok && runs.contains(yaml_id.toInt())) {
			run = &runs[yaml_id.toInt()];
			match = tr("Id");
		}
		if (!run && !yaml_id.isEmpty()) {
			if (auto lst = find_in_stage([&yaml_id](const DbRun &r) { return r.iofId == yaml_id; }); lst.count() == 1) {
				run = lst.first();
				match = tr("IOF ID");
			}
		}
		if (run) {
			name_mismatch = !run->nameMatches(normalized_name);
			if (name_mismatch) {
				match = tr("%1, name differs: %2").arg(match, run->name());
			}
		}
		if (!run && !yaml_id.isEmpty()) {
			// Id of the IOF XML start list can be the registration, it is the first Id there
			if (auto lst = find_in_stage([&yaml_id](const DbRun &r) { return r.registration.compare(yaml_id, Qt::CaseInsensitive) == 0; }); lst.count() == 1) {
				run = lst.first();
				match = tr("Registration");
			}
		}
		if (!run && !yaml_card.isEmpty()) {
			if (auto lst = find_in_stage([&](const DbRun &r) { return r.siId == yaml_card && r.nameMatches(normalized_name); }); lst.count() == 1) {
				run = lst.first();
				match = tr("Card and name");
			}
		}
		if (!run) {
			if (auto lst = find_in_stage([&](const DbRun &r) { return r.className == yaml_class && r.nameMatches(normalized_name); }); lst.count() == 1) {
				run = lst.first();
				match = tr("Name and class");
			}
		}
		if (!run && changelog.contains("NewRunner")) {
			// added in O Checklist, the runner is created with all its start data in one change
			m_changes << newRunnerChange(runner, changelog, class_ids, imported_new_runners);
			continue;
		}
		if (!run) {
			match = tr("Not found");
		}

		auto add_change = [&](DataType data_type, const QString &orig_data, const QString &data, const QVariant &created, bool no_difference) {
			Change c;
			c.runnerName = run? run->name(): name;
			c.className = run? run->className: yaml_class;
			c.card = run? run->siId: yaml_card;
			c.dataType = data_type;
			c.origData = orig_data;
			c.data = data;
			c.created = QDateTime::fromString(created.toString(), Qt::ISODate);
			c.match = match;
			c.nameMismatch = name_mismatch;
			if (!run) {
				c.state = State::NotFound;
			}
			else {
				c.runId = run->runId;
				c.competitorId = run->competitorId;
				const QString run_type = QString::number(c.runId) + '|' + dataTypeName(data_type);
				const bool is_append = data_type == DataType::Note || data_type == DataType::CorridorTime;
				if (is_append && imported.contains(run_type + '|' + data)) {
					c.state = imported.value(run_type + '|' + data);
				}
				else if (!is_append && last_imported.contains(run_type) && last_imported.value(run_type).first == data) {
					c.state = last_imported.value(run_type).second;
				}
				else {
					c.state = no_difference? State::NoDifference: State::New;
				}
			}
			c.apply = c.state == State::New && !c.nameMismatch;
			m_changes << c;
		};

		const QString status = runner.value("StartStatus").toString();
		const bool is_dns = status == k_status_dns;
		const bool is_late_start = status == k_status_late_start;
		add_change(DataType::Status, run && run->notStart? k_status_dns: QString(), status,
				changelog.value(is_dns? "DNS": is_late_start? "LateStart": "TickedOff"),
				run && run->notStart == is_dns);
		if (const QString card = runner.value("NewCard").toString(); !card.isEmpty()) {
			add_change(DataType::Card, run? run->siId: QString(), card, changelog.value("NewCard"), run && run->siId == card);
		}
		if (const QString comment = runner.value("Comment").toString(); !comment.isEmpty()) {
			add_change(DataType::Note, run? run->note: QString(), comment, changelog.value("Comment"), run && run->note.contains(comment));
		}
		if (!is_dns) {
			// the corridor is entered by the tick off or by the late start
			if (const QString corridor_time = changelog.value(is_late_start? "LateStart": "TickedOff").toString(); !corridor_time.isEmpty()) {
				add_change(DataType::CorridorTime, run? run->corridorTime: QString(), corridor_time, corridor_time, false);
			}
		}
	}

	std::stable_sort(m_changes.begin(), m_changes.end(), [](const Change &a, const Change &b) {
		return qf::core::Collator::toAscii7(QLocale::Czech, a.runnerName, true) < qf::core::Collator::toAscii7(QLocale::Czech, b.runnerName, true);
	});
}

OChecklistImportDialog::Change OChecklistImportDialog::newRunnerChange(const QVariantMap &runner, const QVariantMap &changelog, const QHash<QString, int> &class_ids, const QHash<QString, State> &imported_new_runners) const
{
	const QString name = runner.value("Name").toString().simplified();
	const QString class_name = runner.value("ClassName").toString();
	const QString new_card = runner.value("NewCard").toString();
	const QString card = new_card.isEmpty()? runner.value("Card").toString(): new_card;
	const QString status = runner.value("StartStatus").toString();
	const bool is_dns = status == k_status_dns;
	// the last word is the last name, the rest is the first name
	const int last_space = name.lastIndexOf(' ');

	QString start_time;
	if (runner.value("StartType").toString() == QLatin1String("Fixed")) {
		start_time = runner.value("StartTime").toString();
		if (!start_time.isEmpty() && !start_time.contains('T')) {
			// time only (Excel import to O Checklist), it is the time of the stage day
			const QDateTime stage_start = getPlugin<Event::EventPlugin>()->stageStartDateTime(m_stageId);
			start_time = QDateTime(stage_start.date(), QTime::fromString(start_time, Qt::ISODate), stage_start.timeZone()).toString(Qt::ISODate);
		}
	}

	Change c;
	c.dataType = DataType::NewRunner;
	c.runnerName = name;
	c.className = class_name;
	c.card = card;
	c.data = QStringList{name, class_name, card}.join(QStringLiteral(", "));
	c.created = QDateTime::fromString(changelog.value("NewRunner").toString(), Qt::ISODate);
	c.newRunner = {
		{"firstName", last_space < 0? QString(): name.left(last_space)},
		{"lastName", name.mid(last_space + 1)},
		{"classId", class_ids.value(class_name)},
		{"siId", card.toInt() > 0? QVariant(card.toInt()): QVariant()},
		{"club", runner.value("Org")},
		{"note", runner.value("Comment")},
		{"notStart", is_dns},
		{"corridorTime", is_dns? QString(): changelog.value(status == k_status_late_start? "LateStart": "TickedOff").toString()},
		{"startTime", start_time},
	};
	if (!class_ids.contains(class_name)) {
		c.state = State::NotFound;
		c.match = tr("Class %1 not found").arg(class_name);
	}
	else {
		c.state = imported_new_runners.value(c.data, State::New);
		c.match = tr("Added in O Checklist");
	}
	c.apply = c.state == State::New;
	return c;
}

void OChecklistImportDialog::reloadTable()
{
	qf::core::utils::Table::FieldList fields;
	auto add_field = [&fields](const QString &name, QMetaType::Type type) {
		qf::core::utils::Table::Field fld(name, type);
		fld.setCanUpdate(name == QLatin1String("apply"));
		fields << fld;
	};
	add_field("idx", QMetaType::Int);
	add_field("apply", QMetaType::Bool);
	for (const auto *name : {"state", "dataType", "runner", "className", "card", "origData", "data", "match"}) {
		add_field(QString::fromLatin1(name), QMetaType::QString);
	}
	add_field("created", QMetaType::QDateTime);

	qf::core::utils::Table table(fields);
	for (int i = 0; i < m_changes.count(); ++i) {
		const Change &c = m_changes[i];
		if (m_onlyNew && c.state != State::New) {
			continue;
		}
		auto &row = table.appendRow();
		row.setValue("idx", i);
		row.setValue("apply", c.apply);
		row.setValue("state", stateText(c.state));
		row.setValue("dataType", dataTypeText(c.dataType));
		row.setValue("runner", c.runnerName);
		row.setValue("className", c.className);
		row.setValue("card", c.card);
		row.setValue("origData", c.origData);
		row.setValue("data", c.data);
		row.setValue("created", c.created.toLocalTime());
		row.setValue("match", c.match);
		row.clearEditFlags();
	}
	m_model->setTable(table);
	m_model->reload();
	m_tableView->resizeColumnsToContents();
}

void OChecklistImportDialog::saveApplyFlags()
{
	for (int row = 0; row < m_model->rowCount(); ++row) {
		m_changes[m_model->tableRow(row).value("idx").toInt()].apply = m_model->value(row, col_apply).toBool();
	}
}

const OChecklistImportDialog::Change& OChecklistImportDialog::changeForRow(int model_row) const
{
	return m_changes[m_model->tableRow(model_row).value("idx").toInt()];
}

void OChecklistImportDialog::accept()
{
	int applied = 0;
	int rejected = 0;
	QStringList errors;
	saveApplyFlags();
	for (Change &c : m_changes) {
		if (!c.isCheckable()) {
			continue;
		}
		try {
			if (c.apply) {
				applyChange(c);
				storeChange(c, true);
				applied++;
			}
			else if (c.state == State::New) {
				storeChange(c, false);
				rejected++;
			}
		}
		catch (const std::exception &e) {
			// not stored to qxchanges, so it is offered by the next import again
			errors << c.runnerName + " (" + dataTypeText(c.dataType) + "): " + QString::fromUtf8(e.what());
		}
	}
	qfInfo() << "O Checklist import, applied:" << applied << "rejected:" << rejected << "failed:" << errors.count();
	if (!errors.isEmpty()) {
		qf::gui::dialogs::MessageBox::showError(this, tr("Some changes failed, they are not stored and the next import offers them again.\n\n%1").arg(errors.join('\n')));
	}
	Super::accept();
}

void OChecklistImportDialog::applyChange(Change &change)
{
	auto *qxsql = qf::gui::framework::Application::instance()->qxSql();
	bool ok = true;
	switch (change.dataType) {
	case DataType::Status:
		ok = qxsql->updateRecord("runs", change.runId, QVariantMap{{"notStart", change.data == k_status_dns}}, this);
		break;
	case DataType::Card:
		ok = qxsql->updateRecord("runs", change.runId, QVariantMap{{"siId", change.data.toInt()}}, this);
		break;
	case DataType::Note:
		ok = getPlugin<Runs::RunsPlugin>()->appendCompetitorNote(change.competitorId, change.data, this);
		break;
	case DataType::CorridorTime:
		// false means the stored corridor time is earlier, it is not an error
		getPlugin<Runs::RunsPlugin>()->setRunCorridorTimeIfEarlier(change.runId, change.created, this);
		break;
	case DataType::NewRunner:
		createRunner(change);
		break;
	}
	if (!ok) {
		// the record was deleted meanwhile, the change is not stored and the next import offers it again
		QF_EXCEPTION(tr("Record not found, the change is not applied."));
	}
}

void OChecklistImportDialog::createRunner(Change &change)
{
	const QVariantMap &r = change.newRunner;
	Competitors::CompetitorDocument doc;
	doc.loadForInsert();
	doc.setValue("firstName", r.value("firstName"));
	doc.setValue("lastName", r.value("lastName"));
	doc.setValue("classId", r.value("classId"));
	doc.setValue("club", r.value("club"));
	doc.setValue("note", r.value("note"));
	doc.setSiid(r.value("siId"));
	doc.save();
	change.competitorId = doc.dataId().toInt();
	change.runId = doc.runsIds().value(m_stageId - 1);
	if (change.runId <= 0) {
		QF_EXCEPTION(tr("Run of the new runner not found, start data are not set."));
	}
	auto *runs_plugin = getPlugin<Runs::RunsPlugin>();
	if (r.value("notStart").toBool()) {
		qf::gui::framework::Application::instance()->qxSql()->updateRecord("runs", change.runId, QVariantMap{{"notStart", true}}, this);
	}
	if (const QString start_time = r.value("startTime").toString(); !start_time.isEmpty()) {
		runs_plugin->setRunStartTime(change.runId, start_time, this);
	}
	const QString corridor_time = r.value("corridorTime").toString();
	if (const QDateTime dt = QDateTime::fromString(corridor_time, Qt::ISODate); dt.isValid()) {
		runs_plugin->setRunCorridorTimeIfEarlier(change.runId, dt, this);
		// the next import matches the created runner, it must not offer the corridor time again
		Change corridor_change = change;
		corridor_change.dataType = DataType::CorridorTime;
		corridor_change.data = corridor_time;
		corridor_change.created = dt;
		storeChange(corridor_change, true);
	}
}

void OChecklistImportDialog::storeChange(const Change &change, bool accepted)
{
	// the same record as OFeed stores
	QVariantMap rec {
		{"data_type", dataTypeName(change.dataType)},
		{"data", change.data},
		{"foreign_table", "runs"},
		{"foreign_id", change.runId},
		{"orig_data", change.origData},
		{"source", k_source},
		{"user_id", change.competitorId},
		{"stage_id", m_stageId},
		{"created", change.created},
		{"status", accepted? "Accepted": "Rejected"},
		{"status_message", change.runnerName + ": " + change.origData + " -> " + change.data},
	};
	qf::gui::framework::Application::instance()->qxSql()->createRecord("qxchanges", rec, this);
}

QString OChecklistImportDialog::stateText(State state)
{
	switch (state) {
	case State::New: return tr("New");
	case State::Accepted: return tr("Applied already");
	case State::Rejected: return tr("Rejected already");
	case State::NoDifference: return tr("No difference");
	case State::NotFound: return tr("Runner not found");
	}
	return {};
}

QString OChecklistImportDialog::dataTypeName(DataType data_type)
{
	switch (data_type) {
	case DataType::Status: return QStringLiteral("status_change");
	case DataType::Card: return QStringLiteral("si_card_change");
	case DataType::Note: return QStringLiteral("note_change");
	case DataType::CorridorTime: return QStringLiteral("corridor_time_change");
	case DataType::NewRunner: return QStringLiteral("competitor_create");
	}
	return {};
}

QString OChecklistImportDialog::dataTypeText(DataType data_type)
{
	switch (data_type) {
	case DataType::Status: return tr("Status");
	case DataType::Card: return tr("Card");
	case DataType::Note: return tr("Note");
	case DataType::CorridorTime: return tr("Corridor time");
	case DataType::NewRunner: return tr("New runner");
	}
	return {};
}
