#pragma once

#include "service.h"

class QTimer;

namespace Event {
namespace services {

class EmmaClientSettings : public ServiceSettings
{
	using Super = ServiceSettings;
public:
	enum StartExportType {
		TXT = 0,
		CSV
	};
private:
	QF_VARIANTMAP_FIELD2(bool, e, setE, xportStartListTypeXml3, 0)
	QF_VARIANTMAP_FIELD2(bool, e, setE, xportResultTypeXml3, 0)
	QF_VARIANTMAP_FIELD(QString, e, setE, xportDir)
	QF_VARIANTMAP_FIELD(QString, f, setF, ileNameBase)
	QF_VARIANTMAP_FIELD2(int, e, setE, xportIntervalSec, 0)
	QF_VARIANTMAP_FIELD2(bool, e, setE, xportStartTypeTxt, 0)
	QF_VARIANTMAP_FIELD2(bool, e, setE, xportFinishTypeTxt, 0)
	QF_VARIANTMAP_FIELD2(int, s, setS, tartExportType, 0)
public:
	EmmaClientSettings(const QVariantMap &o = QVariantMap()) : Super(o) {}
};

class EmmaClient : public Service
{
	Q_OBJECT

	using Super = Service;
public:
	EmmaClient(QObject *parent);

	void run() override;
	void stop() override;
	EmmaClientSettings settings() const {return EmmaClientSettings(m_settings);}

	static QString serviceName();
	QString serviceDescription() const override;

	void exportRadioCodesRacomTxt();
	void exportResultsIofXml3();
	void exportStartListIofXml3();
	void exportFinishRacomTxt() const;
	void exportStartListRacomTxt() const;
	void exportStartListRacomCsv() const;
	bool preExport() const;
private:
	void loadSettings() override;
	bool createExportDir() const;
	void onDbEventNotify(const QString &domain, int connection_id, const QVariant &data);
	void onCardChecked(const QVariantMap &data);
	qf::gui::framework::DialogWidget *createDetailWidget() override;
private:
	void onExportTimerTimeOut();
	void init();
private:
	QTimer *m_exportTimer = nullptr;
};

}}
