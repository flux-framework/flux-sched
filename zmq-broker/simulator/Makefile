TOP =             $(shell pwd)/../
LIBDIR =          $(TOP)/lib
EXTRA_CFLAGS +=   -I$(TOP) -I$(TOP)/include -I$(TOP)/util -I$(TOP)/zutil -I$(TOP)/simulator
EXTRA_LDFLAGS +=  -L$(TOP)/lib $(TOP)/zutil/libzutil.a $(TOP)/util/libutil.a
LDADD_CLI = $(EXTRA_LDFLAGS) -ljson -lzmq -lczmq -lutil -lm -lmunge

CFLAGS = -Werror -Wall -g -fPIC $(EXTRA_CFLAGS) \
	-I/usr/include/czmq \
	-DPLUGIN_PATH=\"$(TOP)/plugins\" \
	-DEXEC_PATH=\"$(TOP)/libexec\"
LDFLAGS = -Wl,-rpath,$(LIBDIR)

LIBSIM_SOURCES := simulator.c

all: libsim.a

simulator.o: simulator.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ -c $< $(LDADD_CLI)

libsim.a : $(LIBSIM_SOURCES:.c=.o)
	ar rcs $@ $^

clean:
	rm -f *.a *.o

.PHONY: all clean
