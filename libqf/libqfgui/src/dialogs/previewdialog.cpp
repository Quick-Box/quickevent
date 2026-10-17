#include "previewdialog.h"
#include "ui_previewdialog.h"

#include "messagebox.h"

#include <qf/core/log.h>

#include <QFile>
#include <QUrl>
#include <QTextEdit>
#include <QTextStream>
#if QT_VERSION_MAJOR < 6
#include <QTextCodec>
#endif

using namespace qf::gui::dialogs;

PreviewDialog::PreviewDialog(QWidget *parent) :
	Super(parent)
{
	auto *w = new QWidget();
	setCentralWidget(w);
	ui = new Ui::PreviewDialog;
	ui->setupUi(w);
	editor()->setFocus();
	setButtons(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
}

PreviewDialog::~PreviewDialog()
{
	delete ui;
}

void PreviewDialog::setFile(const QString& file_name, const QString &codec_name)
{
	qfLogFuncFrame() << file_name << codec_name;
	QFile f(file_name);
	if(f.open(QFile::ReadOnly)) {
		QTextStream ts(&f);
#if QT_VERSION_MAJOR >= 6
		auto encoding = QStringConverter::encodingForName(codec_name.isEmpty() ? QByteArray("utf-8") : codec_name.toLatin1());
		if(!encoding) {
			MessageBox::showError(this, tr("Cannot load text codec '%1'.").arg(codec_name));
			return;
		}
		ts.setEncoding(encoding.value());
#else
		QTextCodec *tc;
		if(codec_name.isEmpty())
			tc = QTextCodec::codecForName("utf-8");
		else
			tc = QTextCodec::codecForName(codec_name.toLatin1());
		if(!tc) {
			MessageBox::showError(this, tr("Cannot load text codec '%1'.").arg(codec_name));
			return;
		}
		ts.setCodec(tc);
#endif
		QString s = ts.readAll();
		setText(s, file_name);
	}
	else {
		MessageBox::showError(this, tr("Cannot open file '%1' for reading.").arg(file_name));
	}
}

void PreviewDialog::setUrl(const QUrl &url, const QString &codec_name)
{
	QString fn = url.fileName();
	setFile(fn, codec_name);
}

void PreviewDialog::setText(const QString& content, const QString &suggested_file_name)
{
	m_suggestedFileName = suggested_file_name;
	if(m_suggestedFileName.endsWith(".html", Qt::CaseInsensitive))
		ui->textEdit->setHtml(content);
	else
		ui->textEdit->setPlainText(content);
}

QString PreviewDialog::text()
{
	QString ret;
	if(m_suggestedFileName.endsWith(".html", Qt::CaseInsensitive))
		ret = ui->textEdit->toHtml();
	else
		ret = ui->textEdit->toPlainText();
	return ret;
}
/*
void  PreviewDialog::savePersistentData()
{
	if(!xmlConfigPersistentId().isEmpty()) {
		QFXmlConfigElement el = makePersistentPath();
		// save window size
		QString s;
		s = variantToString(pos());
		el.setValue("window/pos", s);
		s = variantToString(size());
		el.setValue("window/size", s);
	}
}

void  PreviewDialog::loadPersistentData()
{
	if(!xmlConfigPersistentId().isEmpty()) {
		QFXmlConfigElement el = persistentPath();
		// load window size
		QString s = el.value("window/pos", QVariant()).toString();
		if(!s.isEmpty()) {
			QVariant v = stringToVariant(s);
			move(v.toPoint());
		}
		s = el.value("window/size", QVariant()).toString();
		if(!s.isEmpty()) {
			QVariant v = stringToVariant(s);
			resize(v.toSize());
		}
	}
}
*/

int PreviewDialog::exec(const QString &content, const QString &suggested_file_name, const QString &persistent_data_id)
{
	setPersistentSettingsId(persistent_data_id);
	loadPersistentSettingsRecursively();
	//loadPersistentData();
	setText(content, suggested_file_name);
	//qfInfo() << "PreviewDialog::exec:" << text();
	int ret = exec();
	//qfInfo() << "PreviewDialog::exec ret:" << text();
	//savePersistentData();
	return ret;
}

int PreviewDialog::exec(QWidget *parent, const QString &content, const QString &suggested_file_name, const QString &persistent_data_id)
{
	PreviewDialog d(parent);
	return d.exec(content, suggested_file_name, persistent_data_id);
}

int PreviewDialog::exec(QWidget *parent, const QUrl &url, const QString &persistent_data_id, const QString &codec_name)
{
	PreviewDialog d(parent);
	d.setPersistentSettingsId(persistent_data_id);
	d.loadPersistentSettingsRecursively();
	d.setUrl(url, codec_name);
	int ret = d.exec();
	return ret;
}

QTextEdit* PreviewDialog::editor()
{
	return ui->textEdit;
}

