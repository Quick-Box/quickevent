#include "texteditwidget.h"
#include "ui_texteditwidget.h"

#include "action.h"
#include "menubar.h"
#include "toolbar.h"
#include "dialogs/filedialog.h"
#include "dialogs/messagebox.h"
#include "dialogs/dialog.h"

#include <qf/core/log.h>
#include <qf/core/utils/fileutils.h>

#include <QPrintDialog>
#include <QPrinter>
#include <QStyle>
#include <QTextStream>
#if QT_VERSION_MAJOR < 6
#include <QTextCodec>
#endif

namespace qfu = qf::core::utils;
using namespace qf::gui;

TextEditWidget::TextEditWidget(QWidget *parent) :
	Super(parent),
	//IPersistentOptions(this),
	ui(new Ui::TextEditWidget)
{
	setCodecName("System");
/*
	QFPixmapCache::insert("icon.libqf.print", ":/libqfgui/images/printer.png");
	QFPixmapCache::insert("icon.libqf.reload", ":/libqfgui/images/reload.png");
	QFPixmapCache::insert("icon.libqf.save", ":/libqfgui/images/save.png");
	QFPixmapCache::insert("icon.libqf.saveas", ":/libqfgui/images/saveas.png");
*/
	ui = new Ui::TextEditWidget;
	ui->setupUi(this);

	//createActions();
	//createToolBar();
	//QFUiBuilder::connectActions(actionList(), this);
}

TextEditWidget::~TextEditWidget()
{
	//qfDebug() << QF_FUNC_NAME;
	QF_SAFE_DELETE(ui);
}

void TextEditWidget::setText(const QString &doc)
{
	ui->editor->setPlainText(doc);
}

QString TextEditWidget::text()
{
	QString ret = ui->editor->toPlainText();
	return ret;
}

void TextEditWidget::save()
{
	save_helper(fileName());
}

void TextEditWidget::saveAs()
{
	QString fn = qfu::FileUtils::file(fileName());
	QString filter = tr("Text files (*.txt)");
	QString ext = qfu::FileUtils::extension(fn);
	if(!ext.isEmpty() && ext != "txt") {
		filter = "(*.%1);;" + filter;
		filter = filter.arg(ext);
	}
	else if(ext.isEmpty()) {
		fn += ".txt";
	}
	fn = dialogs::FileDialog::getSaveFileName(this, tr("Save as"), fn, filter);
	if(fn.isEmpty())
		return;
	save_helper(fn);
}

void TextEditWidget::save_helper(const QString &file_name)
{
	qfLogFuncFrame() << file_name;
	QString fn = file_name;
	qfDebug() << "saving to file" << fn;
	QFile f(fn);
	if(!f.open(QIODevice::WriteOnly)) {
		QString s = tr("Cannot open file %1 for writing.").arg(fn);
		dialogs::MessageBox::showInfo(this, s);
		return;
	}
	//QString codec_name = codecName();
	//if(codec_name.isEmpty()) codec_name = "utf8";
	qfDebug() << "\tcodecName:" << codecName();
	QTextStream ts(&f);
#if QT_VERSION_MAJOR >= 6
	auto encoding = QStringConverter::encodingForName(codecName().toLatin1());
	if(encoding) {
		dialogs::MessageBox::showError(this, tr("Cannot load text codec %1.").arg(codecName()));
		return;
	}
	ts.setEncoding(encoding.value());
#else
	QTextCodec *tc = QTextCodec::codecForName(codecName().toLatin1());
	if(!tc) {
		dialogs::MessageBox::showError(this, tr("Cannot load text codec %1.").arg(codecName()));
		return;
	}
	ts.setCodec(tc);
#endif

	ts << text();
	//f.write(htmlTextOriginal.toUtf8());
	//f.write(text().toUtf8());
}

framework::DialogWidget::ActionMap TextEditWidget::createActions()
{
	ActionMap ret;

	QStyle *sty = style();
	{
		QIcon ico(":/qf/gui/images/print");
		auto *a = new qf::gui::Action(ico, tr("Print"), this);
		ret[QStringLiteral("print")] = a;
		connect(a, SIGNAL(triggered()), this, SLOT(print()));
	}
	{
		QIcon ico = sty->standardIcon(QStyle::SP_DialogSaveButton);
		auto *a = new qf::gui::Action(ico, tr("Save"), this);
		ret[QStringLiteral("save")] = a;
		connect(a, SIGNAL(triggered()), this, SLOT(save()));
	}
	{
		QIcon ico = sty->standardIcon(QStyle::SP_DialogSaveButton);
		auto *a = new qf::gui::Action(ico, tr("Save as"), this);
		ret[QStringLiteral("saveAs")] = a;
		connect(a, SIGNAL(triggered()), this, SLOT(saveAs()));
	}
	{
		QIcon ico(":/qf/gui/images/wordwrap");
		auto *a = new qf::gui::Action(ico, tr("Wrap lines"), this);
		a->setCheckable(true);
		ret[QStringLiteral("wrapLines")] = a;
		connect(a, SIGNAL(triggered(bool)), this, SLOT(wrapLines(bool)));
	}

	return ret;
}

void TextEditWidget::settleDownInDialog(dialogs::Dialog *dlg)
{
	qfLogFuncFrame() << dlg;
	qf::gui::Action *act_file = dlg->menuBar()->actionForPath("file");
	act_file->setText(tr("&File"));
	act_file->addActionInto(action("save"));
	act_file->addActionInto(action("saveAs"));
	act_file->addSeparatorInto();
	act_file->addActionInto(action("print"));

	qf::gui::Action *act_view = dlg->menuBar()->actionForPath("view");
	act_view->setText(tr("&View"));
	act_view->addActionInto(action("wrapLines"));

	qf::gui::ToolBar *tool_bar = dlg->toolBar("main", true);
	tool_bar->addAction(action("save"));
	tool_bar->addAction(action("print"));
	tool_bar->addAction(action("wrapLines"));
}

void TextEditWidget::print()
{
	QPrinter printer(QPrinter::HighResolution);
	printer.setFullPage(true);

	auto *dlg = new QPrintDialog(&printer, this);
	if (dlg->exec() == QDialog::Accepted) {
		ui->editor->document()->print(&printer);
	}
	delete dlg;
}

QTextEdit* TextEditWidget::editor()
{
	return ui->editor;
}

void TextEditWidget::wrapLines(bool checked)
{
	if(checked)
		editor()->setLineWrapMode(QTextEdit::WidgetWidth);
	else
		editor()->setLineWrapMode(QTextEdit::NoWrap);
	qfDebug() << QF_FUNC_NAME << "checked" << checked << "new mode" << ui->editor->lineWrapMode();
}

#if 0
void TextEditWidget::setFile(const QString &file_name, const QString &codec_name)
{
	suggestedFileName = file_name;
	QFile f(fileName());
	setUrl(f, codec_name);
}

void TextEditWidget::setUrl(QFile &url, const QString &codec_name)
{
	setCodecName(codec_name);
	QFile &f = url;
	suggestedFileName = f.fileName();
	if(!f.open(QIODevice::ReadOnly)) {
		QFMessage::information(this, tr("Cannot open file %1 for reading.").arg(f.fileName()));
		return;
	}
	QTextCodec *tc = QTextCodec::codecForName(codecName().toLatin1());
	if(!tc) {
		QFMessage::error(this, tr("Cannot load text codec %1.").arg(codecName()));
		return;
	}
	QTextStream ts(&f);
	ts.setCodec(tc);
	QString s = ts.readAll();
	ui->editor->setPlainText(s);
}

void TextEditWidget::createToolBar()
{
	QToolBar *tb = new QToolBar(nullptr);
	tb->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	QBoxLayout *l = qobject_cast<QBoxLayout*>(layout());
	l->setMargin(0);
	if(l) l->insertWidget(0, tb);
	foreach(QFAction *a, toolBarActions) tb->addAction(a);
}

QFPart::ToolBarList TextEditWidget::createToolBars()
{
	QFPart::ToolBarList ret;
	QFToolBar *t = new QFToolBar(nullptr);
	t->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	foreach(QFAction *a, toolBarActions) t->addAction(a);
	ret << t;
	return ret;
}
#endif
