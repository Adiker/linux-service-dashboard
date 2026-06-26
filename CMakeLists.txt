cmake_minimum_required(VERSION 3.21)

project(linux-service-dashboard VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTORCC ON)

include(GNUInstallDirs)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Test DBus)

qt_standard_project_setup()

enable_testing()

set(SMART_HELPER_TARGET linux-service-dashboard-smart-helper)
if(IS_ABSOLUTE "${CMAKE_INSTALL_LIBEXECDIR}")
    set(SMART_HELPER_RELATIVE_PATH "${CMAKE_INSTALL_LIBEXECDIR}/${SMART_HELPER_TARGET}")
else()
    file(RELATIVE_PATH SMART_HELPER_RELATIVE_PATH
        "${CMAKE_INSTALL_FULL_BINDIR}"
        "${CMAKE_INSTALL_FULL_LIBEXECDIR}/${SMART_HELPER_TARGET}")
endif()

# Raw helper path (no surrounding quotes); the install-time set() below quotes it
# explicitly so paths with spaces survive, while the generated polkit policy still
# receives a bare filesystem path for org.freedesktop.policykit.exec.path.
if(IS_ABSOLUTE "${CMAKE_INSTALL_LIBEXECDIR}")
    set(_lsd_smart_helper_install_path "${CMAKE_INSTALL_LIBEXECDIR}/${SMART_HELPER_TARGET}")
else()
    set(_lsd_smart_helper_install_path "\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBEXECDIR}/${SMART_HELPER_TARGET}")
endif()

option(LSD_INSTALL_POLKIT_POLICY_SYSTEM
    "Install the polkit policy into /usr/share/polkit-1/actions instead of under CMAKE_INSTALL_PREFIX"
    OFF)

if(LSD_INSTALL_POLKIT_POLICY_SYSTEM)
    set(LSD_POLKIT_POLICY_DIR "/usr/share/polkit-1/actions" CACHE PATH
        "Directory where polkit loads action policy files (relative paths are rooted at the install prefix)" FORCE)
else()
    set(LSD_POLKIT_POLICY_DIR "${CMAKE_INSTALL_DATADIR}/polkit-1/actions" CACHE PATH
        "Directory where polkit loads action policy files (relative paths are rooted at the install prefix)" FORCE)
endif()

qt_add_executable(linux-service-dashboard
    resources/resources.qrc
    src/main.cpp
    src/AppIcon.cpp
    src/AppIcon.h
    src/MainWindow.cpp
    src/MainWindow.h
    src/core/CommandResult.h
    src/core/CommandRunner.cpp
    src/core/CommandRunner.h
    src/core/RefreshScheduler.cpp
    src/core/RefreshScheduler.h
    src/core/MountProfileStore.cpp
    src/core/MountProfileStore.h
    src/core/ServiceGroupSettings.cpp
    src/core/ServiceGroupSettings.h
    src/core/ModuleSettings.cpp
    src/core/ModuleSettings.h
    src/models/ServiceModel.cpp
    src/models/ServiceModel.h
    src/models/DockerContainerModel.cpp
    src/models/DockerContainerModel.h
    src/models/MountModel.cpp
    src/models/MountModel.h
    src/models/SensorModel.cpp
    src/models/SensorModel.h
    src/models/DiskModel.cpp
    src/models/DiskModel.h
    src/services/SystemdServiceProvider.cpp
    src/services/SystemdServiceProvider.h
    src/services/DockerProvider.cpp
    src/services/DockerProvider.h
    src/services/NetworkProvider.cpp
    src/services/NetworkProvider.h
    src/services/MountProvider.cpp
    src/services/MountProvider.h
    src/services/SensorProvider.cpp
    src/services/SensorProvider.h
    src/services/SmartProvider.cpp
    src/services/SmartProvider.h
    src/widgets/OverviewPage.cpp
    src/widgets/OverviewPage.h
    src/widgets/SystemdPage.cpp
    src/widgets/SystemdPage.h
    src/widgets/DockerPage.cpp
    src/widgets/DockerPage.h
    src/widgets/VpnPage.cpp
    src/widgets/VpnPage.h
    src/widgets/MountsPage.cpp
    src/widgets/MountsPage.h
    src/widgets/SensorsPage.cpp
    src/widgets/SensorsPage.h
    src/widgets/DisksPage.cpp
    src/widgets/DisksPage.h
    src/widgets/SettingsPage.cpp
    src/widgets/SettingsPage.h
    src/widgets/LogViewerDialog.cpp
    src/widgets/LogViewerDialog.h
    src/widgets/ConfirmActionDialog.cpp
    src/widgets/ConfirmActionDialog.h
    src/utils/JsonUtils.cpp
    src/utils/JsonUtils.h
    src/utils/TimeUtils.cpp
    src/utils/TimeUtils.h
    src/utils/StatusUtils.cpp
    src/utils/StatusUtils.h
    src/utils/FstabParser.cpp
    src/utils/FstabParser.h
    src/utils/TableLayoutPersistence.cpp
    src/utils/TableLayoutPersistence.h
    src/parsers/ProviderParsers.cpp
    src/parsers/ProviderParsers.h
)

target_link_libraries(linux-service-dashboard PRIVATE Qt6::Widgets Qt6::DBus)
target_compile_definitions(linux-service-dashboard PRIVATE SMART_HELPER_RELATIVE_PATH="${SMART_HELPER_RELATIVE_PATH}")

qt_add_executable(provider-parser-tests
    tests/test_provider_parsers.cpp
    src/parsers/ProviderParsers.cpp
    src/utils/JsonUtils.cpp
    src/utils/StatusUtils.cpp
)

target_link_libraries(provider-parser-tests PRIVATE Qt6::Test Qt6::Core)
target_include_directories(provider-parser-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_test(NAME provider-parser-tests COMMAND provider-parser-tests)

qt_add_executable(${SMART_HELPER_TARGET}
    src/helpers/SmartHelper.cpp
)

target_link_libraries(${SMART_HELPER_TARGET} PRIVATE Qt6::Core)

install(TARGETS linux-service-dashboard
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

install(TARGETS ${SMART_HELPER_TARGET}
    RUNTIME DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}
)

install(CODE "
  set(SMART_HELPER_INSTALL_PATH \"${_lsd_smart_helper_install_path}\")
  configure_file(
    \"${CMAKE_CURRENT_SOURCE_DIR}/resources/io.github.Adiker.LinuxServiceDashboard.smart-helper.policy.in\"
    \"${CMAKE_CURRENT_BINARY_DIR}/io.github.Adiker.LinuxServiceDashboard.smart-helper.policy\"
    @ONLY
  )
  if(IS_ABSOLUTE \"${LSD_POLKIT_POLICY_DIR}\")
    set(_lsd_polkit_policy_dest \"${LSD_POLKIT_POLICY_DIR}\")
  else()
    set(_lsd_polkit_policy_dest \"\${CMAKE_INSTALL_PREFIX}/${LSD_POLKIT_POLICY_DIR}\")
  endif()
  file(INSTALL
    \"${CMAKE_CURRENT_BINARY_DIR}/io.github.Adiker.LinuxServiceDashboard.smart-helper.policy\"
    DESTINATION \"\${_lsd_polkit_policy_dest}\"
  )
")

install(FILES resources/io.github.Adiker.LinuxServiceDashboard.desktop
    DESTINATION share/applications
)

install(FILES resources/linux-service-dashboard.svg
    DESTINATION share/icons/hicolor/scalable/apps
    RENAME io.github.Adiker.LinuxServiceDashboard.svg
)

foreach(icon_size 16 22 24 32 48 64 128 256)
    install(FILES resources/icons/linux-service-dashboard-${icon_size}.png
        DESTINATION share/icons/hicolor/${icon_size}x${icon_size}/apps
        RENAME io.github.Adiker.LinuxServiceDashboard.png
    )
endforeach()
