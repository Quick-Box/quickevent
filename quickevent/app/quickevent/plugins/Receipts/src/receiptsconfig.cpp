#include "receiptsconfig.h"

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
