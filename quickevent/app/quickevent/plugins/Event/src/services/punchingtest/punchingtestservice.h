#pragma once

#include "../service.h"

class QTimer;

namespace Event {
namespace services {

class PunchingTestServiceSettings : public ServiceSettings
{
	using Super = ServiceSettings;

	QF_VARIANTMAP_FIELD2(int, p, setP, unchInterval, 10) // seconds between ticks

	// Per-card imperfections: probability = 1/N
	QF_VARIANTMAP_FIELD2(int, u, setU, nknownCardRate, 100) // wrong SI number
	QF_VARIANTMAP_FIELD2(int, m, setM, issingStartRate, 80) // no start punch
	QF_VARIANTMAP_FIELD2(int, m, setM, issingFinishRate, 250) // no finish punch
	QF_VARIANTMAP_FIELD2(int, e, setE, xtraPunchRate, 30) // extra wrong control
	QF_VARIANTMAP_FIELD2(int, b, setB, adCheckRate, 1800) // check outside window

	// Per-control imperfection: probability = 1/N
	QF_VARIANTMAP_FIELD2(int, m, setM, ispunchRate, 930) // missed control

public:
	PunchingTestServiceSettings(const QVariantMap &o = QVariantMap()) : Super(o) {}
};

class PunchingTestService : public Service
{
	Q_OBJECT
	using Super = Service;
public:
	explicit PunchingTestService(QObject *parent);

	void run() override;
	void stop() override;

	PunchingTestServiceSettings settings() const { return PunchingTestServiceSettings(m_settings); }
	static QString serviceName();
	QString serviceDisplayName() const override;

private:
	void onTimerTick();
	qf::gui::framework::DialogWidget *createDetailWidget() override;

private:
	QTimer *m_timer = nullptr;
};

}} // namespace Event::services
