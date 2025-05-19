pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = palserver.pc
EXTRA_DIST = $(pkgconfig_DATA)

h_sources  = $(top_srcdir)/inc/ipc_interface.h \
             $(top_srcdir)/inc/pal_server_wrapper.h \
             $(top_srcdir)/inc/pal_client_manager.h \
             $(top_srcdir)/inc/pal_callback.h \
             $(top_srcdir)/inc/pal_ipc_data_rw.h

AM_CPPFLAGS := -I $(top_srcdir) -I ${top_srcdir}/inc -I $(PKG_CONFIG_SYSROOT_DIR)/usr/include/pal \
               -I $(PKG_CONFIG_SYSROOT_DIR)/usr/include/agm/ -I $(PKG_CONFIG_SYSROOT_DIR)/usr/include/spf

AM_CPPFLAGS += -D__unused=__attribute__\(\(__unused__\)\)

library_include_HEADERS = $(h_sources)
library_includedir = $(includedir)/palserver

lib_LTLIBRARIES = libpalserverwrapper.la
libpalserverwrapper_la_SOURCES   = ${top_srcdir}/src/pal_server_wrapper.cpp ${top_srcdir}/src/pal_client_manager.cpp \
                                   ${top_srcdir}/src/pal_callback.cpp ${top_srcdir}/src/pal_ipc_data_rw.cpp

libpalserverwrapper_la_CPPFLAGS := $(AM_CPPFLAGS)
libpalserverwrapper_la_LIBADD = -lpal -lagm
libpalserverwrapper_la_LDFLAGS   = -lcutils -llog -ldl -lbinder -shared -avoid-version -lutils -lpthread
