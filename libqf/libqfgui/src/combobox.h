#ifndef QF_GUI_COMBOBOX_H
#define QF_GUI_COMBOBOX_H

#include "guiglobal.h"
#include "idatawidget.h"

#include <qf/core/utils.h>

#include <QComboBox>

namespace qf {
namespace gui {

class QFGUI_DECL_EXPORT ComboBox : public QComboBox, public IDataWidget
{
	Q_OBJECT
	Q_PROPERTY(QVariant dataValue READ dataValue WRITE setDataValue NOTIFY dataValueChanged)
	Q_PROPERTY(QString dataId READ dataId WRITE setDataId)
	Q_PROPERTY(bool valueRestrictedToItems READ isValueRestrictedToItems WRITE setValueRestrictedToItems NOTIFY valueRestrictedToItemsChanged)
	Q_PROPERTY(QVariant currentData READ currentData WRITE setCurrentData NOTIFY currentDataChanged)
private:
	typedef QComboBox Super;
public:
	ComboBox(QWidget *parent = nullptr);
	~ComboBox() Q_DECL_OVERRIDE;

	QF_PROPERTY_BOOL_IMPL2(v, V , alueRestrictedToItems, true)

	QVariant currentData() const {return Super::currentData();}
	void setCurrentData(const QVariant &val);
	Q_SIGNAL void currentDataChanged(const QVariant &value);
	Q_SIGNAL void currentDataActivated(const QVariant &value);
public:
	Q_INVOKABLE void insertItem(int index, const QString &text, const QVariant &user_data = QVariant());
	Q_INVOKABLE void setItems(const QVariantList &items);
	Q_INVOKABLE virtual void removeItems();
	Q_INVOKABLE virtual void loadItems(bool force = false);

	// Show popup items in several shorter columns instead of one long list.
	void setMultiColumnPopup(bool b);
	void showPopup() Q_DECL_OVERRIDE;
protected:
	QVariant dataValue() Q_DECL_OVERRIDE;
	void setDataValue(const QVariant &value) Q_DECL_OVERRIDE;
	Q_SIGNAL void dataValueChanged(const QVariant &value);

	Q_SLOT virtual void onCurrentTextChanged(const QString &txt);
	bool eventFilter(QObject *watched, QEvent *event) Q_DECL_OVERRIDE;
private:
	void updateMultiColumnPopupGeometry();
	Q_SLOT void currentDataChanged_helper(int ix);
	Q_SLOT void currentDataActivated_helper(int ix);
protected:
	bool m_ignoreIndexChangedSignals = false;
	bool m_multiColumnPopup = false;
};

class QFGUI_DECL_EXPORT ForeignKeyComboBox : public ComboBox
{
	Q_OBJECT
	Q_PROPERTY(QString referencedTable READ referencedTable WRITE setReferencedTable NOTIFY referencedTableChanged)
	Q_PROPERTY(QString referencedField READ referencedField WRITE setReferencedField NOTIFY referencedFieldChanged)
	Q_PROPERTY(QString referencedCaptionField READ referencedCaptionField WRITE setReferencedCaptionField NOTIFY referencedCaptionFieldChanged)
	Q_PROPERTY(QString itemCaptionFormat READ itemCaptionFormat WRITE setItemCaptionFormat NOTIFY itemCaptionFormatChanged)
	Q_PROPERTY(QString queryString READ queryString WRITE setQueryString NOTIFY queryStringChanged)
private:
	typedef ComboBox Super;
public:
	ForeignKeyComboBox(QWidget *parent = nullptr);

	QF_PROPERTY_IMPL(QString, r, R, eferencedTable)
	QF_PROPERTY_IMPL(QString, r, R, eferencedField)
	QF_PROPERTY_IMPL(QString, r, R, eferencedCaptionField)
	QF_PROPERTY_IMPL2(QString, i, I, temCaptionFormat, QStringLiteral("{{captionField}}"))
	QF_PROPERTY_IMPL(QString, q, Q, ueryString)
public:
	void removeItems() Q_DECL_OVERRIDE;
	void loadItems(bool force = false) Q_DECL_OVERRIDE;
private:
	bool m_itemsLoaded = false;
};

class QFGUI_DECL_EXPORT DbEnumComboBox : public ComboBox
{
	Q_OBJECT
	Q_PROPERTY(QString groupName READ groupName WRITE setGroupName NOTIFY groupNameChanged)
	Q_PROPERTY(QString itemCaptionFormat READ itemCaptionFormat WRITE setItemCaptionFormat NOTIFY itemCaptionFormatChanged)
private:
	typedef ComboBox Super;
public:
	DbEnumComboBox(QWidget *parent = nullptr);

	QF_PROPERTY_IMPL(QString, g, G, roupName)
	QF_PROPERTY_IMPL2(QString, i, I, temCaptionFormat, QStringLiteral("{{caption}}"))
public:
	void removeItems() Q_DECL_OVERRIDE;
	void loadItems(bool force = false) Q_DECL_OVERRIDE;
private:
	bool m_itemsLoaded = false;
};

}}

#endif // COMBOBOX_H
