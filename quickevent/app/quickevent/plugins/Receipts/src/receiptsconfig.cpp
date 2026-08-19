#include "receiptsconfig.h"

#include <QBuffer>
#include <QImageReader>
#include <QVariantMap>

namespace Receipts {

ReceiptsConfig::ReceiptsConfig()
{
	qrCodeCaption = "Live Results";
	imageFormat = "png";
}

ReceiptsConfig ReceiptsConfig::fromVariantMap(const QVariantMap &map)
{
	ReceiptsConfig config;
	config.printQrCode = map.value("printQrCode", config.printQrCode).toBool();
	config.linkUrl = map.value("linkUrl", config.linkUrl).toString();
	config.qrCodeCaption = map.value("qrCodeCaption", config.qrCodeCaption).toString();
	config.printImage = map.value("printImage", config.printImage).toBool();
	config.imageHeightMm = map.value("imageHeightMm", config.imageHeightMm).toInt();
	config.imageBase64 = map.value("imageBase64", config.imageBase64).toString();
	config.imageFormat = map.value("imageFormat", config.imageFormat).toString();
	return config;
}

int ReceiptsConfig::imageHeightMmForImage(const QByteArray &image_data, int fallback_mm)
{
	// receipt column is 65 mm wide (210/3-5) minus 4 mm frame insets on both sides,
	// the report scales the image KeepAspectRatio into (usable width, height)
	static constexpr double USABLE_RECEIPT_WIDTH = 55;
	QBuffer buffer;
	buffer.setData(image_data);
	buffer.open(QIODevice::ReadOnly);
	const QSize image_size = QImageReader(&buffer).size();
	if(image_size.width() <= 0 || image_size.height() <= 0) {
		return fallback_mm;
	}
	return qBound(10, qRound(USABLE_RECEIPT_WIDTH * image_size.height() / image_size.width()), 60);
}

QVariantMap ReceiptsConfig::toVariantMap() const
{
	QVariantMap ret;
	ret["printQrCode"] = printQrCode;
	ret["linkUrl"] = linkUrl;
	ret["qrCodeCaption"] = qrCodeCaption;
	ret["printImage"] = printImage;
	ret["imageHeightMm"] = imageHeightMm;
	ret["imageBase64"] = imageBase64;
	ret["imageFormat"] = imageFormat;
	return ret;
}

}
