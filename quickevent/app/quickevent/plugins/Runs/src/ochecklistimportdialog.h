#pragma once

#include <QDateTime>
#include <QDialog>
#include <QList>
#include <QVariantMap>

namespace qf::gui {
class TableView;
namespace model { class TableModel; }
}

/// Import of the start status report (YAML) from the O Checklist app, offline variant of the OFeed start changes
class OChecklistImportDialog : public QDialog
{
	Q_OBJECT
private:
	using Super = QDialog;
public:
	struct Report
	{
		QVariantMap meta; // Version, Creator, Created, Event
		QList<QVariantMap> runners; // {"Runner": {...}, "ChangeLog": {...}}
		QVariantMap endOfData;
		int dataLineCount = 0; // lines without comments and blank lines
	};
	/// parser of the O Checklist YAML report, it does not support YAML in general
	static Report parseReport(const QString &text);

	OChecklistImportDialog(const Report &report, int stage_id, QWidget *parent = nullptr);

	void accept() override;
private:
	enum class State { New, Accepted, Rejected, NoDifference, NotFound };
	struct Change
	{
		int runId = 0;
		int competitorId = 0;
		QString runnerName;
		QString className;
		QString card;
		QString dataType;
		QString origData;
		QString data;
		QDateTime created;
		QString match;
		State state = State::New;
		bool nameMismatch = false;
		bool apply = false;

		bool isCheckable() const { return state != State::NotFound; }
	};

	void loadChanges(const Report &report);
	void reloadTable();
	/// stores the Apply check boxes of the table rows to the changes
	void saveApplyFlags();
	const Change& changeForRow(int model_row) const;
	void applyChange(const Change &change);
	void storeChange(const Change &change, bool accepted);
	static QString stateText(State state);
	static QString dataTypeText(const QString &data_type);
private:
	int m_stageId = 0;
	QList<Change> m_changes;
	bool m_onlyNew = true;
	qf::gui::TableView *m_tableView = nullptr;
	qf::gui::model::TableModel *m_model = nullptr;
};
