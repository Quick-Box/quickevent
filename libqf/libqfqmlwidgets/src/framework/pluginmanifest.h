#ifndef QF_QMLWIDGETS_FRAMEWORK_PLUGINMANIFEST_H
#define QF_QMLWIDGETS_FRAMEWORK_PLUGINMANIFEST_H

#include "../qmlwidgetsglobal.h"

#include <qf/core/utils.h>

#include <QObject>
#include <QStringList>

namespace qf {
namespace qmlwidgets {
namespace framework {

class QFQMLWIDGETS_DECL_EXPORT PluginManifest : public QObject
{
	Q_OBJECT
	Q_PROPERTY(QString featureId READ featureId WRITE setFeatureId NOTIFY featureIdChanged FINAL)
	Q_PROPERTY(bool disabled READ isDisabled WRITE setDisabled NOTIFY disabledChanged)
	Q_PROPERTY(QStringList dependsOnFeatureIds READ dependsOnFeatureIds WRITE setDependsOnFeatureIds FINAL)
	Q_CLASSINFO("property.addFromBottom.doc", "Part is added at bottom when added to part switch.")
	Q_PROPERTY(bool addFromBottom READ isAddFromBottom WRITE setAddFromBottom FINAL)
	Q_PROPERTY(QString homeDir READ homeDir FINAL)
public:
	explicit PluginManifest(QObject *parent = 0);
	~PluginManifest() Q_DECL_OVERRIDE;
public:
	QF_PROPERTY_BOOL_IMPL(d, D, isabled)
	QF_PROPERTY_BOOL_IMPL(a, A, ddFromBottom)
	QF_PROPERTY_IMPL(QString, h, H, omeDir)

	QString featureId() const { return m_featureId; }
	void setFeatureId(QString id);
	Q_SIGNAL void featureIdChanged(const QString &new_feature_id);

	QStringList dependsOnFeatureIds() const { return m_dependsOnFeatureIds; }
	void setDependsOnFeatureIds(QStringList ids);

private:
	QString m_featureId;
	QStringList m_dependsOnFeatureIds;
};

}}}

#endif // QF_QMLWIDGETS_FRAMEWORK_PLUGINMANIFEST_H
