#pragma once

#include "tablemodel.h"

#include <QObject>
#include <QVariantMap>

namespace qf {
namespace gui {
namespace model {

class QFGUI_DECL_EXPORT DataDocument : public QObject
{
	Q_OBJECT
	Q_PROPERTY(qf::gui::model::DataDocument::RecordEditMode mode READ mode WRITE setMode NOTIFY modeChanged)
	Q_PROPERTY(qf::gui::model::TableModel* model READ model WRITE setModel NOTIFY modelChanged)
	Q_PROPERTY(QVariant dataId READ dataId WRITE setDataId NOTIFY dataIdChanged)
	Q_PROPERTY(QString idFieldName READ idFieldName WRITE setIdFieldName NOTIFY idFieldNameChanged)
public:
	explicit DataDocument(QObject *parent = nullptr);
	~DataDocument() Q_DECL_OVERRIDE;
public:
	enum RecordEditMode {ModeView, ModeEdit, ModeInsert, ModeCopy, ModeDelete};
	Q_ENUM(RecordEditMode)

	QF_PROPERTY_IMPL2(QString, i, I, dFieldName, QStringLiteral("id"))
	QF_PROPERTY_IMPL(QVariant, d, D, ataId)
	QF_PROPERTY_IMPL2(RecordEditMode, m, M, ode, ModeView)
	QF_PROPERTY_IMPL2(int, c, C, urrentModelRow, 0)
public:
	static QString recordEditModeToString(RecordEditMode m);
	void setModel(TableModel *m);
	virtual const TableModel* model() const;
	virtual TableModel* model();
	Q_SIGNAL void modelChanged(TableModel *new_m);

	Q_SIGNAL void loaded();
	Q_SIGNAL void aboutToSave(const QVariant &id, int mode);
	Q_SIGNAL void saved(const QVariant &id, int mode);
	Q_SIGNAL void aboutToDrop(const QVariant &id);
	Q_SIGNAL void dropped(const QVariant &id);
	Q_SIGNAL void valueChanged(const QString &data_id, const QVariant &old_val, const QVariant &new_val);

	Q_SLOT bool load(const QVariant &id, RecordEditMode mode = ModeEdit);
	Q_SLOT bool loadForInsert() {return load(QVariant(), ModeInsert);}
	//! this is a convinience function, @see load() .
	Q_SLOT virtual bool load();
	Q_SLOT virtual bool save();
	Q_SLOT virtual bool drop();
	Q_SLOT virtual bool copy();

	//virtual QMetaType::Type fieldType(const QString &data_id);
	Q_INVOKABLE virtual bool isEmpty() const;
	Q_INVOKABLE QStringList fieldNames() const;
	Q_INVOKABLE virtual bool isValidFieldName(const QString &data_id) const;
	Q_INVOKABLE virtual bool isDirty() const;
	Q_INVOKABLE virtual bool isDirty(const QString &data_id) const;
	Q_INVOKABLE virtual QVariant value(const QString &data_id) const;
	Q_INVOKABLE QVariant value(const QString &data_id, const QVariant &default_value) const
	{
		if(isValidFieldName(data_id) && !isEmpty())
			return value(data_id);
		return default_value;
	}
	Q_INVOKABLE virtual QVariant origValue(const QString &data_id) const;
	//Q_INVOKABLE QVariant origValue(const QString &data_id, const QVariant &default_value);
	Q_INVOKABLE virtual QVariantMap values() const;

	Q_SLOT virtual void setValue(const QString &data_id, const QVariant &val);
	//Q_SLOT void setValue_qml(const QVariant &data_id, const QVariant &val) {setValue(data_id.toString(), val);}
	struct QFGUI_DECL_EXPORT EditState
	{
		RecordEditMode editMode = ModeView;
		QMap<int, QVariant> dirtyValues;
	};
	EditState saveEditState() const;
	void restoreEditState(const EditState &edit_state);
protected:
	///! load model persistent storage via model
	Q_INVOKABLE virtual bool loadData();
	///! save dirty data to persistent storage via model
	Q_INVOKABLE virtual bool saveData();
	///! drop data in persistent storage via model
	Q_INVOKABLE virtual bool dropData();
	//! prepare loaded data to create document copy on next save() call
	Q_INVOKABLE virtual bool copyData();

	virtual TableModel* createModel(QObject *parent);
protected:
	TableModel *m_model = nullptr;
};

}}}

