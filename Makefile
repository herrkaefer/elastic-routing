IDIR = /usr/local/include
ODIR = build
SRCDIR = src
DEMODIR = demo
TARGETDIR = bin

CC = gcc
# CFLAGS = -I$(IDIR) -DDEBUG -DWITHSTATS -DWITHLOG -g -Wall -O2
# CFLAGS = -I$(IDIR) -DDEBUG -g -Wall -O2
CFLAGS = -I$(IDIR) -g -Wall
CFLAGS += -DDEBUG -DWITHSTATS
CFLAGS += -O3
CFLAGS += -std=c99
CFLAGS += -lm

# LDIR = -L./lib3rd/libyaml
# LIBS = -lyaml
LDIR = -L/usr/local/lib
LIBS = -lczmq -lzmq

# _OBJ = numeric_ext.o \
# 	   string_ext.o \
# 	   date_ext.o \
# 	   arrayi.o \
# 	   arrayu.o \
# 	   util.o \
# 	   entropy.o \
# 	   rng.o \
# 	   timer.o \
# 	   matrixd.o \
# 	   queue.o \
# 	   hash.o \
# 	   arrayset.o \
# 	   coord2d.o \
# 	   listu.o \
# 	   listx.o \
# 	   route.o \
# 	   vrp.o \
# 	   evol.o \
# 	   tspi.o \
# 	   tsp.o

_MODULES = numeric_ext \
	       string_ext \
	       date_ext \
	       arrayi \
	       arrayu \
	       util \
	       deps/pcg/entropy \
	       rng \
	       timer \
	       matrixd \
	       matrixu \
	       queue \
	       hash \
	       arrayset \
	       coord2d \
	       listu \
	       listx \
	       route \
	       vrp \
	       solution \
	       evol \
	       tspi \
	       tsp


_OBJ = $(_MODULES:=.o)
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))
# _LIBSRC = numeric_ext.c string_ext.c date_ext.c arrayi.c arrayu.c util.c deps/pcg/entropy.c rng.c timer.c matrixd.c queue.c hash.c arrayset.c coord2d.c listu.c listx.c route.c vrp.c evol.c tspi.c tsp.c
_LIBSRC = $(_MODULES:=.c)
LIBSRC = $(patsubst %,$(SRCDIR)/%,$(_LIBSRC))
LIBTARGET = liber.a
TESTSRC = $(SRCDIR)/selftest.c $(LIBSRC)
TESTTARGET = $(TARGETDIR)/selftest
EXESRC = $(SRCDIR)/main.c $(LIBSRC)
EXETARGET = $(TARGETDIR)/er
DEMOSRC = $(DEMODIR)/demo.c
DEMOTARGET = $(TARGETDIR)/demo

FILES_IN = abc def
FILES_OUT = $(FILES_IN:=.o)


.PHONY: clib cdll test exe demo pylib clean

$(ODIR)/%.o: $(SRCDIR)/%.c
	$(CC) -c $< -o $@ $(CFLAGS)

test:
	$(CC) $(TESTSRC) -o $(TESTTARGET) $(CFLAGS) $(LDIR) $(LIBS)
	$(TESTTARGET)

clib: $(OBJ)
	ar rcs $(LIBTARGET) $(ODIR)/*.o

cdll: $(OBJ)
	$(CC) -shared -o $@ $^ -Wl,--out-implib,$(LIBTARGET) $(LDIR) $(LIBS)

exe:
	$(CC) $(EXESRC) -o $(EXETARGET) $(CFLAGS) $(LDIR) $(LIBS)

demo: clib
	$(CC) -o $(DEMOTARGET) $(DEMOSRC) -Wall -O2 -L. $(LIBTARGET)

pylib:
	python py/setup.py build_ext --inplace

clean:
	rm -rf $(TARGETDIR)/* *.dll *.a *so $(ODIR)/* py/*.c
