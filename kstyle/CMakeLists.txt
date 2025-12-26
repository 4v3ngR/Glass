################# includes #################
include_directories(
  animations
  debug
)

include_directories(${CMAKE_SOURCE_DIR}/libglasscommon)
include_directories(${CMAKE_BINARY_DIR}/libglasscommon)

################# configuration #################
configure_file(config-glass.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-glass.h)

########### next target ###############
set(glass_PART_SRCS
    animations/glassanimation.cpp
    animations/glassanimations.cpp
    animations/glassanimationdata.cpp
    animations/glassbaseengine.cpp
    animations/glassbusyindicatordata.cpp
    animations/glassbusyindicatorengine.cpp
    animations/glassdialdata.cpp
    animations/glassdialengine.cpp
    animations/glassenabledata.cpp
    animations/glassgenericdata.cpp
    animations/glassheaderviewdata.cpp
    animations/glassheaderviewengine.cpp
    animations/glassscrollbardata.cpp
    animations/glassscrollbarengine.cpp
    animations/glassspinboxengine.cpp
    animations/glassspinboxdata.cpp
    animations/glassstackedwidgetdata.cpp
    animations/glassstackedwidgetengine.cpp
    animations/glasstabbarengine.cpp
    animations/glasstabbardata.cpp
    animations/glasstoolboxengine.cpp
    animations/glasstransitiondata.cpp
    animations/glasstransitionwidget.cpp
    animations/glasswidgetstateengine.cpp
    animations/glasswidgetstatedata.cpp
    debug/glasswidgetexplorer.cpp
    glassaddeventfilter.cpp
    glassblurhelper.cpp
    glassframeshadow.cpp
    glasshelper.cpp
    glassmdiwindowshadow.cpp
    glassmnemonics.cpp
    glasspropertynames.cpp
    glassshadowhelper.cpp
    glasssplitterproxy.cpp
    glassstyle.cpp
    glassstyleplugin.cpp
    glasstileset.cpp
    glasswindowmanager.cpp
    glasstoolsareamanager.cpp
)

kconfig_add_kcfg_files(glass_PART_SRCS ../kdecoration/glasssettings.kcfgc)
kconfig_add_kcfg_files(glass_PART_SRCS glassstyleconfigdata.kcfgc)

add_library(glass${QT_MAJOR_VERSION} MODULE ${glass_PART_SRCS})

################# find Qt modules #################
if(QT_MAJOR_VERSION EQUAL 6)
    find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Quick QuickWidgets)
else()
    find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets Quick QuickWidgets)
endif()

target_link_libraries(glass${QT_MAJOR_VERSION}
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Widgets
    Qt${QT_MAJOR_VERSION}::DBus
    Qt${QT_MAJOR_VERSION}::Quick
		Qt${QT_MAJOR_VERSION}::QuickWidgets
)

if( GLASS_HAVE_QTQUICK )
    target_link_libraries(glass${QT_MAJOR_VERSION}
        Qt${QT_MAJOR_VERSION}::Quick
        KF${QT_MAJOR_VERSION}::CoreAddons
    )
endif()

target_link_libraries(glass${QT_MAJOR_VERSION}
    KF${QT_MAJOR_VERSION}::CoreAddons
    KF${QT_MAJOR_VERSION}::ConfigCore
    KF${QT_MAJOR_VERSION}::ConfigGui
    KF${QT_MAJOR_VERSION}::GuiAddons
    KF${QT_MAJOR_VERSION}::IconThemes
    KF${QT_MAJOR_VERSION}::WindowSystem
)

if(QT_MAJOR_VERSION STREQUAL "5")
    target_link_libraries(glass5 KF5::ConfigWidgets)
    if (GLASS_HAVE_QTQUICK)
        target_link_libraries(glass5 KF5::Kirigami2)
    endif()
else()
    target_link_libraries(glass6 KF6::ColorScheme)
    if (GLASS_HAVE_QTQUICK)
        target_link_libraries(glass6 KF6::KirigamiPlatform)
    endif()
endif()


target_link_libraries(glass${QT_MAJOR_VERSION} glasscommon${QT_MAJOR_VERSION})

if(KF${QT_MAJOR_VERSION}FrameworkIntegration_FOUND)
    target_link_libraries(glass${QT_MAJOR_VERSION}  KF${QT_MAJOR_VERSION}::Style)
endif()


if (WIN32)
    # As stated in https://docs.microsoft.com/en-us/cpp/c-runtime-library/math-constants M_PI only gets defined
    # when if _USE_MATH_DEFINES is defined
    target_compile_definitions(glass PRIVATE _USE_MATH_DEFINES _BSD_SOURCE)
endif()


########### install files ###############
if (FOR_FLATPAK)
    set(LIB_TARGET ${QT_PLUGINS_DIR}/styles/)
else()
    set(LIB_TARGET ${KDE_INSTALL_QTPLUGINDIR}/styles/)
endif()
install(TARGETS glass${QT_MAJOR_VERSION} DESTINATION ${LIB_TARGET})

if (NOT FOR_FLATPAK)
    install(FILES glass.themerc  DESTINATION  ${KDE_INSTALL_DATADIR}/kstyle/themes)
endif()

########### subdirectories ###############

if (QT_MAJOR_VERSION EQUAL "6" AND TARGET "KF6::KCMUtils" AND NOT FOR_FLATPAK)
    add_subdirectory(config)
endif()
