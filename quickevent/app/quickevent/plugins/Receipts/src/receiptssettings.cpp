#include "receiptssettings.h"

QString ReceiptsSettings::receiptReportPath() const
{
	QString path = receiptPath();
	for(const auto *name : {"receipts/Default", "receipts/Classic", "receipts/ClassicLottery"}) {
		if(path == QLatin1String(name) + QLatin1String(".qml"))
			return QLatin1String(name) + QLatin1String(".xml");
	}
	return path;
}

ReceiptsSettings::WhenRunnerNotFoundPrint ReceiptsSettings::whenRunnerNotFoundPrintEnum() const
{
	if(whenRunnerNotFoundPrint() == "ReceiptWithoutName")
		return WhenRunnerNotFoundPrint::ReceiptWithoutName;
	if(whenRunnerNotFoundPrint() == "ErrorInfoLong")
		return WhenRunnerNotFoundPrint::ErrorInfoLong;
	return WhenRunnerNotFoundPrint::ErrorInfo;
}

namespace {
const auto GRAPHIC_PRINTER = QStringLiteral("GraphicPrinter");
const auto CHARACTER_PRINTER = QStringLiteral("CharacterPrinter");
const auto INVALID_PRINTER_TYPE = QStringLiteral("InvalidPrinterType");
}

QString ReceiptsSettings::printerTypeToString(ReceiptsSettings::PrinterType e)
{
	switch(e) {
	case ReceiptsSettings::PrinterType::GraphicPrinter:
		return GRAPHIC_PRINTER;
	case ReceiptsSettings::PrinterType::CharacterPrinter:
		return CHARACTER_PRINTER;
	case PrinterType::Invalid:
		return INVALID_PRINTER_TYPE;
	}
	return INVALID_PRINTER_TYPE;
}

ReceiptsSettings::PrinterType ReceiptsSettings::printerTypeFromString(const QString &str)
{
	if(str == GRAPHIC_PRINTER)
		return PrinterType::GraphicPrinter;
	if(str == CHARACTER_PRINTER)
		return PrinterType::CharacterPrinter;
	return PrinterType::Invalid;
}

QString ReceiptsSettings::characterPrinterTypeToString(ReceiptsSettings::CharacterPrinteType e)
{
	switch(e) {
	case ReceiptsSettings::CharacterPrinteType::LPT:
		return QStringLiteral("LPT");
	case ReceiptsSettings::CharacterPrinteType::Directory:
		return QStringLiteral("Directory");
	case ReceiptsSettings::CharacterPrinteType::Network:
		return QStringLiteral("Network");
	}
	return {};
}

ReceiptsSettings::PrinterType ReceiptsSettings::printerTypeEnum() const
{
	return printerTypeFromString(printerType());
}

ReceiptsSettings::CharacterPrinteType ReceiptsSettings::characterPrinterTypeEnum() const
{
	const auto pt = characterPrinterType();
	if(pt == "Directory")
		return CharacterPrinteType::Directory;
	if(pt == "Network")
		return CharacterPrinteType::Network;
	return CharacterPrinteType::LPT;
}

QString ReceiptsSettings::printerCaption() const
{
	QString ret;
	if(printerTypeEnum() == PrinterType::GraphicPrinter) {
		ret = tr("Graphics") + ' ' + graphicsPrinterName();
	}
	else if(printerTypeEnum() == PrinterType::CharacterPrinter) {
		ret = tr("Character") + ' ' + characterPrinterModel() + ' ';
		switch(characterPrinterTypeEnum()) {
			case CharacterPrinteType::LPT:
				ret +=  characterPrinterDevice();
				break;
			case CharacterPrinteType::Directory:
				ret += characterPrinterDirectory();
				break;
			case CharacterPrinteType::Network:
				ret += characterPrinterUrl();
				break;
		}
	}
	return ret;
}

