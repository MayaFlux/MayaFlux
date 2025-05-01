# FindReadline.cmake - Find the GNU Readline library using pkg-config
#
# This module defines:
#  Readline_FOUND        - System has Readline
#  Readline_INCLUDE_DIR  - Readline include directory
#  Readline_LIBRARIES    - Readline libraries

# Try to use pkg-config first (works well with Homebrew)
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_READLINE QUIET readline)
endif()

find_path(Readline_INCLUDE_DIR
      NAMES readline/readline.h
      PATHS
        ${PC_READLINE_INCLUDEDIR}
        ${PC_READLINE_INCLUDE_DIRS}
        /usr/include
        /usr/local/include
        /opt/homebrew/opt/readline/include
        /usr/local/opt/readline/include
      NO_DEFAULT_PATH
    )

find_library(Readline_LIBRARY
      NAMES readline
      PATHS
        ${PC_READLINE_LIBDIR}
        ${PC_READLINE_LIBRARY_DIRS}
        /usr/lib
        /usr/local/lib
        /opt/homebrew/opt/readline/lib
        /usr/local/opt/readline/lib
      NO_DEFAULT_PATH
    )

find_library(History_LIBRARY
      NAMES history
      PATHS
        ${PC_READLINE_LIBDIR}
        ${PC_READLINE_LIBRARY_DIRS}
        /usr/lib
        /usr/local/lib
        /opt/homebrew/opt/readline/lib
        /usr/local/opt/readline/lib
      NO_DEFAULT_PATH
    )

set(Readline_LIBRARIES ${Readline_LIBRARY})
if(History_LIBRARY)
    list(APPEND Readline_LIBRARIES ${History_LIBRARY})
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Readline
      DEFAULT_MSG
      Readline_LIBRARY
      Readline_INCLUDE_DIR
    )

mark_as_advanced(Readline_INCLUDE_DIR Readline_LIBRARY History_LIBRARY)
