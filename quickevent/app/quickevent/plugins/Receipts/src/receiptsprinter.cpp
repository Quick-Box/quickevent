#include "receiptsprinter.h"
#include "receiptsplugin.h"
#include "receiptssettings.h"

#include <qf/core/collator.h>

#include <qf/gui/framework/mainwindow.h>
#include <qf/gui/reports/processor/reportpainter.h>
#include <qf/gui/reports/processor/reportprocessor.h>

#include <QCryptographicHash>
#include <QDomDocument>
#include <QPrinter>
#include <QPrinterInfo>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QFile>
#if QT_VERSION_MAJOR >= 6
#include <QStringEncoder>
#else
#include <QTextCodec>
#endif

//#define QF_TIMESCOPE_ENABLED
#include <qf/core/utils/fileutils.h>
#include <qf/core/utils/timescope.h>

namespace qff = qf::gui::framework;
using Receipts::ReceiptsPlugin;

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
static const auto SkipEmptyParts = QString::SkipEmptyParts;
#else
static const auto SkipEmptyParts = Qt::SkipEmptyParts;
#endif

ReceiptsPrinter::ReceiptsPrinter(QObject *parent)
	: QObject(parent)
{
}

bool ReceiptsPrinter::printReceipt(const QString &report_file_name, const QVariantMap &report_data, int card_id)
{
	qfLogFuncFrame() << "card:" << card_id;
	if(report_file_name.isEmpty()) {
		qfError() << "Empty receipt path.";
		return false;
	}
	QF_TIME_SCOPE("ReceiptsPrinter::printReceipt()");
	qf::gui::reports::ReportProcessor rp;
	{
		QF_TIME_SCOPE("setting report and data");
		auto *plugin = qf::gui::framework::getPlugin<Receipts::ReceiptsPlugin>();
		if(!rp.setReport(plugin->findReportFile(report_file_name)))
			return false;
		for(const auto &[key, val] : report_data.asKeyValueRange()) {
			rp.setTableData(key, val);
		}
	}
	ReceiptsSettings settings;
	if(settings.printerTypeEnum() == ReceiptsSettings::PrinterType::GraphicPrinter) {
		QF_TIME_SCOPE("process graphics");
		QPrinterInfo pi = QPrinterInfo::printerInfo(settings.graphicsPrinterName());
		if(pi.isNull()) {
			for(const auto &s : QPrinterInfo::availablePrinterNames()) {
				qfInfo() << "available printer:" << s;
			}
			pi = QPrinterInfo::defaultPrinter();
		}
		if(pi.isNull()) {
			qfWarning() << "Default printer not set";
			return false;
		}

		qfInfo() << "printing on:" << pi.printerName();
		QPrinter printer(pi);
		// printer.setOutputFormat(QPrinter::NativeFormat);
		qff::MainWindow *fwk = qff::MainWindow::frameWork();
		// Use screen widget (stable DPI) for layout; QPrinter DPI can change after QPainter::begin() on Windows
		rp.process(fwk);

		qf::gui::reports::ReportItemMetaPaintReport *doc;
		{
			QF_TIME_SCOPE("getting processor output");
			doc = rp.processorOutput();
		}
		qf::gui::reports::ReportItemMetaPaint *it = doc->child(0);
		if(it) {
			QF_TIME_SCOPE("draw meta-paint");
			qf::gui::reports::ReportPainter painter(&printer);
			painter.drawMetaPaint(it);
		}
		return true;
	}
	if(settings.printerTypeEnum() == ReceiptsSettings::PrinterType::CharacterPrinter) {
		QDomDocument doc;
		doc.setContent(QLatin1String("<?xml version=\"1.0\"?><report><body/></report>"));
		QDomElement el_body = doc.documentElement().firstChildElement("body");
		qf::gui::reports::ReportProcessor::HtmlExportOptions opts;
		opts.setConvertBandsToTables(false);
		rp.processHtml(el_body, opts);
		//qfInfo() << doc.toString();
		QList<QByteArray> data_lines = createPrinterData(el_body, settings);
		auto save_file = [data_lines](const QString &fn) {
			QFile f(fn);
			if(f.open(QFile::WriteOnly)) {
				//qfInfo() << "writing receipt data to:" << fn;
				for(const auto &ba : data_lines) {
					f.write(ba);
					f.write("\n");
				}
				return true;
			}
			qfError() << "Cannot open file" << f.fileName() << "for writing!";
			return false;
		};
		switch(settings.characterPrinterTypeEnum()) {
			case ReceiptsSettings::CharacterPrinteType::Directory: {
				if(!settings.characterPrinterDirectory().isEmpty()) {
					QString fn = settings.characterPrinterDirectory();
					qf::core::utils::FileUtils::ensurePath(fn);
					QCryptographicHash ch(QCryptographicHash::Sha1);
					for(const auto &ba : data_lines)
						ch.addData(ba);
					fn += '/' + QString::number(card_id) + '-'
							+ QString::fromLatin1(ch.result().toHex().mid(0, 8)) + ".txt";
					return save_file(fn);
				}
				return false;
			}
			case ReceiptsSettings::CharacterPrinteType::LPT: {
				if (!settings.characterPrinterDevice().isEmpty()) {
					return save_file(settings.characterPrinterDevice());
				}
				return false;
			}
			case ReceiptsSettings::CharacterPrinteType::Network: {
				if (!settings.characterPrinterUrl().isEmpty()) {
					QString host = settings.characterPrinterUrl().section(':', 0, 0);
					QHostAddress host_addr(host);
					int port = settings.characterPrinterUrl().section(':', 1, 1).toInt();
					if(settings.isCharacterPrinterUdpProtocol()) {
						QByteArray dgram;
						for(const QByteArray& line : data_lines) {
							dgram += line;
							dgram += '\n';
						}
						// v Racomu chteli pridat 2 radky, aby jim to vyjelo vic,
						// spravne by to asi resil custom footer u vycetky
						dgram += "\n\n\n";
						QUdpSocket socket;
						socket.writeDatagram(dgram, host_addr, port);
						return true;
					}
					{
						QTcpSocket socket;
						socket.connectToHost(host_addr, port, QIODevice::WriteOnly);
						if (socket.waitForConnected(1000)) {
							for(const QByteArray& line : data_lines) {
								socket.write(line);
								socket.write("\n");
							}
							socket.disconnectFromHost();
							if (!socket.waitForDisconnected(1000)) { // waiting till all data are sent
								qfError() << "Error while closing the connection to printer: "
										<< socket.error();
								return false;
							}
							return true;
						}
						qfError() << "Cannot open tcp connection to address "
								<< host << " on port " << port
								<< " reason: " << socket.error();
						return false;
					}
				}
				return false;
			}
		}
	}
	qfError() << "Unknow printer type";
	return false;
}

static const std::vector<QByteArray> epson_commands =
{
	QByteArray("\033E\x01"),		//    BoldOn
	QByteArray("\033E\x00", 3),		//    BoldOff
	QByteArray("\033-\x01"),		//    UnderlineOn
	QByteArray("\033-\x00", 3),		//    UnderlineOff
	QByteArray("\033-\x02"),		//    Underline2On
	QByteArray("\033-\x00", 3),		//    Underline2Off
	QByteArray("\033a\x00", 3),		//    AlignLeft
	QByteArray("\033a\x01", 3),		//    AlignCenter
	QByteArray("\033a\x02", 3),		//    AlignRight
	QByteArray("\033@"),			//    Init
	QByteArray("\035V\102\x00", 4),	//    Cut
	QByteArray("\n"),				//    Eoln
};

struct PrintData
{
	enum class Command : int {
		BoldOn = 0,
		BoldOff,
		UnderlineOn,
		UnderlineOff,
		Underline2On,
		Underline2Off,
		AlignLeft,
		AlignCenter,
		AlignRight,
		Init,
		Cut,
		Eoln,
		HorizontalLine,
		Text,
	};
	Command command;
	QByteArray data;
	int width = 0;
	Qt::Alignment alignment = Qt::AlignLeft;

	PrintData(Command cmd, const QByteArray &data = QByteArray(), int width = 0, Qt::Alignment alignment = Qt::AlignLeft)
		: command(cmd), data(data), width(width), alignment(alignment)
	{}
	bool isCommand() const {return command != Command::Text;}
	int textLength() const {return isCommand()? 0: data.length();}
	QByteArray toByteArray() const
	{
		QByteArray ret;
		switch(command) {
		case Command::BoldOn: ret += "<B>"; break;
		case Command::BoldOff: ret += "</B>"; break;
		case Command::UnderlineOn: ret += "<U>"; break;
		case Command::UnderlineOff: ret += "</U>:"; break;
		case Command::Underline2On: ret += "<u>:"; break;
		case Command::Underline2Off: ret += "</u>:"; break;
		case Command::AlignLeft: ret += "<|-->"; break;
		case Command::AlignCenter: ret += "<-|->"; break;
		case Command::AlignRight: ret += "<--|>"; break;
		case Command::Init: ret += "<INIT>"; break;
		case Command::Cut: ret += "<CUT>"; break;
		case Command::Eoln: ret += "<EOL>"; break;
		case Command::HorizontalLine: ret += "<HR>"; break;
		case Command::Text:
			if(width == 0)
				ret += data;
			else
				ret += "<" + (width < 0? "%:": QByteArray::number(width) + ":") + data + ">";
			break;
		}
		return ret;
	}
};
typedef QList<PrintData> PrintLine;

class DirectPrintContext
{
public:
	PrintLine line;
	int horizontalLayoutNestCount = 0;
};

void ReceiptsPrinter::createPrinterData_helper(const QDomElement &el, DirectPrintContext *print_context, const QString &text_encoding)
{
	PrintLine pre_commands;
	PrintLine post_commands;
	int text_width = 0;
	bool is_halign = el.tagName() == QLatin1String("div")
			&& el.attribute(qf::gui::reports::ReportProcessor::HTML_ATTRIBUTE_LAYOUT) == QLatin1String("horizontal");
	if(is_halign)
		print_context->horizontalLayoutNestCount++;
	QDomNamedNodeMap attrs = el.attributes();
	for (int i = 0; i < attrs.count(); ++i) {
		QDomAttr attr = attrs.item(i).toAttr();
		QString key = attr.name();
		if(key == QLatin1String("lpt_textWidth")) {
			QString val = attr.value();
			if(val == "%")
				text_width = -1;
			else
				text_width = val.toInt();
		}
		else if(key == QLatin1String("lpt_textStyle")) {
			QStringList styles = attr.value().split(',', SkipEmptyParts);
			for(QString s : styles) {
				s = s.trimmed();
				if(s == QLatin1String("bold")) {
					pre_commands << PrintData(PrintData::Command::BoldOn);
					post_commands.insert(0, PrintData(PrintData::Command::BoldOff));
				}
				else if(s == QLatin1String("underline")) {
					pre_commands << PrintData(PrintData::Command::UnderlineOn);
					post_commands.insert(0, PrintData(PrintData::Command::UnderlineOff));
				}
				else if(s == QLatin1String("underline2")) {
					pre_commands << PrintData(PrintData::Command::Underline2On);
					post_commands.insert(0, PrintData(PrintData::Command::Underline2Off));
				}
			}
		}
		else if(key == QLatin1String("lpt_textAlign") && el.tagName() != QLatin1String("p")) {
			QString val = attr.value();
			if(val == QLatin1String("right")) {
				pre_commands << PrintData(PrintData::Command::AlignRight);
				post_commands.insert(0, PrintData(PrintData::Command::AlignLeft));
			}
			else if(val == QLatin1String("center")) {
				pre_commands << PrintData(PrintData::Command::AlignCenter);
				post_commands.insert(0, PrintData(PrintData::Command::AlignLeft));
			}
			else if(val == QLatin1String("left")) {
				pre_commands << PrintData(PrintData::Command::AlignLeft);
				post_commands.insert(0, PrintData(PrintData::Command::AlignLeft));
			}
		}
		else if(key == QLatin1String("lpt_borderTop")) {
			QString val = attr.value();
			pre_commands << PrintData(PrintData::Command::HorizontalLine, val.toUtf8());
		}
		else if(key == QLatin1String("lpt_borderBottom")) {
			QString val = attr.value();
			post_commands << PrintData(PrintData::Command::HorizontalLine, val.toUtf8());
		}
	}
	print_context->line << pre_commands;
	if(el.tagName() == QLatin1String("p")) {
		QString ta = el.attribute(QLatin1String("lpt_textAlign"));
		Qt::Alignment text_align = Qt::AlignLeft;
		if(ta == QLatin1String("right"))
			text_align = Qt::AlignRight;
		else if(ta == QLatin1String("center"))
			text_align = Qt::AlignHCenter;
		QByteArray text = encodeText(el.text(), text_encoding);
		print_context->line << PrintData(PrintData::Command::Text, text, text_width, text_align);
	}
	{
		for(QDomElement el1 = el.firstChildElement(); !el1.isNull(); ) {
			createPrinterData_helper(el1, print_context, text_encoding);
			el1 = el1.nextSiblingElement();
			if(!el1.isNull()) {
				//if(!is_halign || (is_halign && print_context->horizontalLayoutNestCount == 1)) {
				if(!is_halign) {
					print_context->line << PrintData(PrintData::Command::Eoln);
				}
			}
		}
	}
	print_context->line << post_commands;
	if(is_halign)
		print_context->horizontalLayoutNestCount--;
}

QByteArray ReceiptsPrinter::encodeText(const QString text, const QString &text_encoding) const
{
#if QT_VERSION_MAJOR >= 6
	auto ba = text_encoding.toUtf8();
	auto enc = QStringConverter::encodingForName(ba.constData());
	if(enc) {
		auto from_utf16 = QStringEncoder(enc.value());
		return from_utf16(text);
	}
	return qf::core::Collator::toAscii7(QLocale::Czech, text, false);
#else
	QByteArray ret;
	QTextCodec *tc = nullptr;
	if(text_encoding != QLatin1String("ASCII7")) {
		QByteArray ba = text_encoding.toUtf8();
		tc = QTextCodec::codecForName(ba);
	}
	if(tc)
		ret = tc->fromUnicode(text);
	else
		ret = qf::core::Collator::toAscii7(QLocale::Czech, text, false);
	return ret;
#endif
}

namespace {
QList<PrintLine> alignPrinterData(DirectPrintContext *print_context, const ReceiptsSettings &receipts_settings)
{
	QList<PrintLine> ret;
	int line_length = receipts_settings.characterPrinterLineLength();
	PrintLine line;
	{
		line << PrintData(PrintData::Command::Init);
		ret.insert(ret.length(), line);
	}
	line.clear();
	for (int i = 0; i < print_context->line.count(); ++i) {
		const PrintData &pd = print_context->line[i];
		bool is_eol = (pd.command == PrintData::Command::Eoln) || (i == (print_context->line.count() - 1));
		if(pd.command != PrintData::Command::Eoln) {
			line << pd;
		}
		if(is_eol) {
			int fixed_text_len = 0;
			int spring_cnt = 0;
			for (const auto &pd2 : line) {
				if(pd2.width < 0)
					spring_cnt++;
				else if(pd2.width > 0)
					fixed_text_len += pd2.width;
				else
					fixed_text_len += pd2.textLength();
			}
			for (auto &pd2 : line) {
				if(pd2.isCommand())
					continue;
				int w = pd2.width;
				if(w < 0)
					w = (line_length - fixed_text_len) / spring_cnt;
				if(w > 0) {
					int w_rest = w - pd2.textLength();
					if(w_rest > 0) {
						if(pd2.alignment == Qt::AlignLeft)
							pd2.data = pd2.data + QByteArray(w_rest, ' ');
						else if(pd2.alignment == Qt::AlignRight)
							pd2.data = QByteArray(w_rest, ' ') + pd2.data;
						else if(pd2.alignment == Qt::AlignHCenter)
							pd2.data = QByteArray((w_rest/2) + 1, ' ') + pd2.data + QByteArray(w_rest/2 + 1, ' ');
					}
					pd2.data = pd2.data.mid(0, w);
					pd2.width = 0;
				}
			}
			ret.insert(ret.length(), line);
			line.clear();
		}
	}
	{
		line.clear();
		line << PrintData(PrintData::Command::Cut);
		ret.insert(ret.length(), line);
	}
	return ret;
}

QList<QByteArray> interpretControlCodes(const QList<PrintLine> &lines, const ReceiptsSettings &receipts_settings)
{
	QList<QByteArray> ret;
	int line_length = receipts_settings.characterPrinterLineLength();
	bool include_escapes = receipts_settings.isCharacterPrinterGenerateControlCodes();
	for(const PrintLine& line : lines) {
		QByteArray ba;
		int line_text_len = 0;
		for(const PrintData &pd : line) {
			if(pd.command == PrintData::Command::Text) {
				ba += pd.data;
				line_text_len += pd.data.length();
			}
			else if(pd.command == PrintData::Command::HorizontalLine) {
				QByteArray hr_data(line_length, pd.data[0]);
				if(line_text_len == 0) {
					ret.insert(ret.length(), ba + hr_data);
					ba.clear();
				}
				else {
					ret.insert(ret.length(), ba);
					ba = hr_data;
					line_text_len = hr_data.length();
				}
			}
			else if(include_escapes) {
				auto ix = (size_t)pd.command;
				QF_ASSERT(ix < epson_commands.size(), QString("%1 - Bad command index!").arg(ix), continue);
				ba += epson_commands[ix];
			}
		}
		ret.insert(ret.length(), ba);
	}
	return ret;
}
}

QList<QByteArray> ReceiptsPrinter::createPrinterData(const QDomElement &body, const ReceiptsSettings &receipts_settings)
{
	DirectPrintContext dpc;
	createPrinterData_helper(body, &dpc, receipts_settings.characterPrinterCodec());
	QList<PrintLine> lines = alignPrinterData(&dpc, receipts_settings);
	QList<QByteArray> ret = interpretControlCodes(lines, receipts_settings);
	return ret;
}

