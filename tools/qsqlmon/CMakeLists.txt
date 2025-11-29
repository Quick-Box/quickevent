add_executable(qsqlmon
    src/columnselectorwidget.cpp
    src/dlgaltertable.cpp
    src/dlgcolumndef.cpp
    src/dlgeditconnection.cpp
    src/dlgindexdef.cpp
    src/driver/qfhttpmysql/qfhttpmysql.cpp
    src/main.cpp
    src/mainwindow.cpp
    src/qfobjectitemmodel.cpp
    src/qfsqlsyntaxhighlighter.cpp
    src/qfstatusbar.cpp
    src/servertreedock.cpp
    src/servertreeitem.cpp
    src/servertreemodel.cpp
    src/servertreeview.cpp
    src/sqldock.cpp
    src/sqltextedit.cpp
    src/tableviewwidget.cpp
	src/theapp.cpp
	src/qsqlmon.qrc
	src/qsqlmon.rc
	src/centralwidget.ui
	src/columnselectorwidget.ui
	src/dlgaltertable.ui
	src/dlgcolumndef.ui
	src/dlgeditconnection.ui
	src/dlgindexdef.ui
	src/servertreewidget.ui
	src/sqlwidget.ui
	src/tableviewwidget.ui
    )

qt6_add_translation(QM_FILES
	qsqlmon_cs_CZ.ts
	qsqlmon_fr_FR.ts
	qsqlmon_nb_NO.ts
	qsqlmon_nl_BE.ts
	qsqlmon_pl_PL.ts
	qsqlmon_ru_RU.ts
	qsqlmon_uk_UA.ts
	)

qt6_add_lupdate(qsqlmon TS_FILES
	qsqlmon_cs_CZ.ts
	qsqlmon_fr_FR.ts
	qsqlmon_nb_NO.ts
	qsqlmon_nl_BE.ts
	qsqlmon_pl_PL.ts
	qsqlmon_ru_RU.ts
	qsqlmon_uk_UA.ts
	)

target_sources(qsqlmon PRIVATE ${QM_FILES})
target_link_libraries(qsqlmon PRIVATE libqfgui)
target_include_directories(qsqlmon PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

install(TARGETS qsqlmon)
install(FILES ${QM_FILES} DESTINATION ${CMAKE_INSTALL_BINDIR}/translations)
