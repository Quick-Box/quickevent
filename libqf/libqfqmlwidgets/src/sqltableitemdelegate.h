#ifndef QF_QMLWIDGETS_SQLTABLEITEMDELEGATE_H
#define QF_QMLWIDGETS_SQLTABLEITEMDELEGATE_H

#include "tableitemdelegate.h"

namespace qf {
namespace qmlwidgets {

class QFQMLWIDGETS_DECL_EXPORT SqlTableItemDelegate : public TableItemDelegate
{
	Q_OBJECT
private:
	typedef TableItemDelegate Super;
public:
	SqlTableItemDelegate(TableView *parent = nullptr);
public:
	//QString displayText(const QVariant &value, const QLocale &locale) const Q_DECL_OVERRIDE;
	//void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const Q_DECL_OVERRIDE;
	//QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const Q_DECL_OVERRIDE;
	QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const Q_DECL_OVERRIDE;
	void setEditorData(QWidget *editor, const QModelIndex &index) const Q_DECL_OVERRIDE;
	void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const Q_DECL_OVERRIDE;
protected:
	//Q_SLOT void commitAndCloseEditor();
};

} // namespace qmlwidgets
} // namespace qf

#endif // QF_QMLWIDGETS_SQLTABLEITEMDELEGATE_H
