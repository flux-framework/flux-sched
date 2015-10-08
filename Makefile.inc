topdir := $(dir $(lastword $(MAKEFILE_LIST)))
abs_topdir := $(abspath $(topdir))

#
# If user ran ./config dir, then include Makefile.inc from there
# to get FLUX_CFLAGS, FLUX_LIBS, and FLUX_SRCDIR.
# Otherwise we're building against installed flux-core.
#
FLUX_BUILDDIR=$(shell readlink -e $(abs_topdir)/.build)
ifneq ($(FLUX_BUILDDIR),)
include $(FLUX_BUILDDIR)/etc/Makefile.inc
PATH := $(FLUX_BUILDDIR)/src/cmd:$(PATH)
FLUX_LIBS := -Wl,-rpath,$(FLUX_BUILDDIR)/src/modules/libjsc/.libs \
            -L$(FLUX_BUILDDIR)/src/modules/libjsc/.libs -ljsc \
            $(FLUX_LIBS)
else
FLUX_CFLAGS := $(shell pkg-config --cflags flux-core)
FLUX_LIBS := $(shell pkg-config --libs flux-core)
#FIXME: Need a way to pick up module interface libs from pkg-config? 
endif

#
# Borrow some source from flux-core source tree that was not
# intended to be exported (FIXME)
#
ifneq ($(FLUX_SRCDIR),)
vpath %.c $(FLUX_SRCDIR)/src/bindings/lua
vpath %.c $(FLUX_SRCDIR)/src/common/liblsd
vpath %.c $(FLUX_SRCDIR)/src/common/libutil
endif

#
# Build against (in-tree) librdl
#
RDL_CFLAGS = -I$(topdir)/rdl
RDL_LIBS = -Wl,-rpath,$(abs_topdir)/rdl -L$(topdir)/rdl -lrdl
RDL_LUA_CPATH = $(abs_topdir)/rdl/?.so
RDL_LUA_PATH = $(abs_topdir)/rdl/?.lua

SCHED_MODULE_PATH = $(abs_topdir)/sched
SUBMIT_EXEC_PATH = $(abs_topdir)/sched

RESRC_CFLAGS = -I$(topdir)/resrc
RESRC_LIBS = -Wl,-rpath,$(abs_topdir)/resrc -L$(topdir)/resrc -lresrc

SIM_LIBS = -Wl,-rpath,$(abs_topdir)/simulator -L$(topdir)/simulator -lsim

KVS_LIBS = -Wl,-rpath,$(FLUX_BUILDDIR)/src/modules/kvs/.libs \
         -L$(FLUX_BUILDDIR)/src/modules/kvs/.libs -lkvs
#
# When we invoke flux we need to provide additional LUA paths for RDL
#
FLUX = flux -C$(RDL_LUA_CPATH) -L$(RDL_LUA_PATH) -M$(SCHED_MODULE_PATH) -x$(SUBMIT_EXEC_PATH)
COMMON_CFLAGS = -g -Wall -Werror -Wno-error=deprecated-declarations -fPIC -D_GNU_SOURCE=1
