include(ExternalProject)

set(EXTRA_CFLAGS -I${PREFIX_DIR}/include)
set(EXTRA_LDFLAGS -L${PREFIX_DIR}/lib)
set(EXTRA_LIBS "-lm -pthread -ldl")

if (WIN32)

elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")

if (CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
ExternalProject_Add(openssl
    URL ${CMAKE_CURRENT_SOURCE_DIR}/openssl-3.5.4.tar.gz
    SOURCE_DIR  ${CMAKE_CURRENT_BINARY_DIR}/openssl
    BUILD_IN_SOURCE 1
    # On macOS arm64, Apple's assembler may fail on certain OpenSSL ASM sources.
    # Use no-asm to avoid AES/ARM ASM build errors while still enabling EC.
    CONFIGURE_COMMAND
        ./Configure --prefix=${PREFIX_DIR} darwin64-arm64-cc enable-ec no-asm
        --libdir=${PREFIX_DIR}/lib
    BUILD_COMMAND make -j 4
    INSTALL_COMMAND make install_sw
    )
else()
ExternalProject_Add(openssl
    URL ${CMAKE_CURRENT_SOURCE_DIR}/openssl-3.5.4.tar.gz
    SOURCE_DIR  ${CMAKE_CURRENT_BINARY_DIR}/openssl
    BUILD_IN_SOURCE 1
    # Enable EC support explicitly and allow assembly optimizations.
    CONFIGURE_COMMAND
        ./Configure --prefix=${PREFIX_DIR} darwin64-x86_64-cc enable-ec no-asm
        --libdir=${PREFIX_DIR}/lib
    BUILD_COMMAND make -j 4
    INSTALL_COMMAND make install_sw
    )
endif()

else()
ExternalProject_Add(openssl
    URL ${CMAKE_CURRENT_SOURCE_DIR}/openssl-3.5.4.tar.gz
    SOURCE_DIR  ${CMAKE_CURRENT_BINARY_DIR}/openssl
    BUILD_IN_SOURCE 1
    # Enable EC explicitly; keep static build if desired, but ensure EC/X25519 are available in the build.
    CONFIGURE_COMMAND
        ./Configure --prefix=${PREFIX_DIR} no-shared linux-x86_64 enable-ec
        --libdir=${PREFIX_DIR}/lib
    BUILD_COMMAND make -j 4
    INSTALL_COMMAND make install_sw
    )
endif()

add_subdirectory(libuv)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)
add_subdirectory(yaml-cpp)
set(ENABLE_OPENSSL ON CACHE BOOL "Enable OpenSSL for libsrtp" FORCE)
# Disable libsrtp test/example binaries in this project build.
set(TEST_APPS OFF CACHE BOOL "Build test applications" FORCE)
add_subdirectory(libsrtp)

