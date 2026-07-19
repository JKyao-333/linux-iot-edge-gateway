set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

set(
    AARCH64_SYSROOT
    ""
    CACHE PATH
    "Directory containing extracted ARM64 development packages"
)

# CMake configures a nested project while probing the cross compiler. Keep the
# dependency sysroot available inside that try_compile project as well.
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES AARCH64_SYSROOT)

if(NOT AARCH64_SYSROOT AND DEFINED ENV{AARCH64_SYSROOT})
    set(AARCH64_SYSROOT "$ENV{AARCH64_SYSROOT}")
endif()

if(NOT AARCH64_SYSROOT)
    message(FATAL_ERROR
        "AARCH64_SYSROOT is required. Run scripts/setup_aarch64_sysroot.sh "
        "and pass -DAARCH64_SYSROOT=/path/to/aarch64-sysroot."
    )
endif()

if(NOT EXISTS "${AARCH64_SYSROOT}/usr/include")
    message(FATAL_ERROR
        "ARM64 dependency sysroot not found: ${AARCH64_SYSROOT}. "
        "Run scripts/setup_aarch64_sysroot.sh first."
    )
endif()

set(AARCH64_LIBRARY_DIR
    "${AARCH64_SYSROOT}/usr/lib/aarch64-linux-gnu"
)

set(CMAKE_FIND_ROOT_PATH
    "${AARCH64_SYSROOT}"
    "/usr/aarch64-linux-gnu"
)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(ENV{PKG_CONFIG_PATH} "")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "${AARCH64_SYSROOT}")
set(ENV{PKG_CONFIG_LIBDIR}
    "${AARCH64_LIBRARY_DIR}/pkgconfig:${AARCH64_SYSROOT}/usr/share/pkgconfig"
)

string(APPEND CMAKE_EXE_LINKER_FLAGS_INIT
    " -Wl,-rpath-link,${AARCH64_LIBRARY_DIR}"
)
