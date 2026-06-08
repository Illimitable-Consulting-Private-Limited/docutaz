##################################################
# Find and Add OpenSSL shared (dynamic) library 
# 
# - This module defines the following variables:
#   OpenSSL_DIR
#   OPENSSL_VERSION
#
# - Imported shared(dynamic) libraries "ssl" and "crypto" are created.
#
# - It is assumed that OpenSSL has been built and path to OpenSSL build directory is 
#   added into system(environment) variable DOCUTAZ_CMAKE_PREFIX_PATH.
#
##################################################

# Try to find OpenSSL directory (uses CMAKE_PREFIX_PATH locations)
#-------------------------------------------
find_path(
    OpenSSL_DIR include/openssl/ssl.h
    DOC "Path to OpenSSL (github.com/openssl/openssl) root directory"
)

# Find OpenSSL version
#-------------------------------------------
set (OPENSSL_INCLUDE_DIR "${OpenSSL_DIR}/include")

function(from_hex HEX DEC)
  string(TOUPPER "${HEX}" HEX)
  set(_res 0)
  string(LENGTH "${HEX}" _strlen)

  while (_strlen GREATER 0)
    math(EXPR _res "${_res} * 16")
    string(SUBSTRING "${HEX}" 0 1 NIBBLE)
    string(SUBSTRING "${HEX}" 1 -1 HEX)
    if (NIBBLE STREQUAL "A")
      math(EXPR _res "${_res} + 10")
    elseif (NIBBLE STREQUAL "B")
      math(EXPR _res "${_res} + 11")
    elseif (NIBBLE STREQUAL "C")
      math(EXPR _res "${_res} + 12")
    elseif (NIBBLE STREQUAL "D")
      math(EXPR _res "${_res} + 13")
    elseif (NIBBLE STREQUAL "E")
      math(EXPR _res "${_res} + 14")
    elseif (NIBBLE STREQUAL "F")
      math(EXPR _res "${_res} + 15")
    else()
      math(EXPR _res "${_res} + ${NIBBLE}")
    endif()

    string(LENGTH "${HEX}" _strlen)
  endwhile()

  set(${DEC} ${_res} PARENT_SCOPE)
endfunction()

if (OPENSSL_INCLUDE_DIR)
  if(OPENSSL_INCLUDE_DIR AND EXISTS "${OPENSSL_INCLUDE_DIR}/openssl/opensslv.h")
    file(STRINGS "${OPENSSL_INCLUDE_DIR}/openssl/opensslv.h" openssl_version_str
         REGEX "^#[\t ]*define[\t ]+OPENSSL_VERSION_NUMBER[\t ]+0x([0-9a-fA-F])+.*")

    if(openssl_version_str)
      # The version number is encoded as 0xMNNFFPPS: major minor fix patch status
      string(REGEX REPLACE "^.*OPENSSL_VERSION_NUMBER[\t ]+0x([0-9a-fA-F])([0-9a-fA-F][0-9a-fA-F])([0-9a-fA-F][0-9a-fA-F])([0-9a-fA-F][0-9a-fA-F])([0-9a-fA-F]).*$"
             "\\1;\\2;\\3;\\4;\\5" OPENSSL_VERSION_LIST "${openssl_version_str}")
      list(LENGTH OPENSSL_VERSION_LIST _vlen)
      if(_vlen GREATER_EQUAL 4)
        list(GET OPENSSL_VERSION_LIST 0 OPENSSL_VERSION_MAJOR)
        list(GET OPENSSL_VERSION_LIST 1 OPENSSL_VERSION_MINOR)
        from_hex("${OPENSSL_VERSION_MINOR}" OPENSSL_VERSION_MINOR)
        list(GET OPENSSL_VERSION_LIST 2 OPENSSL_VERSION_FIX)
        from_hex("${OPENSSL_VERSION_FIX}" OPENSSL_VERSION_FIX)
        list(GET OPENSSL_VERSION_LIST 3 OPENSSL_VERSION_PATCH)

        if (NOT OPENSSL_VERSION_PATCH STREQUAL "00")
          from_hex("${OPENSSL_VERSION_PATCH}" _tmp)
          math(EXPR OPENSSL_VERSION_PATCH_ASCII "${_tmp} + 96")
          unset(_tmp)
          string(ASCII "${OPENSSL_VERSION_PATCH_ASCII}" OPENSSL_VERSION_PATCH_STRING)
        endif ()
        set(OPENSSL_VERSION "${OPENSSL_VERSION_MAJOR}.${OPENSSL_VERSION_MINOR}.${OPENSSL_VERSION_FIX}${OPENSSL_VERSION_PATCH_STRING}")
      endif()
    else()
      # OpenSSL 3.x uses a different version constant — fall back to pkg-config or unknown
      find_package(PkgConfig QUIET)
      if(PKG_CONFIG_FOUND)
        pkg_check_modules(OPENSSL_PC QUIET openssl)
        if(OPENSSL_PC_VERSION)
          set(OPENSSL_VERSION "${OPENSSL_PC_VERSION}")
        endif()
      endif()
      if(NOT OPENSSL_VERSION)
        set(OPENSSL_VERSION "unknown")
      endif()
    endif()
  endif ()
endif ()


# Add imported ssl and crypto libraries (only if not already defined)
#-------------------------------------------

# Add imported target ssl (ssleay32)
if(NOT TARGET ssl)
  add_library(ssl SHARED IMPORTED)
endif()

# Add imported target for crypto (libeay32)
if(NOT TARGET crypto)
  add_library(crypto SHARED IMPORTED)
endif()
    
# todo: refactor
if(SYSTEM_WINDOWS)
    # Locate the import libraries rather than assuming a flat layout — vcpkg and
    # most OpenSSL distributions put them under lib/ (libssl.lib / libcrypto.lib).
    find_library(_OPENSSL_SSL_LIB    NAMES ssl libssl ssleay32      HINTS "${OpenSSL_DIR}/lib" "${OpenSSL_DIR}")
    find_library(_OPENSSL_CRYPTO_LIB NAMES crypto libcrypto libeay32 HINTS "${OpenSSL_DIR}/lib" "${OpenSSL_DIR}")
    set_target_properties(ssl PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES   "${OpenSSL_DIR}/include"
        IMPORTED_IMPLIB                 "${_OPENSSL_SSL_LIB}"
    )
    set_target_properties(crypto PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES   "${OpenSSL_DIR}/include"
        IMPORTED_IMPLIB                 "${_OPENSSL_CRYPTO_LIB}"
    )
else()
  find_library(_OPENSSL_SSL_LIB NAMES ssl ssleay32 HINTS "${OpenSSL_DIR}/lib64" "${OpenSSL_DIR}/lib" NO_DEFAULT_PATH)
  find_library(_OPENSSL_SSL_LIB NAMES ssl ssleay32)
  find_library(_OPENSSL_CRYPTO_LIB NAMES crypto libeay32 HINTS "${OpenSSL_DIR}/lib64" "${OpenSSL_DIR}/lib" NO_DEFAULT_PATH)
  find_library(_OPENSSL_CRYPTO_LIB NAMES crypto libeay32)
  set_target_properties(ssl PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES   "${OpenSSL_DIR}/include"
      IMPORTED_LOCATION               "${_OPENSSL_SSL_LIB}"
  )
  set_target_properties(crypto PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES   "${OpenSSL_DIR}/include"
      IMPORTED_LOCATION               "${_OPENSSL_CRYPTO_LIB}"
  )
endif()

# Honor the standard find_package(OpenSSL) contract so third-party package
# configs that do find_dependency(OpenSSL) are satisfied by this custom finder.
# vcpkg's libssh2-config.cmake (used on Windows) otherwise fails with "OpenSSL
# could not be found" even though ssl/crypto were located here. We also expose
# the canonical OpenSSL::SSL / OpenSSL::Crypto targets such configs link against.
if(_OPENSSL_SSL_LIB AND _OPENSSL_CRYPTO_LIB)
    set(OPENSSL_FOUND TRUE)
    set(OpenSSL_FOUND TRUE)
    set(OPENSSL_INCLUDE_DIR    "${OpenSSL_DIR}/include")
    set(OPENSSL_SSL_LIBRARY    "${_OPENSSL_SSL_LIB}")
    set(OPENSSL_CRYPTO_LIBRARY "${_OPENSSL_CRYPTO_LIB}")
    set(OPENSSL_LIBRARIES      "${_OPENSSL_SSL_LIB};${_OPENSSL_CRYPTO_LIB}")

    if(NOT TARGET OpenSSL::Crypto)
        add_library(OpenSSL::Crypto ALIAS crypto)
    endif()
    if(NOT TARGET OpenSSL::SSL)
        add_library(OpenSSL::SSL ALIAS ssl)
    endif()
endif()

# End of file