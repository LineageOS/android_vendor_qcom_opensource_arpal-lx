
AM_CPPFLAGS := -I $(top_srcdir) -I $(top_srcdir)/inc -I $(PKG_CONFIG_SYSROOT_DIR)/usr/include/pal
AM_CPPFLAGS += -D__unused=__attribute__\(\(__unused__\)\)
lib_LTLIBRARIES      = libpalclient.la
libpalclient_ladir = $(libdir)
libpalclient_la_SOURCES   = ${top_srcdir}/src/pal_client_wrapper.cpp
libpalclient_la_CPPFLAGS := $(AM_CPPFLAGS)
libpalclient_la_LDFLAGS   = -lcutils -llog -ldl -lbinder -shared -avoid-version -lutils -lrt
libpalclient_la_LIBADD     =  -lpalserverwrapper

# bin_PROGRAMS := pal_client_test
# pal_client_test_SOURCES  := ${top_srcdir}/test/pal_test.cpp

# pal_client_test_la_CPPFLAGS := $(AM_CPPFLAGS)
# pal_client_test_LDADD    := libpalclient.la
