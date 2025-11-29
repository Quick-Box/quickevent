add_library(libsiut SHARED
    src/device/crc529.c
    src/device/commport.cpp
    src/device/sidevicedriver.cpp
    src/device/sitask.cpp
    src/message/simessagedata.cpp
    src/sicard.cpp
    src/sipunch.cpp
    )

qt6_add_translation(QM_FILES
	libsiut_cs_CZ.ts
	libsiut_fr_FR.ts
	libsiut_nb_NO.ts
	libsiut_nl_BE.ts
	libsiut_pl_PL.ts
	libsiut_ru_RU.ts
	libsiut_uk_UA.ts
	)

qt6_add_lupdate(libsiut TS_FILES
	libsiut_cs_CZ.ts
	libsiut_fr_FR.ts
	libsiut_nb_NO.ts
	libsiut_nl_BE.ts
	libsiut_pl_PL.ts
	libsiut_ru_RU.ts
	libsiut_uk_UA.ts
	)

target_sources(libsiut PRIVATE ${QM_FILES})
target_compile_definitions(libsiut PRIVATE SIUT_BUILD_DLL)
target_link_libraries(libsiut PUBLIC Qt::Core Qt::SerialPort libqfcore)
target_include_directories(libsiut PUBLIC include)

install(TARGETS libsiut)
install(FILES ${QM_FILES} DESTINATION ${CMAKE_INSTALL_BINDIR}/translations)
