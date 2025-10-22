#include "sqltablemodel.h"

#include <quickevent/core/si/siid.h>
#include <quickevent/core/og/timems.h>

#include <qf/core/log.h>

#include <QMetaType>



namespace quickevent::gui::og {

SqlTableModel::SqlTableModel(QObject *parent)
	: Super(parent)
{
}

SqlTableModel::~SqlTableModel()
= default;

QVariant SqlTableModel::data(const QModelIndex &index, int role) const
{
	if(role == Qt::DisplayRole) {
		QVariant v = Super::data(index, Qt::EditRole);
		int type = v.userType();
		if(type == qMetaTypeId<core::og::TimeMs>()) {
			auto t = v.value<core::og::TimeMs>();
			return t.toString();
		}
		if(type == qMetaTypeId<core::si::SiId>()) {
			int id = (int)v.value<core::si::SiId>();
			if(id == 0)
				return QString();// QStringLiteral("null");
			return id;
		}
	}
	else if(role == SortRole) {
		QVariant v = Super::data(index, Qt::EditRole);
		int type = v.userType();
		if(type == qMetaTypeId<core::og::TimeMs>()) {
			auto t = v.value<core::og::TimeMs>();
			return t.msec();
		}
		return Super::data(index, role);
	}
	else if(role == Qt::TextAlignmentRole) {
		QVariant v = Super::data(index, Qt::EditRole);
		if(v.userType() == qMetaTypeId<core::og::TimeMs>()) {
			return Qt::AlignRight;
		}
	}
	return Super::data(index, role);
}

QVariant SqlTableModel::rawValueToEdit(int column_index, const QVariant &val) const
{
	QVariant ret = val;
	int type = columnType(column_index);
	if(type == qMetaTypeId<core::og::TimeMs>()) {
		core::og::TimeMs t;
		if(!ret.isNull()) {
			t = core::og::TimeMs(ret.toInt());
		}
		ret = QVariant::fromValue(t);
	}
	else if(type == qMetaTypeId<core::si::SiId>()) {
		core::si::SiId id(val.toInt());
		ret = QVariant::fromValue(id);
	}
	return ret;
}

QVariant SqlTableModel::editValueToRaw(int column_index, const QVariant &val) const
{
	QVariant ret = val;
	int type = columnType(column_index);
	if(type == qMetaTypeId<core::og::TimeMs>()) {
		auto t = val.value<core::og::TimeMs>();
#if QT_VERSION_MAJOR >= 6
		ret = t.isValid()? t.msec(): QVariant(QMetaType(QMetaType::Int));
#else
		ret = t.isValid()? t.msec(): QVariant(QVariant::Int);
#endif
	}
	else if(type == qMetaTypeId<core::si::SiId>()) {
		int siid = 0;
		if (val.userType() == qMetaTypeId<QString>()) {
			siid = val.toString().toInt();
		}
		else if (val.userType() == qMetaTypeId<core::si::SiId>()) {
			siid = (int)val.value<core::si::SiId>();
		}
		else {
			siid = val.toInt();
		}
		if(siid == 0)
#if QT_VERSION_MAJOR >= 6
			ret = QVariant(QMetaType(QMetaType::Int));
#else
			ret = QVariant(QVariant::Int);
#endif
		else
			ret = siid;
	}
	//qfInfo() << val << ret;
	return ret;
}

}
