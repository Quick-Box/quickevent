add_executable(quickshow
    src/appclioptions.cpp
    src/application.cpp
    src/cellrenderer.cpp
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.ui
    src/model.cpp
    src/table.cpp
    quickshow.rc
    )

qt6_add_translation(QM_FILES
	quickshow_cs_CZ.ts
	quickshow_fr_FR.ts
	quickshow_nb_NO.ts
	quickshow_nl_BE.ts
	quickshow_pl_PL.ts
	quickshow_ru_RU.ts
	quickshow_uk_UA.ts
	)

qt6_add_lupdate(quickshow TS_FILES
	quickshow_cs_CZ.ts
	quickshow_fr_FR.ts
	quickshow_nb_NO.ts
	quickshow_nl_BE.ts
	quickshow_pl_PL.ts
	quickshow_ru_RU.ts
	quickshow_uk_UA.ts
	)

target_sources(quickshow PRIVATE ${QM_FILES})
target_link_libraries(quickshow PRIVATE Qt::Widgets Qt::Sql libqfcore)
target_include_directories(quickshow PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

install(TARGETS quickshow)
install(FILES ${QM_FILES} DESTINATION ${CMAKE_INSTALL_BINDIR}/translations)
