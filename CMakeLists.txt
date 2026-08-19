cmake_minimum_required(VERSION 3.16)
project(OmniAuth LANGUAGES CXX)

include(qt.cmake)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(QT_NO_PRIVATE_MODULE_WARNING ON)

find_package(QT NAMES Qt6 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR}
    COMPONENTS
        Core
        Gui
        Widgets
        Sql
)
find_package(OpenCV REQUIRED)
find_package(unofficial-sodium CONFIG REQUIRED)

qt_standard_project_setup()

add_subdirectory(third_party/FluentQt)

include(FetchContent)

# QWindowKit
FetchContent_Declare(
    qwindowkit
    GIT_REPOSITORY https://github.com/stdware/qwindowkit.git
    GIT_TAG        1.5.0
)
FetchContent_MakeAvailable(qwindowkit)


set(ONNXRUNTIME_VERSION "1.19.0" CACHE STRING "ONNX Runtime Version")

if(WIN32)
    set(ONNXRUNTIME_EXT "zip")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|aarch64")
        set(ONNXRUNTIME_ARCH "win-arm64")
    else()
        set(ONNXRUNTIME_ARCH "win-x64")
    endif()
    set(ONNXRUNTIME_LIB_NAME "onnxruntime.lib")
    set(ONNXRUNTIME_BIN_NAME "onnxruntime.dll")
elseif(APPLE)
    set(ONNXRUNTIME_EXT "tgz")
    set(ONNXRUNTIME_ARCH "osx-universal2")
    set(ONNXRUNTIME_BIN_NAME "libonnxruntime.dylib")
else() # Linux / Unix
    set(ONNXRUNTIME_EXT "tgz")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|aarch64")
        set(ONNXRUNTIME_ARCH "linux-aarch64")
    else()
        set(ONNXRUNTIME_ARCH "linux-x64")
    endif()
    set(ONNXRUNTIME_BIN_NAME "libonnxruntime.so")
endif()

FetchContent_Declare(
    onnxruntime_binaries
    URL "https://github.com/microsoft/onnxruntime/releases/download/v${ONNXRUNTIME_VERSION}/onnxruntime-${ONNXRUNTIME_ARCH}-${ONNXRUNTIME_VERSION}.${ONNXRUNTIME_EXT}"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(onnxruntime_binaries)

add_library(onnxruntime::onnxruntime SHARED IMPORTED GLOBAL)

set_target_properties(onnxruntime::onnxruntime PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${onnxruntime_binaries_SOURCE_DIR}/include"
)

if(WIN32)
    set_target_properties(onnxruntime::onnxruntime PROPERTIES
        IMPORTED_LOCATION "${onnxruntime_binaries_SOURCE_DIR}/lib/${ONNXRUNTIME_BIN_NAME}"
        IMPORTED_IMPLIB "${onnxruntime_binaries_SOURCE_DIR}/lib/${ONNXRUNTIME_LIB_NAME}"
    )
elseif(APPLE)
    set_target_properties(onnxruntime::onnxruntime PROPERTIES
        IMPORTED_LOCATION "${onnxruntime_binaries_SOURCE_DIR}/lib/libonnxruntime.dylib"
    )
else() # Linux
    set_target_properties(onnxruntime::onnxruntime PROPERTIES
        IMPORTED_LOCATION "${onnxruntime_binaries_SOURCE_DIR}/lib/libonnxruntime.so"
    )
endif()


option(COLLECT_BEHAVIOR_DATA "Enable behavior data collection mode for training" OFF)

set(PROJECT_SOURCES 
    src/main.cpp

    src/ui/widget/FaceScannerWidget.h
    src/ui/widget/FaceScannerWidget.cpp
    src/ui/widget/SliderCaptchaWidget.h
    src/ui/widget/SliderCaptchaWidget.cpp
    src/ui/widget/CaptchaOverlay.h
    src/ui/widget/CaptchaOverlay.cpp
    src/ui/widget/Stepper.h
    src/ui/widget/Stepper.cpp

    src/domain/usecase/FaceLoginUseCase.h
    src/domain/usecase/FaceLoginUseCase.cpp
    src/domain/usecase/FaceEnrollUseCase.h
    src/domain/usecase/FaceEnrollUseCase.cpp
    src/domain/usecase/PasswordLoginUseCase.h
    src/domain/usecase/PasswordLoginUseCase.cpp
    src/domain/usecase/SmsLoginUseCase.h
    src/domain/usecase/SmsLoginUseCase.cpp
    src/domain/usecase/RegisterUseCase.h
    src/domain/usecase/RegisterUseCase.cpp
    src/domain/usecase/ResetPasswordUseCase.h
    src/domain/usecase/ResetPasswordUseCase.cpp
    src/domain/usecase/CaptchaService.h
    src/domain/usecase/CaptchaService.cpp
    src/domain/model/BehaviorTracker.h
    src/domain/model/BehaviorTracker.cpp
    src/domain/model/TrackPoint.h

    src/data/local/RetinaFaceEngine.h
    src/data/local/RetinaFaceEngine.cpp
    src/data/local/AntiSpoofingEngine.h
    src/data/local/AntiSpoofingEngine.cpp
    src/data/local/ArcFaceEngine.h
    src/data/local/ArcFaceEngine.cpp
    src/data/local/BehaviorRiskEngine.h
    src/data/local/BehaviorRiskEngine.cpp
    src/domain/repository/BehaviorRiskRepository.h
    src/data/local/LocalRiskDataSource.h
    src/data/local/LocalRiskDataSource.cpp
    src/data/remote/RemoteRiskDataSource.h
    src/data/remote/RemoteRiskDataSource.cpp
    src/data/repository/BehaviorRiskRepositoryImpl.h
    src/data/repository/BehaviorRiskRepositoryImpl.cpp
    src/domain/repository/UserRepository.h
    src/data/local/LocalUserDataSource.h
    src/data/local/LocalUserDataSource.cpp
    src/data/remote/RemoteUserDataSource.h
    src/data/repository/UserRepositoryImpl.h
    src/data/repository/UserRepositoryImpl.cpp
    src/domain/repository/CaptchaRepository.h
    src/data/local/LocalCaptchaDataSource.h
    src/data/local/LocalCaptchaDataSource.cpp
    src/data/remote/RemoteCaptchaDataSource.h
    src/data/remote/RemoteCaptchaDataSource.cpp
    src/data/repository/CaptchaRepositoryImpl.h
    src/data/repository/CaptchaRepositoryImpl.cpp
    src/domain/repository/FaceAuthRepository.h
    src/data/local/LocalFaceDataSource.h
    src/data/local/LocalFaceDataSource.cpp
    src/data/remote/RemoteFaceDataSource.h
    src/data/remote/RemoteFaceDataSource.cpp
    src/data/repository/FaceAuthRepositoryImpl.h
    src/data/repository/FaceAuthRepositoryImpl.cpp
    src/domain/usecase/BehaviorRiskService.h
    src/domain/usecase/BehaviorRiskService.cpp
    src/data/di/AppContainer.h
    src/data/di/AppContainer.cpp
    src/core/CryptoUtils.h
    src/core/CryptoUtils.cpp

    src/domain/entity/User.h
    src/domain/model/UserAuthDTO.h
    src/domain/model/FaceTypes.h
    src/ui/screen/login/LoginPage.h
    src/ui/screen/login/LoginPage.cpp
    src/ui/screen/facescan/FaceScannerPage.h
    src/ui/screen/facescan/FaceScannerPage.cpp
    src/ui/screen/register/RegisterPage.h
    src/ui/screen/register/RegisterPage.cpp
    src/ui/screen/recovery/RecoveryPage.h
    src/ui/screen/recovery/RecoveryPage.cpp

    src/ui/common/BaseViewModel.h
    src/ui/screen/main/AuthViewModel.h
    src/ui/screen/main/AuthViewModel.cpp
    src/ui/screen/login/LoginViewModel.h
    src/ui/screen/login/LoginViewModel.cpp
    src/ui/screen/facescan/FaceScannerViewModel.h
    src/ui/screen/facescan/FaceScannerViewModel.cpp
    src/ui/screen/register/RegisterViewModel.h
    src/ui/screen/register/RegisterViewModel.cpp
    src/ui/screen/recovery/RecoveryViewModel.h
    src/ui/screen/recovery/RecoveryViewModel.cpp

    src/ui/screen/main/AuthWindow.h
    src/ui/screen/main/AuthWindow.cpp
    src/ui/screen/main/MainWindow.h
    src/ui/screen/main/MainWindow.cpp

    src/ui/window/WindowBase.h
    src/ui/window/NavigationWindow.h
    src/ui/animation/AnimatedVisualSource.h
    src/ui/animation/AnimatedBackVisualSource.h
    src/ui/animation/AnimatedGlobalNavVisualSource.h
    src/ui/animation/AnimatedSettingsVisualSource.h
    src/ui/animation/AnimatedIcon.h
    src/ui/animation/AnimatedIcon.cpp
    src/ui/window/TitleBar.cpp
    src/ui/window/WindowBase.cpp
    src/ui/window/NavigationWindow.cpp

    src/ui/navigation/NavigationWidget.h
    src/ui/navigation/NavigationWidget.cpp
    src/ui/navigation/NavigationToolButton.h
    src/ui/navigation/NavigationToolButton.cpp
    src/ui/navigation/NavigationSeparator.h
    src/ui/navigation/NavigationSeparator.cpp
    src/ui/navigation/NavigationPushButton.h
    src/ui/navigation/NavigationPushButton.cpp
    src/ui/navigation/NavigationTreeItem.h
    src/ui/navigation/NavigationTreeItem.cpp
    src/ui/navigation/NavigationSectionHeader.h
    src/ui/navigation/NavigationSectionHeader.cpp
    src/ui/navigation/NavigationIndicator.h
    src/ui/navigation/NavigationIndicator.cpp
    src/ui/navigation/NavigationTreeWidget.h
    src/ui/navigation/NavigationTreeWidget.cpp
    src/ui/navigation/NavigationTreeWidgetBase.h
    src/ui/navigation/NavigationTreeWidgetBase.cpp
    src/ui/navigation/NavigationFocusHost.h
    src/ui/navigation/NavigationMetrics.h
    src/ui/navigation/NavigationFlyout.h
    src/ui/navigation/NavigationFlyout.cpp
    src/ui/navigation/NavigationPanel.h
    src/ui/navigation/NavigationPanel.cpp
    src/ui/navigation/NavigationHistory.h
    src/ui/navigation/NavigationHistory.cpp
)

qt_add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

if(COLLECT_BEHAVIOR_DATA)
    target_compile_definitions(${PROJECT_NAME} PRIVATE COLLECT_BEHAVIOR_DATA)
endif()

add_subdirectory(res)

set_target_properties(${PROJECT_NAME}
    PROPERTIES
        WIN32_EXECUTABLE TRUE
)

set(CMAKE_BUILD_RPATH_USE_ORIGIN ON)
if(APPLE)
    set(CMAKE_INSTALL_RPATH "@executable_path")
elseif(UNIX)
    set(CMAKE_INSTALL_RPATH "$ORIGIN")
endif()

if(WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/res/models
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/models
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${onnxruntime_binaries_SOURCE_DIR}/lib/${ONNXRUNTIME_BIN_NAME}"
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )
    
    if(DEFINED OpenCV_LIB_PATH)
        get_filename_component(OPENCV_BIN_DIR "${OpenCV_LIB_PATH}/../bin" ABSOLUTE)
    elseif(DEFINED OpenCV_DIR)
        if(EXISTS "${OpenCV_DIR}/x64/vc16/bin")
            set(OPENCV_BIN_DIR "${OpenCV_DIR}/x64/vc16/bin")
        elseif(EXISTS "${OpenCV_DIR}/build/x64/vc16/bin")
            set(OPENCV_BIN_DIR "${OpenCV_DIR}/build/x64/vc16/bin")
        endif()
    endif()

    if(OPENCV_BIN_DIR AND EXISTS "${OPENCV_BIN_DIR}")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${OPENCV_BIN_DIR}"
                $<TARGET_FILE_DIR:${PROJECT_NAME}>
        )
    endif()
elseif(APPLE)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/res/models
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/models
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${onnxruntime_binaries_SOURCE_DIR}/lib/libonnxruntime.dylib"
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )
else() # Linux
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/res/models
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/models
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${onnxruntime_binaries_SOURCE_DIR}/lib/libonnxruntime.so"
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )
endif()

target_include_directories(${PROJECT_NAME} PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        Qt::Core
        Qt::Gui
        Qt::Widgets
        Qt::Sql
        FluentQt::FluentQt
        ${OpenCV_LIBS}
        onnxruntime::onnxruntime
        QWindowKit::Widgets
        unofficial-sodium::sodium
)
