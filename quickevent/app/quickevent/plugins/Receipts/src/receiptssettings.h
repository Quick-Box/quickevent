#pragma once

#include <plugins/Core/src/settings.h>
#include <QCoreApplication>

class ReceiptsSettings : public Core::Settings
{
	Q_DECLARE_TR_FUNCTIONS(ReceiptsSettings)
public:
	enum class WhenRunnerNotFoundPrint { ErrorInfo, ErrorInfoLong, ReceiptWithoutName, };
	enum class PrinterType { Invalid, GraphicPrinter,  CharacterPrinter};
	enum class CharacterPrinteType { LPT, Directory, Network, };
public:
	ReceiptsSettings() : Core::Settings("plugin/Receipts") {}

	SETTINGS_FIELD1(bool, isT, setT, hisReaderOnly, true);
	SETTINGS_FIELD1(bool, isA, setA, utoPrint, true);
	SETTINGS_FIELD1(QString, r, setR, eceiptPath, "");
	SETTINGS_FIELD1(QString, w, setW, henRunnerNotFoundPrint, "ErrorInfo");

	SETTINGS_FIELD(QString, p, setP, rinterType, "printer/type", {});
	SETTINGS_FIELD(QString, g, setG, raphicsPrinterName, "printer/graphicsPrinterName", {});
	SETTINGS_FIELD(QString, c, setC, haracterPrinterType, "printer/characterPrinterType", {});
	SETTINGS_FIELD(QString, c, setC, haracterPrinterDevice, "printer/characterPrinterDevice", QStringLiteral("/dev/usb/lp1"));
	SETTINGS_FIELD(QString, c, setC, haracterPrinterDirectory, "printer/characterPrinterDirectory", {});
	SETTINGS_FIELD(QString, c, setC, haracterPrinterModel, "printer/characterPrinterModel", QStringLiteral("Epson TM-T88V"));
	SETTINGS_FIELD(int, c, setC, haracterPrinterLineLength, "printer/characterPrinterLineLength", 41)
	SETTINGS_FIELD(bool, is, set, CharacterPrinterGenerateControlCodes, "printer/isCharacterPrinterGenerateControlCodes", true);
	SETTINGS_FIELD(QString, c, setC, haracterPrinterUrl, "printer/characterPrinterUrl", {});
	SETTINGS_FIELD(bool, is, set, CharacterPrinterUdpProtocol, "printer/isCharacterPrinterUdpProtocol", {});
	SETTINGS_FIELD(QString, c, setC, haracterPrinterCodec, "printer/characterPrinterCodec", {});

	/// receipt report file path, built-in receipts were QML and then XML files before
	QString receiptReportPath() const;
	WhenRunnerNotFoundPrint whenRunnerNotFoundPrintEnum() const;
	PrinterType printerTypeEnum() const;
	static QString printerTypeToString(PrinterType e);
	static PrinterType printerTypeFromString(const QString &str);
	CharacterPrinteType characterPrinterTypeEnum() const;
	static QString characterPrinterTypeToString(CharacterPrinteType e);
	QString printerCaption() const;
};

