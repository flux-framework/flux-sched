topdir := $(dir $(lastword $(MAKEFILE_LIST)))
abs_topdir := $(abspath $(topdir))
flux_top_builddir := $(abspath $(abs_topdir)/../flux-core)
flux_top_srcdir := $(abspath $(abs_topdir)/../flux-core)

#
# Build against compiled flux-core source tree
#
FLUX_CFLAGS := $(shell PKG_CONFIG_PATH=$(flux_top_builddir)/etc \
	pkg-config --cflags flux-core-srctree)
FLUX_LIBS := $(shell PKG_CONFIG_PATH=$(flux_top_builddir)/etc \
	pkg-config --libs flux-core-srctree)

#
# Borrow some source from flux-core source tree that was not
# intended to be exported (FIXME)
#
vpath %.c $(flux_top_srcdir)/src/bindings/lua
vpath %.c $(flux_top_srcdir)/src/common/liblsd
vpath %.c $(flux_top_srcdir)/src/common/libutil

#
# Build against (in-tree) librdl
#
RDL_CFLAGS = -I$(topdir)/rdl
RDL_LIBS = -Wl,-rpath,$(abs_topdir)/rdl -L$(topdir)/rdl -lrdl
RDL_LUA_CPATH = $(abs_topdir)/rdl/?.so
RDL_LUA_PATH = $(abs_topdir)/rdl/?.lua

#
# When we invoke flux we need to provide additional LUA paths for RDL
#
PATH := $(PATH):$(flux_top_builddir)/src/cmd
FLUX = flux -C$(RDL_LUA_CPATH) -L$(RDL_LUA_PATH) \
	-M$(abs_topdir)/sched:$(abs_topdir)/echo:$(abs_topdir)/mon
COMMON_CFLAGS = -Wall -Werror -fPIC -D_GNU_SOURCE=1
