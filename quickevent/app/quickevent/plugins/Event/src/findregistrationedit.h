#ifndef FINDREGISTRATIONEDIT_H
#define FINDREGISTRATIONEDIT_H

#include <QLineEdit>

class FindRegistrationsModel;

class FindRegistrationEdit : public QLineEdit
{
	Q_OBJECT
private:
	typedef QLineEdit Super;
public:
	static constexpr int CompletionRole = Qt::UserRole + 1;

	FindRegistrationEdit(QWidget *parent = nullptr);

	Q_SIGNAL void registrationSelected(const QVariantMap &registration_values);
protected:
	void focusInEvent(QFocusEvent * event) Q_DECL_OVERRIDE;
private:
	Q_SLOT void onCompleterActivated(const QModelIndex &index);
private:
	FindRegistrationsModel *m_findRegistrationsModel = nullptr;
};

#endif // FINDREGISTRATIONEDIT_H
