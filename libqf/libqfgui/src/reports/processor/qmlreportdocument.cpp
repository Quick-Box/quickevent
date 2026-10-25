#include "qmlreportdocument.h"
#include "reportitemreport.h"
#include "reportprocessor.h"
#include "../../framework/plugin.h"
#include "../../framework/reportfilecache.h"

#include <qf/core/log.h>

#include <QCoreApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>

//#define QF_TIMESCOPE_ENABLED
#include <qf/core/utils/timescope.h>

using namespace qf::gui::reports;

namespace {
QList<QmlReportDocument::QmlEngineInitializer> s_engineInitializers;
}

void QmlReportDocument::addQmlEngineInitializer(const QmlEngineInitializer &fn)
{
	s_engineInitializers << fn;
}

QStringList QmlReportDocument::qmlEngineImportPaths()
{
	QStringList lst;
#ifdef Q_OS_UNIX
	lst << QCoreApplication::applicationDirPath() + "/../lib/qml";
#endif
	lst << QCoreApplication::applicationDirPath() + "/qml";
	if(const auto reports_dir = framework::Plugin::reportFileCache()->localReportsDir(); !reports_dir.isEmpty())
		lst << reports_dir;
	lst << ":/quickevent";
	return lst;
}

QmlReportDocument::QmlReportDocument(ReportProcessor *processor)
{
	qfMessage() << "Creating report QML engine";
	m_qmlEngine = new QQmlEngine();
	m_qmlEngine->rootContext()->setContextProperty("reportProcessor", processor);
	m_qmlEngine->rootContext()->setContextProperty("application", QCoreApplication::instance());
	for(const auto &path : qmlEngineImportPaths()) {
		qfMessage() << "Adding report QML engine import path:" << path;
		m_qmlEngine->addImportPath(path);
	}
	for (const auto &init : s_engineInitializers)
		init(m_qmlEngine);
	m_component = new QQmlComponent(m_qmlEngine, m_qmlEngine);
}

QmlReportDocument::~QmlReportDocument()
{
	delete m_qmlEngine;
}

bool QmlReportDocument::load(const QString &file_name)
{
	qfLogFuncFrame() << file_name;
	QString fn = file_name;
	if(fn.startsWith(QLatin1String(":/")))
		fn = QLatin1String("qrc") + fn;
	QUrl url = (fn.startsWith(QLatin1String("qrc:/")))? QUrl(fn): QUrl::fromLocalFile(fn);
	qfDebug() << "loading url:" << url.toString();
	m_component->loadUrl(url);
	if(m_component->isError()) {
		qfError() << "Erorr loading report component:" << file_name << m_component->errorString();
		return false;
	}
	if(!m_component->isReady()) {
		qfError() << "QML component" << url.toString() << "cannot be loaded asynchronously";
		return false;
	}
	return true;
}

QUrl QmlReportDocument::url() const
{
	return m_component->url();
}

ReportItemReport *QmlReportDocument::createReport(const QVariantMap &init_properties)
{
	QObject *o;
	ReportItemReport *root;
	{
		QF_TIME_SCOPE("QmlReportDocument::createReport() - creating report object begin");
		o = m_component->beginCreate(m_qmlEngine->rootContext());
		root = qobject_cast<ReportItemReport*>(o);
	}
	if(!root) {
		qfError() << "Error creating root object from component:" << m_component << m_component->url().toString();
		qfError() << "Created object:" << o;
		for(const auto &err : m_component->errors())
			qfError() << err.toString();
		delete o;
		return nullptr;
	}
	QF_TIME_SCOPE("QmlReportDocument::createReport() - creating report object finish");
	for(const auto &[key, val] : init_properties.asKeyValueRange()) {
		QByteArray ba = key.toLatin1();
		if(!root->setProperty(ba.constData(), val)) {
			qfWarning() << "Cannot set report root property" << key << ", root element should have this property defined explicitly.";
		}
	}
	m_component->completeCreate();
	return root;
}
