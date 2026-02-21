cmake_minimum_required(VERSION 3.23)

project(csv_median_calculator 
    VERSION 1.0.0
    LANGUAGES CXX
)

# C++23
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Опция для строгих проверок
option(ENABLE_STRICT_WARNINGS "Enable strict compiler warnings" ON)

# Статическая линковка для MinGW (Windows)
if(MINGW)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++ -static")
    message(STATUS "MinGW detected: enabling static linking")
endif()

# FetchContent
include(FetchContent)

# ============================================================================
# Зависимости через FetchContent
# ============================================================================

# Boost (Program_options и Accumulators)
message(STATUS "Fetching Boost...")
FetchContent_Declare(
    Boost
    URL https://github.com/boostorg/boost/releases/download/boost-1.84.0/boost-1.84.0.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(BOOST_INCLUDE_LIBRARIES program_options accumulators)
set(BOOST_ENABLE_CMAKE ON)
FetchContent_MakeAvailable(Boost)

# toml++
message(STATUS "Fetching toml++...")
FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.4.0
)
FetchContent_MakeAvailable(tomlplusplus)

# spdlog
message(STATUS "Fetching spdlog...")
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.13.0
)
FetchContent_MakeAvailable(spdlog)

# ============================================================================
# Исполняемый файл
# ============================================================================

add_executable(csv_median_calculator
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
)

# Подключение заголовочных файлов
target_include_directories(csv_median_calculator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Линковка библиотек
target_link_libraries(csv_median_calculator PRIVATE
    Boost::program_options
    tomlplusplus::tomlplusplus
    spdlog::spdlog
)

# Строгие флаги компиляции
if(ENABLE_STRICT_WARNINGS)
    if(MSVC)
        target_compile_options(csv_median_calculator PRIVATE /W4)
    else()
        target_compile_options(csv_median_calculator PRIVATE 
            -Wall -Wextra -Wpedantic)
    endif()
endif()

# ============================================================================
# Генератор тестовых данных
# ============================================================================

add_executable(generate_test_data
    ${CMAKE_CURRENT_SOURCE_DIR}/src/generate_test_data.cpp
)

# Строгие флаги для генератора
if(ENABLE_STRICT_WARNINGS)
    if(MSVC)
        target_compile_options(generate_test_data PRIVATE /W4)
    else()
        target_compile_options(generate_test_data PRIVATE 
            -Wall -Wextra -Wpedantic)
    endif()
endif()

# ============================================================================
# Копирование конфигурации
# ============================================================================
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config.toml
    ${CMAKE_CURRENT_BINARY_DIR}/config.toml
    COPYONLY
)

# ============================================================================
# Установка
# ============================================================================

install(TARGETS csv_median_calculator generate_test_data
    RUNTIME DESTINATION bin
)

install(FILES config.toml
    DESTINATION bin
)