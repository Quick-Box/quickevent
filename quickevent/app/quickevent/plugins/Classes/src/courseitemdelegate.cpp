#include "courseitemdelegate.h"

#include <qf/core/assert.h>

#include <QComboBox>
#include <QCompleter>

CourseItemDelegate::CourseItemDelegate(QObject *parent)
	: QStyledItemDelegate(parent)
{}

QWidget *CourseItemDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	Q_UNUSED(option)
	Q_UNUSED(index)
	auto *editor = new QComboBox(parent);
	initCombo(editor, m_idToCourseName, textImplicit());
	return editor;
}

void CourseItemDelegate::initCombo(QComboBox *combo, const QMap<int, QString> &courses, const QString &null_text)
{
	combo->setInsertPolicy(QComboBox::NoInsert);
	combo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
	combo->setMaxVisibleItems(20);
	combo->setEditable(true);
	QMap<QString, int> name_to_id;
	if (!null_text.isEmpty()) {
		combo->addItem(null_text, {});
	}
	for (const auto &[id, name] : courses.asKeyValueRange()) {
		name_to_id[name] = id;
	}
	for (const auto &[name, id] : name_to_id.asKeyValueRange()) {
		combo->addItem(name, id);
	}
	// Enable filtering
	auto items = name_to_id.keys();
	if (!null_text.isEmpty()) {
		items.insert(0, null_text);
	}
	auto *completer = new QCompleter(items, combo);
	completer->setCaseSensitivity(Qt::CaseInsensitive);
	completer->setFilterMode(Qt::MatchContains);
	combo->setCompleter(completer);
}

void CourseItemDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
	auto *cbx = qobject_cast<QComboBox *>(editor);
	QF_ASSERT(cbx != nullptr, "Bad combo!", return);
	QString id = index.data(Qt::EditRole).toString();
	int ix = cbx->findData(id);
	cbx->setCurrentIndex(ix);
}
void CourseItemDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
	qfLogFuncFrame();
	auto *cbx = qobject_cast<QComboBox *>(editor);
	QF_ASSERT(cbx != nullptr, "Bad combo!", return);
	qfDebug() << "setting model data:" << cbx->currentText() << cbx->currentData();
	model->setData(index, cbx->currentData(), Qt::EditRole);
	emit const_cast<CourseItemDelegate*>(this)->courseIdChanged(); // NOLINT(cppcoreguidelines-pro-type-const-cast)
}

QString CourseItemDelegate::displayText(const QVariant &value, const QLocale &locale) const
{
	Q_UNUSED(locale)
	return m_idToCourseName.value(value.toInt(), textImplicit());
}

void CourseItemDelegate::setCourses(const QMap<int, QString> &courses)
{
	m_idToCourseName = courses;
}

QString CourseItemDelegate::textImplicit()
{
	return tr("Implicit");
}

