#ifndef QF_GUI_FRAMEWORK_PLUGIN_H
#define QF_GUI_FRAMEWORK_PLUGIN_H

#include "../guiglobal.h"

#include <qf/core/utils.h>

#include <QObject>

class QQmlEngine;

namespace qf {
namespace gui {
namespace framework {

class ReportFileCache;

class QFGUI_DECL_EXPORT Plugin : public QObject
{
	Q_OBJECT
public:
	explicit Plugin(const QString &feature_id, QObject *parent = nullptr);
	explicit Plugin(QObject *parent = nullptr);
	~Plugin() Q_DECL_OVERRIDE;

	static QString pluginDataDir();
	static ReportFileCache *reportFileCache();



	QString homeDir() const { return pluginDataDir() + '/' +  featureId(); }
	QString qmlDir() const { return homeDir() + "/qml"; }
	//QString qmlReportsDir() const { return qmlDir() + "/reports"; }
	QString featureId() const { return m_featureId; }
	//QString settingsDir() const { return "plugin/" +  featureId(); }
	QString findReportFile(const QString &report_file_path) const;
	struct QFGUI_DECL_EXPORT ReportFileInfo
	{
		QString reportName;
		QString reportFilePath;

		bool operator==(const ReportFileInfo &o) const {
			return reportName == o.reportName && reportFilePath == o.reportFilePath;
		}
	};
	QList<ReportFileInfo> listReportFiles(const QString &report_dir,
		const QString &suffix = QStringLiteral("qml")) const;

	Q_SIGNAL void installed();
private:
	QString m_featureId;

};

}}}

#endif
