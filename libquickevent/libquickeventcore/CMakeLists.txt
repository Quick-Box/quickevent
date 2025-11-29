add_library(libquickeventcore SHARED
    src/codedef.cpp
    src/coursedef.cpp
    src/exporters/fileexporter.cpp
    src/exporters/htmlfileexporter.cpp
    src/exporters/stageresultscsvexporter.cpp
    src/exporters/stageresultshtmlexporter.cpp
    src/exporters/stagestartlisthtmlexporter.cpp
    src/og/timems.cpp
    src/runstatus.cpp
    src/si/checkedcard.cpp
    src/si/checkedpunch.cpp
    src/si/punchrecord.cpp
    src/si/readcard.cpp
    src/si/siid.cpp
    src/utils.cpp
	libquickeventcore.qrc
    )

qt6_add_translation(QM_FILES
	libquickeventcore_cs_CZ.ts
	libquickeventcore_fr_FR.ts
	libquickeventcore_nb_NO.ts
	libquickeventcore_nl_BE.ts
	libquickeventcore_pl_PL.ts
	libquickeventcore_ru_RU.ts
	libquickeventcore_uk_UA.ts
	)

qt6_add_lupdate(libquickeventcore TS_FILES
	libquickeventcore_cs_CZ.ts
	libquickeventcore_fr_FR.ts
	libquickeventcore_nb_NO.ts
	libquickeventcore_nl_BE.ts
	libquickeventcore_pl_PL.ts
	libquickeventcore_ru_RU.ts
	libquickeventcore_uk_UA.ts
	)

target_sources(libquickeventcore PRIVATE ${QM_FILES})
target_compile_definitions(libquickeventcore PRIVATE QUICKEVENTCORE_BUILD_DLL)
target_link_libraries(libquickeventcore PUBLIC Qt::Core Qt::Sql libsiut)
target_include_directories(libquickeventcore PUBLIC include)

install(TARGETS libquickeventcore)
install(FILES ${QM_FILES} DESTINATION ${CMAKE_INSTALL_BINDIR}/translations)
