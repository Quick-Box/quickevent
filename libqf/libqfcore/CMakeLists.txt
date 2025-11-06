add_library(libqfcore SHARED
    src/core/collator.cpp
    src/core/exception.cpp
    src/core/logentrymap.cpp
    src/core/stacktrace.cpp
    src/core/string.cpp
    src/core/utils.cpp
    src/network/networkaccessmanager.cpp
    src/network/networkreply.cpp
    src/sql/catalog.cpp
    src/sql/connection.cpp
    src/sql/dbenum.cpp
    src/sql/dbenumcache.cpp
    src/sql/query.cpp
    src/sql/querybuilder.cpp
    src/sql/transaction.cpp
    src/utils/clioptions.cpp
    src/utils/crypt.cpp
    src/utils/csvreader.cpp
    src/utils/fileutils.cpp
    src/utils/htmlutils.cpp
    src/utils/settings.cpp
    src/utils/table.cpp
    src/utils/timescope.cpp
    src/utils/treeitembase.cpp
    src/utils/treetable.cpp
    )

qt6_add_translation(QM_FILES
	libqfcore-cs_CZ.ts
	libqfcore-fr_FR.ts
	libqfcore-nb_NO.ts
	libqfcore-nl_BE.ts
	libqfcore-pl_PL.ts
	libqfcore-ru_RU.ts
	libqfcore-uk_UA.ts
	)

qt6_add_lupdate(libqfcore TS_FILES
	libqfcore-cs_CZ.ts
	libqfcore-fr_FR.ts
	libqfcore-nb_NO.ts
	libqfcore-nl_BE.ts
	libqfcore-pl_PL.ts
	libqfcore-ru_RU.ts
	libqfcore-uk_UA.ts
	)

target_sources(libqfcore PRIVATE ${QM_FILES})
target_link_libraries(libqfcore PUBLIC libnecrolog::libnecrolog Qt::Core Qt::Sql PRIVATE Qt::Gui Qt::Qml Qt::Xml)
target_include_directories(libqfcore PUBLIC include)
target_compile_definitions(libqfcore PRIVATE QFCORE_BUILD_DLL)

if(WIN32)
    # Need these for some WinAPI calls
    target_compile_definitions(libqfcore PRIVATE UNICODE)
    target_compile_definitions(libqfcore PRIVATE _UNICODE)
endif()

install(TARGETS libqfcore)
install(FILES ${QM_FILES} DESTINATION ${CMAKE_INSTALL_BINDIR}/translations)
