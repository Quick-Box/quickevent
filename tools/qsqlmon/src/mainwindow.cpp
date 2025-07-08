#include "theapp.h"
#include "mainwindow.h"
#include "sqldock.h"
#include "sqltextedit.h"
#include "servertreedock.h"
#include "servertreemodel.h"
#include "servertreeitem.h"
#include "dlgeditconnection.h"
#include "dlgaltertable.h"
#include "columnselectorwidget.h"
#include "qfstatusbar.h"

#include "driver/qfhttpmysql/qfhttpmysql.h"

#include "qfsqlsyntaxhighlighter.h"

#include <qf/core/log.h>
#include <qf/core/assert.h>
#include <qf/core/string.h>
#include <qf/core/utils/fileutils.h>
#include <qf/core/model/sqltablemodel.h>
#include <qf/core/sql/query.h>
#include <qf/qmlwidgets/tableview.h>
#include <qf/qmlwidgets/dialogs/messagebox.h>
#include <qf/qmlwidgets/dialogs/filedialog.h>
#include <qf/qmlwidgets/dialogs/previewdialog.h>
#include <qf/qmlwidgets/dialogs/dialog.h>

#include <QStandardItemModel>
#include <QTextCursor>
#include <QScrollBar>
#include <QUrl>
#include <QToolBar>
#include <QTimer>
#include <QSqlError>
#include <QSqlIndex>
#include <QSqlField>
#include <QLibraryInfo>
#include <QDialog>
#include <QWindowStateChangeEvent>
#include <QPluginLoader>
#include <QSpinBox>
#include <QMessageBox>
#include <QMenuBar>
#include <QInputDialog>
#include <QSettings>
#include <QTextStream>

#include <limits>

MainWindow::MainWindow(QWidget *parent, Qt::WindowFlags flags)
	: QMainWindow(parent, flags)
	, qf::qmlwidgets::framework::IPersistentSettings(this)
{
	//setAttribute(Qt::WA_DeleteOnClose);
	f_sqlDelimiter = ';';
	f_activeSetNames = "<no change>";

	//loadPersistentData();

	auto *g = new QWidget(this);
	//g->setMinimumSize(400, 400);
	setCentralWidget(g);

	ui.setupUi(g);

	createActions();
	createMenus();
	createToolBars();
	//qfDebug("%s: %i",__FILE__, __LINE__);
	createStatusBar();
	createDockWindows();

	setPersistentSettingsId("MainWindow");
	loadPersistentSettings();

	init(); // must bee after all create*() functions
}

MainWindow::~MainWindow()
{
	qfLogFuncFrame();
	savePersistentSettings();
}

void MainWindow::init()
{
	setWindowTitle(tr("QT SQL Monitor"));
	setWindowIcon(QIcon(":/images/sun.png"));
	/*
	QList<int> szs;
	szs << size.width()/3 << (size.width() - size.width()/3);
	ui.splitter01->setSizes(szs);
	*/
	auto *model = new ServerTreeModel(this);
	//qfDebug() << "MODEL" << qobject_cast<QFObjectItemModel*>(model) << model;
	//model->dumpObjectInfo();
	model->loadSettings();

	Ui::ServerTreeWidget &ui_srv = serverDock->ui;
	ui_srv.treeServers->setModel(model);
	connect(ui_srv.treeServers, SIGNAL(expanded(const QModelIndex&)), this, SLOT(treeNodeExpanded(const QModelIndex&)));
	connect(ui_srv.treeServers, SIGNAL(collapsed(const QModelIndex&)), this, SLOT(treeNodeCollapsed(const QModelIndex&)));
	connect(ui_srv.treeServers, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(treeNodeDoubleClicked(const QModelIndex&)));
	connect(ui_srv.treeServers, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(treeServersContextMenuRequest(const QPoint&)));
	ui_srv.treeServers->setContextMenuPolicy(Qt::CustomContextMenu);
	ui_srv.treeServers->resizeColumnToContents(0);

	//ui.tblSql->verticalHeader()->setFixedHeight(12);
	//setActiveConnection(qf::core::sql::Connection());
	setActiveConnection2(nullptr);

	connect(ui.queryView, SIGNAL(statusBarAction(const QString&)), this, SLOT(onTableStatusBarAction(const QString&)));

	setPersistentSettingsId("MainWindow");

	/// jinak jsem nevymyslel, jak zaridit, aby se nastavil 1. sloupec treeview
	/// na sirku textu. eventy tu funkci ignorovaly.
	QTimer::singleShot(0, this, SLOT(lazyInit()));
}

void MainWindow::lazyInit()
{
	loadPersistentSettingsRecursively();
	serverDock->ui.treeServers->resizeColumnToContents(0);
}

qf::core::model::SqlTableModel* MainWindow::queryViewModel()
{
	auto *m1 = ui.queryView->tableView()->tableModel();
	auto *m = qobject_cast<qf::core::model::SqlTableModel*>(m1);
	qfDebug() << "model:" << m1 << m;
	//QF_CHECK(m!=nullptr, "Model is NULL or not a kind of qf::core::model::SqlTableModel.");
	return m;
}

void MainWindow::setQueryViewModel(qf::core::model::SqlTableModel *m)
{
	qfDebug() << "set model:" << m;
	ui.queryView->tableView()->setTableModel(m);
	if(m) {
		m->setParent(ui.queryView);
		connect(m, SIGNAL(reloaded()), ui.queryView, SLOT(updateStatus()));
		//ui.queryView->updateStatus();
	}
	qfDebug() << "set model read back:" << ui.queryView->tableView()->tableModel();
}

qf::core::sql::Connection MainWindow::setActiveConnection2(Database *dd)
{
	qf::core::sql::Connection c;
	if(dd)
		c = dd->sqlConnection();
	//qfInfo() << c.signature();
	qf::core::sql::Connection ret = setActiveConnection1(c);
	#if 0
	if(dd && c.isValid()) {
		Connection *cc = qfFindParent<Connection*>(dd, !Qf::ThrowExc);
		//if(cc) qfInfo() << "cc:" << cc->param("driver");
		if(cc && cc->param("driver").endsWith("MYSQL")) {
			QString set_names = cc->param("mysqlSetNames");
			if(set_names != f_activeSetNames) {
				f_activeSetNames = set_names;
				if(set_names.isEmpty() || set_names == "<no change>") set_names = "utf8";
				QFSqlQuery q(c);
                q.exec("SET NAMES " SARG(set_names));
                qfInfo() << "SET NAMES " SARG(set_names);
			}
		}
	}
	#endif
	return ret;
}

qf::core::sql::Connection MainWindow::setActiveConnection1(const qf::core::sql::Connection &c)
{
	qfLogFuncFrame();// << c.signature();
	//qfInfo() << "set activeConnection:" << c.signature();
	//fprintf(stderr, "set activeConnection MYSQL %p\n", c.driver()->handle().constData());
	if(c.isValid()) {
		setStatusText(qf::core::sql::Connection(c).info());
		foreach(QString s, c.connectOptions().split(';')) {
			QString opt = "QF_CODEC_NAME";
			QStringList sl = s.split('=');
			if(!sl.isEmpty() && sl[0].trimmed() == opt) {
				setStatusText("codec: " + sl[1], 1);
			}
		}
		/*
		{
			if(c->param("driver").endsWith("MYSQL")) {
				QString set_names = c->param("mysqlSetNames");
				if(!set_names.isEmpty() && set_names[0] != '<') {
					QFSqlQuery q(sqlConnection);
					q.exec("SET NAMES "SARG(set_names));
				}
			}
		}
		*/
	}
	else {
		setStatusText("not connected ...");
		setStatusText("", 1);
	}
	if(activeConnection().isValid()) {
		if(qf::core::sql::Connection(activeConnection()).signature() == qf::core::sql::Connection(c).signature())
			return c;
	}
	QObject *old_model = queryViewModel();
	qfDebug() << "\t deletenig old model:" << old_model;
	QF_SAFE_DELETE(old_model);
	auto *m = new qf::core::model::SqlTableModel(this);
	m->setConnectionName(c.connectionName());
	qfDebug() << "\t new table model created:" << m;
	setQueryViewModel(m);
	qfDebug() << "\t model set";

	qf::core::sql::Connection ret = m_activeConnection;
	//qfDebug() << "m_activeConnection = c";
	m_activeConnection = c;
	//qfInfo() << "activeConnection:" << m_activeConnection.signature();
	//queryModel->setConnection(c);

	/// vymen completer
	{
		QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

		QStandardItemModel *completion_model = nullptr;
		completion_model = new QStandardItemModel();
		QStandardItem *root_item = nullptr;
		root_item = completion_model->invisibleRootItem();

		//bool completion = theApp()->config()->value("/sqleditor/fieldcompletion").toBool();
#if 0
		if(completion && m_activeConnection.isOpen()) try {
			QSet<QString> field_names;
			/// fieldy
			QFSqlCatalog &catalog = m_activeConnection.catalog();
			foreach(QString db, catalog.databases()) {
				QFSqlDbInfo dbi = catalog.database(db, !Qf::ThrowExc);
				if(dbi.isValid()) {
					QStandardItem *db_item = new QStandardItem(db);
					root_item->appendRow(db_item);
					foreach(QString tbl, dbi.tables()) {
						QFSqlTableInfo tbli = dbi.table(tbl, !Qf::ThrowExc);
						if(tbli.isValid()) {
							QStandardItem *tbl_item = new QStandardItem(tbl);
							QStandardItem *root_tbl_item = new QStandardItem(tbl);
							db_item->appendRow(tbl_item);
							root_item->appendRow(root_tbl_item);
							foreach(QString fld, tbli.fields()) {
								QStandardItem *fld_item = new QStandardItem(fld);
								tbl_item->appendRow(fld_item);
								root_tbl_item->appendRow(fld_item->clone());
								field_names << fld;
							}
						}
					}
				}
			}
			foreach(QString s, field_names) root_item->appendRow(new QStandardItem(s));
		}
		catch(QFException &e) {QFDlgException::exec(this, e);}
#endif
		/// klicovy slova
		//if(theApp()->config()->value("/sqleditor/sqlcompletion").toBool())
		{
			QStringList sl = QFSqlSyntaxHighlighter::keyWords().values();
			foreach(QString s, sl) {
				auto *item = new QStandardItem(s);
				root_item->appendRow(item);
			}
		}

		SqlTextEdit *tsql = sqlDock->ui.txtSql;
		tsql->setCompletionModel(completion_model);

		QApplication::restoreOverrideCursor();
	}

	return ret;
}

void MainWindow::setStatusText(const QString& s, int where)
{
	auto *sb = qobject_cast<QFStatusBar*>(statusBar());
	if(sb) sb->setLabelText(where, s);
}

/*
void MainWindow::closeEvent(QCloseEvent *event)
{
    event->accept();
}

bool MainWindow::event(QEvent *event)
{

	bool ret = QMainWindow::event(event);
	//if(event->type() == QEvent::Polish) qfDebug() << "polished event";
	return ret;
}

void MainWindow::showEvent(QShowEvent * event)
{
    QMainWindow::showEvent(event);
	//ui.treeServers->resizeColumnToContents(0);
	//qfDebug() << "show event";
}
void MainWindow::focusInEvent(QFocusEvent * event)
{
    QMainWindow::focusInEvent(event);
	ui.treeServers->resizeColumnToContents(0);
	qfDebug() << "focus in event";
}
*/

void MainWindow::about()
{
	QMessageBox::about(this, tr("About Application"),
					   tr("<b>Qt SQL Monitor</b><br>%2<br>"
							   "ver. %1").arg(theApp()->versionString()).arg(__DATE__));
}

void MainWindow::aboutQt()
{
	QString s = tr("<p><b>Qt %1</b></p>"
			"<p><a href=\"http://www.trolltech.com/qt/\">www.trolltech.com/qt/</a><p>").arg(QT_VERSION_STR);
	QMessageBox mb(this);
	mb.setWindowTitle(tr("About Qt"));
	mb.setText(s);
	QPixmap pm(":/libqfgui/images/qt-logo.png");
	mb.setIconPixmap(pm);
	mb.addButton(QMessageBox::Ok);
	mb.exec();
	//QApplication::aboutQt();
}

void MainWindow::createActions()
{
	QAction *a;
	/*
	a = new QAction(QPixmap(":/images/connect.png"), tr("Connect"), this);
	a->setCheckable(true);
	a->setStatusTip(tr("Connect application to the UDP server"));
    //connect(actConnect, SIGNAL(checked(bool)), this, SLOT(connectServer(bool)));
	actionMap["connect"] = a;
	*/
	a = new QAction(tr("&Config"), this);
	a->setStatusTip(tr("Open the aplication config window"));
	connect(a, &QAction::triggered, this, &MainWindow::configure);
	actionMap["config"] = a;

	a = new QAction(tr("&Quit"), this);
	a->setShortcut(tr("Ctrl+Q"));
	a->setStatusTip(tr("Exit the application"));
	connect(a, &QAction::triggered, this, &MainWindow::close);
	actionMap["exit"] = a;

	a = new QAction(QPixmap(":/images/lightning.png"), "&Execute SQL", this);
	a->setShortcut(tr("Shift+Enter"));
	a->setStatusTip(tr("Execute SQL command"));
	connect(a, &QAction::triggered, this, &MainWindow::executeSql);
	actionMap["executeSql"] = a;

	a = new QAction(QPixmap(":/images/lightning-file.png"), "&Execute SQL script", this);
	//a->setShortcut(tr("Shift+Enter"));
	a->setStatusTip(tr("Execute SQL script"));
	connect(a, &QAction::triggered, this, &MainWindow::executeSqlScript);
	actionMap["executeSqlScript"] = a;

	a = new QAction(QPixmap(":/images/lightning-selection.png"), "&Execute selected lines", this);
	//a->setShortcut(tr("Shift+Enter"));
	a->setStatusTip(tr("Execute selected lines"));
	connect(a, &QAction::triggered, this, &MainWindow::executeSelectedLines);
	actionMap["executeSelectedLines"] = a;

	a = new QAction("&Set SQL delimiter", this);
	//a->setShortcut(tr("Shift+Enter"));
	a->setStatusTip(tr("Set SQL delimiter"));
	connect(a, &QAction::triggered, this, &MainWindow::setSqlDelimiter);
	actionMap["setSqlDelimiter"] = a;

	a = new QAction("Show SQL journal", this);
	connect(a, &QAction::triggered, this, &MainWindow::showSqlJournal);
	actionMap["showSqlJournal"] = a;

	a = new QAction(QPixmap(":/images/tear-off.png"), tr("Tear off SQL table"), this);
	a->setShortcut(tr("Ctrl+T"));
	a->setToolTip(tr("Tear off SQL table in a new modeless window"));
	connect(a, &QAction::triggered, this, &MainWindow::tearOffTable);
	actionMap["tearOffTable"] = a;

	a = new QAction(QPixmap(":/libqfgui/images/wordwrap.png"), tr("Word wrap"), this);
	a->setCheckable(true);
	a->setChecked(true);
	//a->setShortcut(tr("Ctrl+T"));
	//a->setToolTip(tr("Tear off SQL table in a new modeless window"));
	connect(a, &QAction::triggered, this, &MainWindow::wordWrapSqlEditor);
	actionMap["wordWrapSqlEditor"] = a;

	//qfDebug("%s: %i",__FILE__, __LINE__);
	a = new QAction(tr("&About"), this);
	a->setStatusTip(tr("Show the application's About box"));
	connect(a, &QAction::triggered, this, &MainWindow::about);
	actionMap["about"] = a;

	a = new QAction(tr("&About Qt"), this);
	//a->setStatusTip(tr("Show the application's About box"));
	connect(a, &QAction::triggered, this, &MainWindow::aboutQt);
	actionMap["aboutQt"] = a;

	a = new QAction(tr("&Change Log"), this);
	connect(a, &QAction::triggered, this, &MainWindow::changeLog);
	actionMap["changeLog"] = a;

	a = new QAction(tr("&MySQL syntax"), this);
	connect(a, &QAction::triggered, this, &MainWindow::mysqlSyntax);
	actionMap["mysqlSyntax"] = a;

	a = new QAction(tr("&SQLite syntax"), this);
	connect(a, &QAction::triggered, this, &MainWindow::sqliteSyntax);
	actionMap["sqliteSyntax"] = a;

	a = new QAction(tr("&Available drivers"), this);
	connect(a, &QAction::triggered, this, &MainWindow::availableDrivers);
	actionMap["availableDrivers"] = a;

	a = new QAction(tr("&Check drivers"), this);
	connect(a, &QAction::triggered, this, &MainWindow::checkDrivers);
	actionMap["checkDrivers"] = a;
	/*
	a = new QAction(tr("About &Qt"), this);
	a->setStatusTip(tr("Show the Qt library's About box"));
	connect(a, &QAction::triggered, this, &MainWindow::aboutQt()));
	actionMap["aboutQt"] = a;
	*/
	a = new QAction(tr("Add connection"), this);
	a->setStatusTip(tr("Add database connection to the tree"));
	connect(a, &QAction::triggered, this, [this]() { addServer(); });
	actionMap["addServer"] = a;
}

void MainWindow::createMenus()
{
	menuFile = menuBar()->addMenu(tr("&Server"));
    //menuFile->addAction(action("connect"));
	menuFile->addSeparator();
	menuFile->addAction(action("config"));
	menuFile->addSeparator();
	menuFile->addAction(action("exit"));

	QMenu *m = menuBar()->addMenu(tr("&Sql"));
	m->addAction(action("executeSql"));
	m->addAction(action("executeSelectedLines"));
	m->addAction(action("executeSqlScript"));
	m->addAction(action("setSqlDelimiter"));
	m->addSeparator();
	m->addAction(action("showSqlJournal"));

	menuView = menuBar()->addMenu(tr("&View"));

	menuBar()->addSeparator();

	menuHelp = menuBar()->addMenu(tr("&Help"));
	menuHelp->addAction(action("about"));
    menuHelp->addAction(action("aboutQt"));
	menuHelp->addSeparator();
	menuHelp->addAction(action("changeLog"));
	menuHelp->addAction(action("mysqlSyntax"));
	menuHelp->addAction(action("sqliteSyntax"));
	menuHelp->addSeparator();
	menuHelp->addAction(action("availableDrivers"));
	menuHelp->addAction(action("checkDrivers"));
}

void MainWindow::createToolBars()
{
	QToolBar *t;
	t = addToolBar(tr("Sql"));
	t->setObjectName("mainToolBar");
	t->addAction(action("executeSql"));
	t->addAction(action("executeSelectedLines"));
	t->addAction(action("executeSqlScript"));
	t->addAction(action("tearOffTable"));
	t->addAction(action("wordWrapSqlEditor"));
	t->addSeparator();
	t->addWidget(new QLabel(tr("limit")));
	edLimit = new QSpinBox();
	edLimit->setMinimum(-1);
	edLimit->setMaximum(std::numeric_limits<int>::max());
	edLimit->setSpecialValueText(tr("unlimited"));
	edLimit->setValue(-1);
	t->addWidget(edLimit);
}

void MainWindow::createStatusBar()
{
	setStatusBar(new QFStatusBar());
	setStatusText(tr("not connected ..."), 0);
	setStatusText(tr("info"), 1);
}

void MainWindow::createDockWindows()
{
	sqlDock = new SqlDock(this);
	menuView->addAction(sqlDock->toggleViewAction());
	addDockWidget(Qt::BottomDockWidgetArea, sqlDock);
	connect(sqlDock->ui.txtSql, SIGNAL(doSql()), actionMap["executeSql"], SIGNAL(triggered()));
    //connect(sqlDock->ui.txtSql, SIGNAL(doSql()), this, SLOT(executeSql()));

	serverDock = new ServerTreeDock(this);
	menuView->addAction(serverDock->toggleViewAction());
	addDockWidget(Qt::LeftDockWidgetArea, serverDock);
}

void MainWindow::configure()
{
	/*
	QFDlgXmlConfig dlg(this);
	QFXmlConfig conf;
	conf.copyData(theApp()->config());
	//conf.detachData();
	dlg.setConfig(&conf);
	if(dlg.exec() == QDialog::Accepted) {
		theApp()->config()->copyData(&conf);
		theApp()->config()->save();
		//setStatusText("codec: " + conf.value("/i18n/dbtextcodec", QVariant()).toString(), 1);
		theApp()->redirectLog();
	}
	*/
}

void MainWindow::appendInfo(const QString &s)
{
	static const int max_chars = 5000;
	Ui::SqlWidget &ui_sql = sqlDock->ui;
	QString ss = ui_sql.txtLog->toPlainText();
	if(ss.length() > max_chars) {
		ss = ss.right(max_chars * 2 / 3);
		ss = "-----------------------------------\n" + ss;
		ui_sql.txtLog->setPlainText(ss);
	}
	ui_sql.txtLog->append(s);
    // tohle zaruci, ze uvidim vzdycky posledni radek
	QScrollBar *sb = ui_sql.txtLog->verticalScrollBar();
	sb->setValue(sb->maximum());
}

//-------------------------------------------------------------------------
//-------------------------------------------------------------------------

bool MainWindow::execQuery(const QString& query_str)
{
	qfDebug() << QF_FUNC_NAME;
	if(!activeConnection().isOpen()) {
		qf::qmlwidgets::dialogs::MessageBox::showInfo(this, tr("No active connection !"));
		return false;
	}

	QString qs = query_str.trimmed();
	appendInfo(qs);
	bool ok = true;
	bool is_select = qs.startsWith(QLatin1String("SELECT"), Qt::CaseInsensitive);
	do {
		if(is_select) {
			qf::core::model::SqlTableModel *m = queryViewModel();
			m->clearColumns();
			m->setQuery(qs);
			ok = m->reload();
			if(ok) {
				ui.queryView->tableView()->resizeColumnsToContents();
				ui.queryView->setInfo(qs);
			}
			else {
				QString msg = m->recentlyExecutedQueryError().text();
				qf::qmlwidgets::dialogs::MessageBox::showError(this, msg);
				appendInfo(msg);
			}
		}
		else {
			qf::core::sql::Query q(activeConnection());
			ok = q.exec(qs);
			if(!ok) {
				QString msg = q.lastError().text();
				qf::qmlwidgets::dialogs::MessageBox::showError(this, msg);
				appendInfo(msg);
			}
			else {
				if(q.isSelect()) {
					//qfInfo() << "SELECT";
					// it was actualy SELECT, sqlite PRAGMA table_info can behave like this
					is_select = true;
					continue;
				}
				appendInfo(tr("affected rows: %1").arg(q.numRowsAffected()));
			}
		}
		break;
	} while(true);

	return ok;
}

qf::core::sql::Connection MainWindow::activeConnection()
{
	//QF_CHECK(m_activeConnection.isValid(), "Connection is not valid !!!");
	return m_activeConnection;
}

bool MainWindow::execCommand(const QString& query_str)
{
	qfDebug() << QF_FUNC_NAME << "\n\t" << query_str;
	if(!activeConnection().isOpen()) {
		qf::qmlwidgets::dialogs::MessageBox::showInfo(this, tr("No active connection !"));
		return true;
	}
	appendInfo(query_str);
	QSqlQuery q(activeConnection());
	bool ok = q.exec(query_str);
	if(ok) {
		appendInfo(tr("affected rows: %1").arg(q.numRowsAffected()));
	}
	else {
		QString msg = q.lastError().text();
		qf::qmlwidgets::dialogs::MessageBox::showError(this, msg);
		appendInfo(msg);
	}
	return ok;
}

static bool is_white_space(QChar c)
{
	ushort u = c.unicode();
	if(u <= ' ') {
		if(u == '\n') return false;
		return true;
	}
	return false;
}

void MainWindow::executeSql()
{
	/// extract command under cursor position (commands are separated by ;)
	SqlTextEdit *ed = sqlDock->ui.txtSql;
	qf::core::String s = ed->toPlainText();
	QTextCursor c = ed->textCursor();
	int pos = c.position();

	if(pos == s.size() && pos>0) pos--;

	/// specialni je pripad, kdy je kurzor za strednikem na konci radku
	/// to znamena, nalevo bile znaky nasledujici strednik
	/// napravo mezery ukoncene \n
	int i;
	for(i=pos; i>0; i--) {
		//qfDebug() << "c: " << s[i] << " n: " << s[i].unicode();
		if(!s[i].isSpace()) break;
	}
	if(s[i]==f_sqlDelimiter) {
		int strednik_pos = i;
		for(i=pos; i<s.size(); i++) if(!is_white_space(s[i])) break;
		if(i==s.size() || s[i]=='\n') {
			/// je to ten pripad
			/// v takovem pripade posun pos na ukoncujici strednik
			pos = strednik_pos;
		}
	}

	/// spocitel stredniky az do konce, aby se poznalo, jestli je kurzor uvnitr nebo vne uvozovek
	bool cursor_in_quotes = false;
	for(int i=pos; i<s.size(); i++) {
		if(s[i]=='\'') cursor_in_quotes = !cursor_in_quotes;
	}
	//qfDebug() << QF_FUNC_NAME << "cursor in quotes:" << cursor_in_quotes;

	int p1, p2;
	bool in_quotes = cursor_in_quotes;
	for(p1=pos-1; p1>=0; p1--) {
		if(s[p1] == '\'') in_quotes = !in_quotes;
		else if(s[p1] == f_sqlDelimiter && !in_quotes) break;
	}
	p1++;
	in_quotes = cursor_in_quotes;
	for(p2=pos; p2<s.size(); p2++) {
		if(s[p2] == '\'') in_quotes = !in_quotes;
		else if(s[p2] == f_sqlDelimiter && !in_quotes) break;
	}

	s = s.slice(p1, p2).trimmed();
	qfDebug() << QString("Executing SQL: [%1]").arg(s);
	execQuery(s);
}

void MainWindow::executeSqlLines(const QString & lines)
{
	if(!lines.isEmpty()) {
		QStringList sl = qf::core::String(lines).splitAndTrim(f_sqlDelimiter, '\'');
		foreach(QString s, sl)
			if(!execQuery(s))
				break;
	}
}

void MainWindow::executeSelectedLines()
{
	qfLogFuncFrame();
	SqlTextEdit *ed = sqlDock->ui.txtSql;
	QString s = ed->toPlainText();
	QTextCursor c = ed->textCursor();
	int sel_end = c.selectionEnd();
	c.setPosition(c.selectionStart());
	c.movePosition(QTextCursor::StartOfLine);
	c.setPosition(sel_end, QTextCursor::KeepAnchor);
	c.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
	sqlDock->ui.txtSql->setTextCursor(c);
	executeSqlLines(c.selectedText());
}

void MainWindow::executeSqlScript()
{
	QString s = qf::qmlwidgets::dialogs::FileDialog::getOpenFileName(this, "Choose a file", QString(), "SQL script (*.sql);; All files (*)");
	if(!s.isEmpty()) {
		QFile f(s);
		if(f.open(QFile::ReadOnly)) {
			QTextStream in(&f);
#if QT_VERSION_MAJOR >= 6
			in.setEncoding(QStringConverter::encodingForName("UTF-8").value());
#else
			in.setCodec("UTF-8");
#endif
			s = in.readAll();
			executeSqlLines(s);
		}
	}
}

void MainWindow::wordWrapSqlEditor(bool checked)
{
	qfLogFuncFrame();
	SqlTextEdit *ed = sqlDock->ui.txtSql;
	ed->setWordWrapMode(checked? QTextOption::WordWrap: QTextOption::NoWrap);
}

void MainWindow::setDbSearchPath(const QString &path)
{
	if(activeConnection().driverName().endsWith("MYSQL")) {
		if(!path.isEmpty()) {
			auto *http_proxy_driver = qobject_cast<QFHttpMySqlDriver*>(activeConnection().driver());
			if(http_proxy_driver) {
				http_proxy_driver->setCurrentDatabase(path);
			}
			else {
				QSqlQuery q(activeConnection());
				QString s = "USE " + path;
				qfDebug() << QString("Executing SQL: [%1]").arg(s);
				q.exec(s);
			}
			setStatusText(path, 2);
			return;
		}
	}
	else if(activeConnection().driverName().endsWith("PSQL")) {
		if(!path.isEmpty()) {
			QSqlQuery q(activeConnection());
			QString s = "SET SCHEMA '" + path + "'";
			qfDebug() << QString("Executing SQL: [%1]").arg(s);
			q.exec(s);
			setStatusText(path, 2);
			return;
		}
	}
	setStatusText(QString(), 2);
}

void MainWindow::treeNodeExpanded(const QModelIndex &index)
{
	auto *model = qobject_cast<const ServerTreeModel*>(index.model());
	QObject *o = model->index2object(index);
	setDbSearchPath(QString());
	if(o) do {
		Ui::ServerTreeWidget &ui_srv = serverDock->ui;
		ui_srv.treeServers->resizeColumnToContents(0);
		if(auto *c = qobject_cast<Connection*>(o)) {
			Q_UNUSED(c);
			break;
		}
		if(auto *d = qobject_cast<Database*>(o)) {
			// aktivni je to pripojeni, ktere bylo naposledy expandovano
			setActiveConnection2(d);
			break;
		}
		if(auto *s = qobject_cast<Schema*>(o)) {
			Database *d = s->database();
			setActiveConnection2(d);
			setDbSearchPath(s->objectName());
			break;
		}
	} while(false);
}

void MainWindow::treeNodeCollapsed(const QModelIndex &index)
{
	// POZOR sem nedavat nic, co maze objekty, protoze QTreeView::rowsAboutToBeRemoved()
	// vola tuhle funkci, takze jsem si to mazal pod prdeli
	auto *model = qobject_cast<const ServerTreeModel*>(index.model());
	QObject *o = model->index2object(index);
	if(!o) return;
	if(auto *c = qobject_cast<Connection*>(o)) {
		Q_UNUSED(c);
	}
	else if(auto *d = qobject_cast<Database*>(o)) {
		Q_UNUSED(d);
	}
}

void MainWindow::treeNodeDoubleClicked(const QModelIndex &index)
{
	/// double click pripojuje/odpojuje
	Ui::ServerTreeWidget &ui_srv = serverDock->ui;
	auto *model = qobject_cast<const ServerTreeModel*>(index.model());
	QObject *o = model->index2object(index);
	if(o) {
		//if(activeConnection.isOpen()) queryModel->clear();
		if(auto *c = qobject_cast<Connection*>(o)) {
			// pokud ma deti, je pripojen, tak at se odpoji
			if(c->isOpen()) {
				/// vymaz vsechny deti
				qfDebug() << QF_FUNC_NAME << c;
				ui_srv.treeServers->setExpanded(index, false);
				c->close();
				setActiveConnection1(qf::core::sql::Connection());
			}
			else {
				/// pripoj se, zjisti, jaky tam jsou database a otevri tu z connection
				Database* d = c->open();
				if(d) setActiveConnection2(d);
				// open first database in list to show its tables in tree
				QModelIndex ix = index.model()->index(0, 0, index);
				ui_srv.treeServers->setExpanded(ix, true);
			}
		}
		else if(auto *d = qobject_cast<Database*>(o)) {
			if(d->isOpen()) {
				d->close();
				setActiveConnection1(qf::core::sql::Connection());
				//ui.treeServers->close(index);
				ui_srv.treeServers->setExpanded(index, false);
			}
			else {
				d->open();
				setActiveConnection2(d);
				// zakomentovani tohoto radku paradoxne zpusobilo, ze aplikace se chova, jako kdyby tam byl.
				// Jeho pritomnost naopak zpusobi, ze se po otevreni databaze tabulky nerozevrou
				//ui_srv.treeServers->setExpanded(index, true);
			}
		}
		else if(auto *s = qobject_cast<Schema*>(o)) {
			Database *d = s->database();
			Q_ASSERT(d != nullptr);
			setActiveConnection2(d);
			Q_ASSERT(activeConnection().isOpen());
			if(s->isOpen()) {
				s->close();
				//ui.treeServers->close(index);
				ui_srv.treeServers->setExpanded(index, false);
			}
			else {
				s->open();
				// zakomentovani tohoto radku paradoxne zpusobilo, ze aplikace se chova, jako kdyby tam byl.
				// Jeho pritomnost naopak zpusobi, ze se po otevreni databaze tabulky nerozevrou
				//ui_srv.treeServers->setExpanded(index, true);
				setDbSearchPath(s->objectName());
			}
			ui_srv.treeServers->resizeColumnToContents(0);
		}
		else if(auto *t = qobject_cast<Table*>(o)) {
			Database *d = t->database();
			Q_ASSERT(d != nullptr);
			setActiveConnection2(d);
			//qfDebug() << "Table double clicked" << activeConnection().info();
			Q_ASSERT(activeConnection().isOpen());
			QString s;
			/*
			s = t->objectName();
			QSqlIndex ix = activeConnection().primaryIndex(s);
			s = "PRI INDEX: ";
			for(int i=0; i<ix.count(); i++) {
				s += ix.fieldName(i);
				s += (ix.isDescending(i))? " DESC ": " ASC ";
			}
			s = s.slice(0, -1);
			*/
			// @3 ui.lblInfo->setText(s);
			// @4 ui.lblRW->setText(ix.count()? "RW": "RO");
			s = t->schema();
			QString table_name = t->objectName();
			if(activeConnection().driverName().endsWith("MYSQL")) table_name = "`" + table_name + "`";
			if(s.isEmpty()) s = table_name;
			else s = s + "." + table_name;
			s = "SELECT * FROM " + s;
			if(edLimit->value() >= 0) s += " LIMIT " + QString::number(edLimit->value());
			execQuery(s);
			setDbSearchPath(t->parent()->objectName());
			//queryModel->setQuery(s, activeConnection);
			// @5 ui.lblRowCnt->setText(QString("%1 rows").arg(queryModel->rowCount()));
		}
	}
}

void MainWindow::treeServersContextMenuRequest(const QPoint& point)
{
	Ui::ServerTreeWidget &ui_srv = serverDock->ui;
	QModelIndex mi = ui_srv.treeServers->indexAt(point);
	{
		if(!mi.isValid()) {
			// Server popup
			QMenu menu(this);
			menu.setTitle(tr("Server menu"));
			menu.addAction(action("addServer"));
			menu.exec(ui_srv.treeServers->viewport()->mapToGlobal(point));
		}
		else {
			auto *model = qobject_cast<ServerTreeModel*>(ui_srv.treeServers->model());
			Q_ASSERT(model != nullptr);
			QObject *o = model->index2object(mi);
			Q_ASSERT(o != nullptr);
			if(auto *connection = qobject_cast<Connection*>(o)) {
				QMenu menu(this);
				menu.setTitle(tr("Connection menu"));
				menu.addAction(action("addServer"));
				QAction *act_editConnection = menu.addAction(tr("Edit connection"));
				QAction *act_copyConnection = menu.addAction(tr("Copy connection"));
				QAction *act_removeConnection = menu.addAction(tr("Remove connection"));
				QAction *a = menu.exec(ui_srv.treeServers->viewport()->mapToGlobal(point));
				if(a == act_editConnection) {
					DlgEditConnection dlg(this);
					dlg.setParams(connection->params());
					if(dlg.exec() == QDialog::Accepted) {
						connection->setParams(dlg.params());
					}
				}
				else if(a == act_copyConnection) {
					addServer(connection);
				}
				else if(a == act_removeConnection) {
					if(qf::qmlwidgets::dialogs::MessageBox::askYesNo(this, tr("Delete connection ?"), false)) {
						qf::qmlwidgets::dialogs::MessageBox::showError(this, "NIY");
						/* old impl
						QObject *o = model->take(mi);
						Q_ASSERT(o == connection);
						QFDomElement el = connection->m_params.parentNode().toElement();
						el.removeChild(connection->m_params);//.toElement();
						o->deleteLater();
						theApp()->config()->setDataDirty(true);
						*/
					}
				}
			}
			else if(auto *d = qobject_cast<Database*>(o)) {
				QMenu menu(this);
				menu.setTitle(tr("Database menu"));
				QAction *actCreateTable = menu.addAction(tr("Create schema"));
				QAction *a = menu.exec(ui_srv.treeServers->viewport()->mapToGlobal(point));
				if(a == actCreateTable) {
					QString s = QInputDialog::getText(this, tr("Query"), tr("Enter schema name"), QLineEdit::Normal);
					if(!s.isEmpty()) {
						bool ok = false;
						s = s.trimmed();
						if(!s.isEmpty()) {
							if(activeConnection().driverName().endsWith("MYSQL")) {
								QString qs = "CREATE DATABASE " + s;
								ok = execCommand(qs);
							}
							if(ok) {
								new Schema(d, s);
								ui_srv.treeServers->setExpanded(mi, false);
								ui_srv.treeServers->setExpanded(mi, true);
							}
						}
					}
				}
			}
			else if(auto *current_schema = qobject_cast<Schema*>(o)) {
				Q_UNUSED(current_schema);
				QMenu menu(this);
				menu.setTitle(tr("Schema menu"));
				QAction *actCreateTable = menu.addAction(tr("Create table"));
				menu.addSeparator();
				QAction *actCreateDatabaseScript = menu.addAction(tr("Create database script"));
				QAction *actDumpDatabaseScript = menu.addAction(tr("Dump database script"));
				menu.addSeparator();
				QAction *actDropSchema = menu.addAction(tr("Drop schema"));
				QAction *actCheckDatabase = menu.addAction(tr("Check database"));
				QAction *a = menu.exec(ui_srv.treeServers->viewport()->mapToGlobal(point));
				if(a == actCreateTable) {
					QString s = QInputDialog::getText(this, tr("Query"), tr("Enter table name"));
					if(!s.isEmpty()) {
						s = s.trimmed();
						if(!s.isEmpty()) {
							QString s1 = current_schema->objectName() + "." + s;
							QString qs = "UNSUPPORTED DRIVER " + activeConnection().driverName();
							if(activeConnection().driverName().endsWith("SQLITE")) {
								qs = "CREATE TABLE " + s1 + " (id integer PRIMARY KEY AUTOINCREMENT);";
							}
							else if(activeConnection().driverName().endsWith("PSQL")) {
								qs = "CREATE TABLE " + s1 + " (id serial PRIMARY KEY);";
							}
							else if(activeConnection().driverName().endsWith("MYSQL")) {
								qs = "CREATE TABLE " + s1 + " (id INT PRIMARY KEY AUTO_INCREMENT);";
							}
							bool ok = execCommand(qs);
							if(ok) {
								auto *t = new Table(nullptr, s, QSql::Tables);
								model->append(t, mi);
								ui_srv.treeServers->setExpanded(mi, false);
								ui_srv.treeServers->setExpanded(mi, true);
							}
						}
					}
				}
				else if(a == actCreateDatabaseScript) {
					qf::core::sql::Connection dbi(activeConnection());
					QString s = dbi.createSchemaSqlCommand(current_schema->objectName(), false);
					qf::qmlwidgets::dialogs::PreviewDialog dlg(this);
					new QFSqlSyntaxHighlighter(dlg.editor());
					dlg.exec(s, "create_" + current_schema->objectName() +".sql", "dlgTextView");
				}
				else if(a == actDumpDatabaseScript) {
					QString s = current_schema->createScript(Schema::CreateTableSql | Schema::DumpTableSql | Schema::IncludeViews);
					qfInfo() << s;
					qf::qmlwidgets::dialogs::MessageBox::showError(this, "NIY");
					/*
					QString fn = QFFileUtils::joinPath(qfApp()->tempDir(), sch->objectName() + ".sql");
					QUrl url = QFFileUtils::saveText(s, fn);
					if(!url.isEmpty()) QFDlgOpenUrl::openUrl(url);
					else QFMessage::error(tr("Chyba pri ukladani docasneho souboru '%1'").arg(fn));
					*/
				}
				else if(a == actDropSchema) {
					if(qf::qmlwidgets::dialogs::MessageBox::askYesNo(this, tr("Realy drop schema '%1'").arg(current_schema->objectName()), true)) {
						QString qs = "DROP SCHEMA " + current_schema->objectName();
						qf::core::sql::Connection c = activeConnection();
						if(c.driverName().endsWith("PSQL", Qt::CaseInsensitive)) {
							qs += " CASCADE";
						}
						execCommand(qs);
						QModelIndex parix = mi.parent();
						QObject *o = model->take(mi);
						o->deleteLater();
						ui_srv.treeServers->setExpanded(parix, false);
						ui_srv.treeServers->setExpanded(parix, true);
					}
				}
				else if(a == actCheckDatabase) {
					qf::qmlwidgets::dialogs::MessageBox::showError(this, "NIY");
					/*
					sch->open();
					QList<Table*> lst = sch->findChildren<Table*>();
					QStringList sl;
					QFBasicTable::TextExportOptions opts;
					opts.setFieldSeparator("\t");
					opts.setExportColumnNames(false);
					foreach(Table *t, lst) {
						QString tbl_name = t->objectName();
						//qfInfo() << tbl_name << t;
						QString qs = "CHECK TABLE " + tbl_name;
						execQuery(qs);
						QString line;
						{
							QTextStream ts(&line);
							queryViewModel()->table()->exportCSV(ts, "*", opts);
							QApplication::processEvents();
						}
						//qfInfo() << line;
						sl << line.trimmed().split('\t').join("</td><td>");
					}
					QString s = "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">"
							"<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"en\" lang=\"en\">"
							"<head><meta http-equiv=\"Content-Type\" content=\"text/html;charset=utf-8\" /><title>" + sch->objectName() + " database check</title></head>"
							"<body><table border=\"1\">\n<tr><td>" + sl.join("</td></tr>\n<tr><td>") + "</td></tr>\n</table></body></html>";
					QFDlgHtmlView::exec(this, s, "check_" + sch->objectName() +".html", "dlgHtmlView");
					*/
				}
			}
			else if(auto *table = qobject_cast<Table*>(o)) {
				auto *current_schema = qobject_cast<Schema*>(table->parent());
				QF_ASSERT(current_schema != nullptr, "Parent schema is NULL", return);
				QString full_table_name = current_schema->objectName() + '.' + table->objectName();

				QMenu menu(this);
				menu.setTitle(tr("Table menu"));
				QAction *actDescribeTable = menu.addAction(tr("Show columns"));
				QAction *actShowIndexes = menu.addAction(tr("Show indexes"));
				QAction *actShowFKeys = menu.addAction(tr("Show foreign keys"));
				QAction *actCreateScript = menu.addAction(tr("Create table script"));
				QAction *actDumpScript = menu.addAction(tr("Dump table script"));
				menu.addSeparator();
				QAction *actColumnSelector = menu.addAction(tr("Column selector"));
				menu.addSeparator();
				QAction *actAlterTable = menu.addAction(tr("Alter table"));
				QAction *actDropTable = menu.addAction(tr("Drop table"));
				QAction *actTruncateTable = menu.addAction(tr("Truncate table"));
				QAction *actRenameTable = menu.addAction(tr("Rename table"));
				QAction *actMysqlCheckTable = nullptr;
				QAction *actMysqlRepairTable = nullptr;
				QAction *actMysqlOptimizeTable = nullptr;
				if(activeConnection().driverName().endsWith("MYSQL")) {
					menu.addSeparator();
					QMenu *mysql_m = menu.addMenu("MySQL");
					actMysqlCheckTable = mysql_m->addAction(tr("Check table"));
					actMysqlRepairTable = mysql_m->addAction(tr("Repair table"));
					actMysqlOptimizeTable = mysql_m->addAction(tr("Optimize table"));
				}
				QAction *a = menu.exec(ui_srv.treeServers->viewport()->mapToGlobal(point));
				if(!a) {
					return;
				}
				if(a == actDescribeTable) {
					if(activeConnection().driverName().endsWith("SQLITE")) {
						QString s = "PRAGMA table_info(%1)";
						execQuery(s.arg(table->objectName()));
					}
					else if(activeConnection().driverName().endsWith("PSQL")) {
						QString s = "SELECT"
								" column_name AS name,"
								" data_type AS type,"
								" is_nullable AS null,"
								" column_default AS default"
								" FROM information_schema.columns"
								" WHERE table_name = '%1'"
								"  AND table_schema = '%2'"
								" ORDER BY ordinal_position";
						execQuery(s.arg(table->objectName()).arg(table->schema()));
					}
					else if(activeConnection().driverName().endsWith("MYSQL")) {
						QString s = "SHOW FULL COLUMNS FROM %1";
						execQuery(s.arg(full_table_name));
					}
				}
				else if(a == actShowIndexes) {
					if(activeConnection().driverName().endsWith("SQLITE")) {
						QString s = "PRAGMA index_list(%1)";
						execQuery(s.arg(table->objectName()));
					}
					else if(activeConnection().driverName().endsWith("PSQL")) {
						QString s = "SELECT indexes.relname AS indexname, indisprimary AS isprimary, indisunique AS isunique, columns.attname AS colname"
								" FROM pg_index LEFT JOIN pg_class AS indexes ON pg_index.indexrelid = indexes.oid"
								" LEFT JOIN pg_attribute AS columns ON columns.attrelid = pg_index.indrelid"
								" WHERE pg_index.indrelid='%1.%2'::regclass AND columns.attnum = ANY (indkey)";
						execQuery(s.arg(table->parent()->objectName()).arg(table->objectName()));
					}
					else if(activeConnection().driverName().endsWith("MYSQL")) {
						QString s = "SHOW INDEXES FROM  %1";
						execQuery(s.arg(full_table_name));
					}
				}
				else if(a == actShowFKeys) {
					if(activeConnection().driverName().endsWith("SQLITE")) {
						QString s = "PRAGMA foreign_key_list(%1)";
						execQuery(s.arg(table->objectName()));
					}
					else if(activeConnection().driverName().endsWith("PSQL")) {
						QString s = "SELECT conname AS constrname, fromcols.attname AS fkeys, tocols.attname AS referncedfields,"
								" CASE WHEN confupdtype='r' THEN 'RESTRICT'"
								" WHEN confupdtype='c' THEN 'CASCADE'"
								" ELSE 'NO ACTION'"
								" END  AS onupdate,"
								" CASE WHEN confdeltype='r' THEN 'RESTRICT'"
								" WHEN confdeltype='c' THEN 'CASCADE'"
								" ELSE 'NO ACTION'"
								" END  AS ondelete,"
								" confmatchtype AS onmatch"
								" FROM pg_constraint AS cc"
								" LEFT JOIN pg_attribute AS fromcols ON cc.conrelid=fromcols.attrelid"
								" LEFT JOIN pg_attribute AS tocols ON cc.conrelid=tocols.attrelid"
								" WHERE cc.contype = 'f' AND fromcols.attnum = ANY(cc.conkey) AND tocols.attnum = ANY(cc.confkey)"
								"  AND cc.conrelid='%1'::regclass";
						execQuery(s.arg(full_table_name));
					}
					else if(activeConnection().driverName().endsWith("MYSQL")) {
						qf::qmlwidgets::dialogs::MessageBox::showInfo(this, tr("Not implemented yet."));
					}
				}
				else if(a == actCreateScript) {
					qf::core::sql::Connection dbi(activeConnection());
					QString s = dbi.createTableSqlCommand(full_table_name);
					qf::qmlwidgets::dialogs::PreviewDialog dlg(this);
					new QFSqlSyntaxHighlighter(dlg.editor());
					dlg.exec(s, "create_" + table->objectName()+".sql", "dlgTextView");
				}
				else if(a == actDumpScript) {
					qf::core::sql::Connection dbi(activeConnection());
					QString s = dbi.createTableSqlCommand(full_table_name);
					s += "\n\n";
					s += dbi.dumpTableSqlCommand(full_table_name);
					qf::qmlwidgets::dialogs::PreviewDialog dlg(this);
					new QFSqlSyntaxHighlighter(dlg.editor());
					dlg.exec(s, "dump_" + table->objectName()+".sql", "dlgTextView");
				}
				else if(a == actColumnSelector) {
					qf::core::sql::Connection conn = activeConnection();
					auto *w = new ColumnSelectorWidget(table->objectName(), conn);
					qf::qmlwidgets::dialogs::Dialog dlg(this);
					dlg.setCentralWidget(w);
					connect(w, SIGNAL(columnNamesCopiedToClipboard(QString)), sqlDock->sqlTextEdit(), SLOT(paste()));
					if(dlg.exec() == QDialog::Accepted) {}
				}
				else if(a == actAlterTable) {
					auto *dlg = new DlgAlterTable(this, table->parent()->objectName(), table->objectName());
					dlg->show();
				}
				else if(a == actRenameTable) {
					QString s = QInputDialog::getText(this, tr("Query"), tr("Enter new table name for table %1").arg(table->objectName()));
					if(!s.isEmpty()) {
						s = s.trimmed();
						if(!s.isEmpty()) {
							QString qs = "ALTER TABLE " + table->objectName() + " RENAME TO " + s;
							execCommand(qs);
							table->setObjectName(s);
						}
					}
				}
				else if(a == actDropTable) {
					QString s = table->objectName();
					QString what = "TABLE";
					if(table->kind == QSql::Views) what = "VIEW";
					if(qf::qmlwidgets::dialogs::MessageBox::askYesNo(this, tr("Realy drop the table '%1'").arg(s), true)) {
						s = "DROP " + what + " " + s;
						execCommand(s);
						QModelIndex parix = mi.parent();
						QObject *o = model->take(mi);
						o->deleteLater();
						ui_srv.treeServers->setExpanded(parix, false);
						ui_srv.treeServers->setExpanded(parix, true);
					}
				}
				else if(a == actTruncateTable) {
					if(table->kind == QSql::Tables) {
						if(qf::qmlwidgets::dialogs::MessageBox::askYesNo(this, tr("Realy truncate the table '%1'").arg(full_table_name), true)) {
							if(activeConnection().driverName().endsWith("SQLITE")) {
								QString s = "DELETE FROM " + full_table_name;
								execCommand(s);
								s = "VACUUM";
								execCommand(s);
							}
							else {
								QString s = "TRUNCATE TABLE " + full_table_name;
								execCommand(s);
							}
						}
					}
				}
				else if(a == actMysqlCheckTable) {
					QString s = full_table_name;
					s = "CHECK TABLE " + s;
					execQuery(s);
					//QFDlgTextView dlg(this);
					//dlg.exec(s, "check_" + table->objectName()+".txt", "dlgTextView");
				}
				else if(a == actMysqlOptimizeTable) {
					QString s = full_table_name;
					s = "OPTIMIZE TABLE " + s;
					execQuery(s);
				}
				else if(a == actMysqlRepairTable) {
					QString s = full_table_name;
					s = "REPAIR TABLE " + s;
					execQuery(s);
				}
			}
		}
	}
}

//-------------------------------------------------------------------------
void MainWindow::addServer(Connection *connection_to_copy)
{
	//qf::qmlwidgets::dialogs::MessageBox::showError(this, "NIY");

	Ui::ServerTreeWidget &ui_srv = serverDock->ui;
	auto *model = qobject_cast<ServerTreeModel*>(ui_srv.treeServers->model());
	DlgEditConnection dlg(this);
	if(connection_to_copy)
		dlg.setParams(connection_to_copy->params());
	if(dlg.exec() == QDialog::Accepted) {
		auto *c = new Connection(dlg.params());
		//theApp()->config()->dataDocument().cd("/servers").appendChild(c->m_params);
		//theApp()->config()->setDataDirty(true);
		model->append(c, QModelIndex());
	}

}

void MainWindow::changeLog()
{
	qf::qmlwidgets::dialogs::MessageBox::showError(this, "NIY");
	/*
	QFile f(":/ChangeLog");
	QFDlgHtmlView::exec(this, f, "dlgChangeLog");
	*/
}

void MainWindow::mysqlSyntax()
{
	QUrl url = QUrl::fromLocalFile(":/doc/syntax/mysqlsyntax.html");
	qf::qmlwidgets::dialogs::PreviewDialog::exec(this, url, "dlgSqlSyntax");
}

void MainWindow::sqliteSyntax()
{
	QUrl url = QUrl::fromLocalFile(":/doc/syntax/sqlitesyntax.html");
	qf::qmlwidgets::dialogs::PreviewDialog::exec(this, url, "dlgSqlSyntax");
}

void MainWindow::availableDrivers()
{
	QString s;
	{
		QTextStream ts(&s);
		#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
		ts << "QT build key:" << QLibraryInfo::buildKey() << endl << endl;
		#endif
		ts << "Available drivers:" << '\n';
		QStringList sl = qf::core::sql::Connection::drivers();
		foreach(QString s, sl) {
			ts << "\t'" << s << "'" << '\n';
		}
	}
	qf::qmlwidgets::dialogs::PreviewDialog::exec(this, s);
}

void MainWindow::checkDrivers()
{
	QString msg;
	QTextStream ts(&msg);
	{
		for(QString plugin_dir : QCoreApplication::libraryPaths()) {
			qfInfo() << "plugin dir:" << plugin_dir;
			QString path = qf::core::utils::FileUtils::joinPath(plugin_dir, "sqldrivers");
			ts << tr("Plugins found (looked in %1):").arg(QDir::toNativeSeparators(path)) << '\n';
			QDir dir(path);
			dir.setFilter(QDir::Files);
			//dir.setNameFilters(QStringList() << "*.dll");
			QStringList file_names = dir.entryList();
			foreach (QString file_name, file_names) {
				ts << tr("checking: ") << file_name << ": ";
				QPluginLoader loader(dir.absoluteFilePath(file_name));
				QObject *plugin = loader.instance();
				if(plugin) {
					ts << tr("OK ") << '\n';
				}
				else {
					ts << tr("ERROR ") << '\n';
					ts << loader.errorString() << '\n';
				}
			}
		}
	}
	qf::qmlwidgets::dialogs::PreviewDialog::exec(this, msg);
}

void MainWindow::tearOffTable()
{
	auto *dlg = new QDialog();
	dlg->setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint);
	tearOffDialogs << dlg;
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	QBoxLayout *ly = new QHBoxLayout(dlg);
	TableViewWidget *w = ui.queryView;
	ly->addWidget(w);
	dlg->resize(w->size());
	dlg->show();

	ly = qobject_cast<QBoxLayout*>(ui.frmTable->layout());
	if(ly) {
		ui.queryView = new TableViewWidget();
		//ui.queryView->setContextMenuPolicy(Qt::ActionsContextMenu);
		//ui.queryView->setContextMenuPolicy();
		connect(ui.queryView, &TableViewWidget::statusBarAction, this, &MainWindow::onTableStatusBarAction);
		ly->addWidget(ui.queryView);
		auto *m = new qf::core::model::SqlTableModel(ui.queryView);
		m->setConnectionName(activeConnection().connectionName());
		setQueryViewModel(m);
	}
}

void MainWindow::closeEvent(QCloseEvent *e)
{
	Q_UNUSED(e);
	foreach(QDialog *dlg, tearOffDialogs)
		QF_SAFE_DELETE(dlg);
}
/*
void MainWindow::showEvent(QShowEvent * e)
{
	qfDebug() << QF_FUNC_NAME;
}

void MainWindow::focusInEvent(QFocusEvent *e)
{
	qfDebug() << QF_FUNC_NAME;
}
*/
void MainWindow::changeEvent(QEvent * e)
{
	if(!e) return;
	if(e->type() == QEvent::ActivationChange) {
		qfDebug() << QF_FUNC_NAME << "new state:" << windowState();
		qfDebug() << "\t is active window:" << isActiveWindow();
		//raise();
	}
}

void MainWindow::onTableStatusBarAction(const QString &_text)
{
	QString text = _text.trimmed();
	if(!text.isEmpty()) {
		sqlDock->sqlTextEdit()->append(text);
	}
}

void MainWindow::setProgressValue(double val, const QString & label_text)
{
	auto *sb = qobject_cast<QFStatusBar*>(statusBar());
	if(sb) sb->setProgressValue(val, label_text);
}

void MainWindow::showSqlJournal()
{
	qf::qmlwidgets::dialogs::MessageBox::showError(this, "NIY");
	/*
	QString s = theApp()->sqlJournal()->content().join("\n");
	QFDlgTextView dlg(this);
	new QFSqlSyntaxHighlighter(dlg.editor());
	if(dlg.exec(s, "journal.sql", "dlgTextView")) {
		s = dlg.text();
		theApp()->sqlJournal()->setContent(s.split('\n'));
	}
	*/
}

void MainWindow::setSqlDelimiter()
{
	QString s = f_sqlDelimiter;
	bool ok;
	s = QInputDialog::getText(this, tr("Get delimiter"), tr("SQL commands delimiter (one character):"), QLineEdit::Normal, s, &ok);
	if(ok && !s.isEmpty()) f_sqlDelimiter = s[0];
}

void MainWindow::loadPersistentSettings()
{
	QString path = persistentSettingsPath();
	qfLogFuncFrame() << path;
	if(!path.isEmpty()) {
		QSettings settings;
		settings.beginGroup(path);
		restoreGeometry(settings.value("geometry").toByteArray());
		restoreState(settings.value("state").toByteArray());
	}
}

void MainWindow::savePersistentSettings()
{
	QString path = persistentSettingsPath();
	qfLogFuncFrame() << path;
	if(!path.isEmpty()) {
		QSettings settings;
		settings.beginGroup(path);
		settings.setValue("state", saveState());
		settings.setValue("geometry", saveGeometry());
	}
}
