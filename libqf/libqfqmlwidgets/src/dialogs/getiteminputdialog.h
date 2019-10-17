#ifndef QF_QMLWIDGETS_DIALOGS_GETITEMINPUTDIALOG_H
#define QF_QMLWIDGETS_DIALOGS_GETITEMINPUTDIALOG_H

#include "../qmlwidgetsglobal.h"

#include <QDialog>
#include <QVariant>

class QLabel;
class QComboBox;

namespace qf {
namespace qmlwidgets {
namespace dialogs {

class QFQMLWIDGETS_DECL_EXPORT GetItemInputDialog : public QDialog
{
	Q_OBJECT
public:
	explicit GetItemInputDialog(QWidget *parent = nullptr);
	~GetItemInputDialog() Q_DECL_OVERRIDE;
public:
	void setLabelText(const QString &text);
	void setItems(const QStringList &items, const QVariantList &data = QVariantList());
	int currentItemIndex();

	QComboBox *comboBox() {return m_comboBox;}
	QString currentText();
	QVariant currentData();
	void setCurrentItemIndex(int ix);

	static int getItem(QWidget *parent, const QString &title, const QString &label_text, const QStringList &items, int current_item_index = -1);
private:
	QLabel *m_label;
	QComboBox *m_comboBox;
};

}}}

#endif // QF_QMLWIDGETS_DIALOGS_GETITEMINPUTDIALOG_H
