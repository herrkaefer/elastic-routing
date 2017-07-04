IDIR = -I/usr/local/include -I../libcube/include
ODIR = build
SRCDIR = src
DEMODIR = demo
TARGETDIR = bin

CC = gcc
CFLAGS = $(IDIR) -g -Wall
# CFLAGS += -DWITHSTATS
CFLAGS += -O3
CFLAGS += -std=c99

# LDIR = -L./lib3rd/libyaml
# LIBS = -lyaml
LDIR = -L/usr/local/lib -L../libcube
LIBS = -lm -lcube
# LIBS += -lczmq -lzmq

_MODULES = coord2d \
	       route \
	       solution \
	       vrp \
	       tspi \
	       tsp \
	       cvrp \
	       vrptw

_OBJS = $(_MODULES:=.o)
OBJS = $(patsubst %,$(ODIR)/%,$(_OBJS))
_LIBSRC = $(_MODULES:=.c)
LIBSRC = $(patsubst %,$(SRCDIR)/%,$(_LIBSRC))
LIBTARGET = liber.a
TESTOBJS = $(OBJS) $(ODIR)/selftest.o
TESTTARGET = $(TARGETDIR)/selftest
EXESRC = $(SRCDIR)/main.c $(LIBSRC)
EXEOBJS = $(OBJS) $(ODIR)/main.o
EXETARGET = $(TARGETDIR)/er
DEMOSRC = $(DEMODIR)/demo.c
DEMOTARGET = $(TARGETDIR)/demo

.PHONY: clib cdll test exe demo pylib clean

$(ODIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(@D)
	$(CC) -c $< -o $@ $(CFLAGS)

test: $(TESTOBJS)
	$(CC) -o $(TESTTARGET) $(TESTOBJS) $(CFLAGS) $(LDIR) $(LIBS)
	$(TESTTARGET)

clib: $(OBJS)
	ar rcs $(LIBTARGET) $(OBJS)

cdll: $(OBJS)
	$(CC) -shared -o $@ $^ -Wl,--out-implib,$(LIBTARGET) $(LDIR) $(LIBS)

exe: $(EXEOBJS)
	$(CC) -o $(EXESRC) $(EXEOBJS) $(CFLAGS) $(LDIR) $(LIBS)

demo: clib
	$(CC) -o $(DEMOTARGET) $(DEMOSRC) -Wall -O2 -L. $(LIBTARGET)

pylib:
	python py/setup.py build_ext --inplace
	mv py*.so py/
	python py/test_benchmark.py

clean:
	rm -rf $(TARGETDIR)/* *.dll *.a py/*so $(ODIR)/* py/*.c
