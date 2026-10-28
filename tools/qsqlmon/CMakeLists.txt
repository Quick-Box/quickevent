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
	qsqlmon-cs_CZ.ts
	qsqlmon-fr_FR.ts
	qsqlmon-nb_NO.ts
	qsqlmon-nl_BE.ts
	qsqlmon-pl_PL.ts
	qsqlmon-ru_RU.ts
	qsqlmon-es.ts
	qsqlmon-uk_UA.ts
	)

qt6_add_lupdate(qsqlmon TS_FILES
	qsqlmon-cs_CZ.ts
	qsqlmon-fr_FR.ts
	qsqlmon-nb_NO.ts
	qsqlmon-nl_BE.ts
	qsqlmon-pl_PL.ts
	qsqlmon-ru_RU.ts
	qsqlmon-es.ts
	qsqlmon-uk_UA.ts
	)

target_sources(qsqlmon PRIVATE ${QM_FILES})
target_link_libraries(qsqlmon PRIVATE libqfgui Qt::Network)
target_include_directories(qsqlmon PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(APPLE)
	install(TARGETS qsqlmon RUNTIME DESTINATION quickevent.app/Contents/MacOS)
	install(FILES ${QM_FILES} DESTINATION quickevent.app/Contents/MacOS/translations)
else()
	install(TARGETS qsqlmon)
	install(FILES ${QM_FILES} DESTINATION ${CMAKE_INSTALL_BINDIR}/translations)
endif()
