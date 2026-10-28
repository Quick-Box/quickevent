#pragma once

#include <plugins/Event/src/services/service.h>

class QTimer;

namespace Runs {
namespace services {

class ResultsExporterSettings : public Event::services::ServiceSettings
{
	using Super = Event::services::ServiceSettings;

public:
	enum class OutputFormat {HtmlMulti = 0, CSOS, CSV, CSVMulti, IofXml3, COUNT};

	QF_VARIANTMAP_FIELD(QString, e, setE, xportDir)
	QF_VARIANTMAP_FIELD2(int, e, setE, xportIntervalSec, 0)
	QF_VARIANTMAP_FIELD(QString, w, setW, henFinishedRunCmd)
	QF_VARIANTMAP_FIELD2(int, o, setO, utputFormat, static_cast<int>(OutputFormat::HtmlMulti))
	QF_VARIANTMAP_FIELD2(bool, e, setE, xportMultiFile, true)
	QF_VARIANTMAP_FIELD2(QChar, c, setC, svSeparator, ';')
	QF_VARIANTMAP_FIELD2(bool, e, setE, xportCsvWithDNS, false)
public:
	ResultsExporterSettings(const QVariantMap &o = QVariantMap()) : Super(o) {}
};

class ResultsExporter : public Event::services::Service
{
	Q_OBJECT

	using Super = Event::services::Service;
public:
	ResultsExporter(QObject *parent);

	void run() override;
	void stop() override;
	ResultsExporterSettings settings() const {return ResultsExporterSettings(m_settings);}

	qf::gui::framework::DialogWidget *createDetailWidget() override;

	static QString serviceName();
	QString serviceDescription() const override;

	bool exportResults() const;
	void whenFinishedRunCmd() const;
private:
	void loadSettings() override;
	void onExportTimerTimeOut();
	void init();
private:
	QTimer *m_exportTimer = nullptr;
};

}}

