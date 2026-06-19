cmake_minimum_required(VERSION 3.21)

project(linux-service-dashboard VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTORCC ON)

include(GNUInstallDirs)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

qt_standard_project_setup()

set(SMART_HELPER_TARGET linux-service-dashboard-smart-helper)
set(SMART_HELPER_INSTALL_PATH "${CMAKE_INSTALL_FULL_LIBEXECDIR}/${SMART_HELPER_TARGET}")

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

configure_file(
    resources/io.github.Adiker.LinuxServiceDashboard.smart-helper.policy.in
    ${CMAKE_CURRENT_BINARY_DIR}/io.github.Adiker.LinuxServiceDashboard.smart-helper.policy
    @ONLY
)

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
)

target_link_libraries(linux-service-dashboard PRIVATE Qt6::Widgets)
target_compile_definitions(linux-service-dashboard PRIVATE SMART_HELPER_PATH="${SMART_HELPER_INSTALL_PATH}")

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

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/io.github.Adiker.LinuxServiceDashboard.smart-helper.policy
    DESTINATION ${LSD_POLKIT_POLICY_DIR}
)

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
