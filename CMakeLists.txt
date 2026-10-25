cmake_minimum_required(VERSION 3.16)
project(pass-frontend LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(X11 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GPGME REQUIRED IMPORTED_TARGET gpgme)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pg")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -DNDEBUG")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -g")

set(SOURCE_FILES
  src/main.c
  src/crypto.c
)

add_executable(${PROJECT_NAME} ${SOURCE_FILES})

set(CMAKE_BUILD_TYPE Release)

target_include_directories(${PROJECT_NAME} PRIVATE ${X11_INCLUDE_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE ${X11_LIBRARIES} Xrandr PkgConfig::GPGME)
