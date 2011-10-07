FIND_LIBRARY(LIBCURL_LIBRARY NAMES libcurl.so PATHS /usr/lib )
FIND_PATH(LIBCURL_INCLUDE_DIR NAMES curl.h PATHS /usr/include/curl )


if( LIBCURL_LIBRARY AND LIBCURL_INCLUDE_DIR )
    message( STATUS "Found libcurl: ${LIBCURL_LIBRARY}, ${LIBCURL_INCLUDE_DIR}")
    set( LIBCURL_FOUND 1 )
else( LIBCURL_LIBRARY AND LIBCURL_INCLUDE_DIR )
    message( FATAL_ERROR "Could NOT find libcurl" )
endif( LIBCURL_LIBRARY AND LIBCURL_INCLUDE_DIR )