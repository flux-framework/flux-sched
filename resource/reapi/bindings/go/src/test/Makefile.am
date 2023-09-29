AUTOMAKE_OPTIONS = subdir-objects no-dependencies

AM_CFLAGS = \
    -I$(abs_top_builddir)/resource/reapi/bindings/c

GOPATH = \
    $(abs_top_builddir)/resource/reapi/bindings/go

GO111MODULE = off

AM_LDFLAGS = \
    -L$(top_builddir)/resource/reapi/bindings/c/.libs \
    -lreapi_cli \
    -L$(top_builddir)/resource/.libs \
    -lresource \
    -lflux-idset \
    -lstdc++ \
    -lczmq \
    -ljansson \
    -lhwloc \
    -lboost_system \
    -lflux-hostlist \
    -lboost_graph \
    -lyaml-cpp

COMMONENVVAR=GOOS=$(shell uname -s | tr A-Z a-z)

if WITH_GO
    TESTS = main$(EXEEXT)
    check_PROGRAMS = $(TESTS)
endif

main_SOURCES = main.go

main$(EXEEXT):
	$(TOP_DIR) GOPATH=$(GOPATH) $(COMMONENVVAR) GO111MODULE=$(GO111MODULE) CGO_CFLAGS="$(AM_CFLAGS)" CGO_LDFLAGS="$(AM_LDFLAGS)" $(GOC) build -ldflags '-w' -o $@ $<

clean:
	rm -f main main.log main.trs

clean-local:
	-rm -f main main.log main.trs

