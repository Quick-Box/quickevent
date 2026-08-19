#pragma once

#include <QString>

namespace Receipts {

struct ReceiptsConfig
{
	ReceiptsConfig();
	static ReceiptsConfig fromVariantMap(const QVariantMap &map);
	QVariantMap toVariantMap() const;
	/// height at which the image fills the receipt width, derived from its aspect ratio
	static int imageHeightMmForImage(const QByteArray &image_data, int fallback_mm);

	bool printQrCode = false;
	QString linkUrl;
	QString qrCodeCaption;   // Default "Live Results" applied by AppDbConfig::receiptsConfig()
	bool printImage = false;
	int imageHeightMm = 18;  // Clamped to [10, 60]
	QString imageBase64;
	QString imageFormat;     // Default "png" applied by AppDbConfig::receiptsConfig()
};

}
