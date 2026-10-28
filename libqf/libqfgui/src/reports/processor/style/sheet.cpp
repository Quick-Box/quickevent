#include "sheet.h"

#include <qf/core/log.h>

using namespace qf::gui::reports::style;

//==========================================================
//           Sheet
//==========================================================
Sheet::Sheet(QObject *parent)
	: QObject(parent)
{
	qfLogFuncFrame();
}

Sheet::~Sheet()
{
	qfLogFuncFrame();
}

void Sheet::createStyleCache()
{
	m_definedStyles.clear();
	createStyleCache_helper(this);
}

void Sheet::createStyleCache_helper(QObject *parent)
{
	if(!parent)
		return;
	Q_FOREACH(auto child, parent->children()) {
		auto *so = dynamic_cast<StyleObject*>(child);
		if(so) {
			QString name = so->name();
			if(!name.isEmpty()) {
				setStyleObjectForName(so->styleGroup(), name, so);
			}
		}
		createStyleCache_helper(child);
	}
}

QObject *Sheet::styleObjectForName(StyleObject::StyleGroup style_object_group, const QString &name, bool should_exist)
{
	ObjectMap om = m_definedStyles.value(style_object_group);
	QObject *ret = om.value(name);
	if(ret == nullptr && should_exist)
		qfError() << "Cannot find style object for style group:" << style_object_group << "name:" + name;
	return ret;
}

void Sheet::setStyleObjectForName(StyleObject::StyleGroup style_object_group, const QString &name, QObject *o)
{
	//qfInfo() << Q_FUNC_INFO << name << style_object_group << o;
	ObjectMap &om = m_definedStyles[style_object_group];
	if(o)
		om[name] = o;
	else
		om.remove(name);
}
