#include "styleobject.h"
#include "../reportitemreport.h"

#include <qf/core/log.h>
#include <qf/core/assert.h>

#include <QObject>

using namespace qf::gui::reports::style;

StyleObject::StyleObject(QObject *parent, StyleGroup style_group)
    : QObject(parent), m_styleGroup(style_group)
{
    m_dirty = true;
	//setName(nextSequentialName());
}

QString StyleObject::name()
{
    return objectName();
}

void StyleObject::setName(const QString &s)
{
	if(s != objectName()) {
		//QString old_name = objectName();
		setObjectName(s);
		//if(parent())
		//	updateStyleCache(old_name, s);
		emit nameChanged(s);
	}
}

qf::gui::reports::ReportItemReport *StyleObject::reportItem()
{
	ReportItemReport *ret = nullptr;
	QObject *it = this;
	//qfInfo() << "parents of" << it;
	while(it) {
		ret = qobject_cast<ReportItemReport*>(it);
		if(ret) {
			break;
		}
		it = it->QObject::parent();
		//qfInfo() << "\t" << it;
	}
	//if(ret == nullptr)
	//	qfError() << "FCK";
	QF_ASSERT(ret != nullptr, "Cannot find Report Root Item", return ret);
	return ret;
}

qf::gui::reports::ReportProcessor *StyleObject::reportProcessor()
{
	ReportProcessor *ret = nullptr;
	ReportItemReport *rir = reportItem();
	if(rir) {
		ret = rir->reportProcessor();
	}
	QF_ASSERT(ret != nullptr, "Report Style Item without ReportProcessor", return ret);
	return ret;
}

Sheet *StyleObject::rootStyleSheet()
{
	Sheet *ret = nullptr;
	ReportItemReport *rir = reportItem();
	if(rir)
		ret = rir->styleSheet();
    return ret;
}

QObject *StyleObject::styleobjectFromVariant(const QVariant &v, StyleGroup style_group, bool should_exist)
{
	QObject *ret = nullptr;
	if(v.isValid()) {
		ret = v.value<QObject*>();
		if(!ret) {
#if QT_VERSION_MAJOR >= 6
			if(v.typeId() == QMetaType::QString) {
#else
			if(v.type() == QVariant::String) {
#endif
				StyleGroup sg = style_group;
				if(sg == SGOwn)
					sg = m_styleGroup;
				ret = rootStyleSheet()->styleObjectForName(sg, v.toString(), should_exist);
			}
		}
	}
	return ret;
}
/*
QString StyleObject::nextSequentialName()
{
    static int n = 0;
	return QString("%1_%2").arg(m_styleGroup).arg(++n);
}
*/
/*
void StyleObject::updateStyleCache(const QString &old_name, const QString &new_name)
{
	qfLogFuncFrame() << "old_name:" << old_name << "new_name:" << new_name;
	Sheet *root_ss = rootStyleSheet();
	if(root_ss) {
		root_ss->setStyleObjectForName(m_styleGroup, old_name, nullptr);
		root_ss->setStyleObjectForName(m_styleGroup, new_name, this);
	}
	else {
		qfWarning() << Q_FUNC_INFO << "Cannot find root style sheet";
	}
}
*/
/*
bool StyleObject::event(QEvent *ev)
{
	bool ret = Super::event(ev);
	if(ev->type() == QEvent::ParentChange) {
		qfInfo() << "new parent:" << parent();
	}
	return ret;
}
*/
