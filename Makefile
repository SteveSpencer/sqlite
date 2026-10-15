#
# Makefile for addon/sqlite3 library
#
#

CIRCLEHOME = ../..
DEFINE += -DSQLITE_OMIT_DEPRECATED -DSQLITE_OMIT_WAL -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_OS_OTHER=1 -DSQLITE_CIRCLE
OBJS	= sqlite3.o circlevfs.o stdfuncs.o

libsqlite3.a: $(OBJS)
	@echo "  AR    $@"
	@rm -f $@
	@$(AR) cr $@ $(OBJS)

include $(CIRCLEHOME)/Rules.mk

-include $(DEPS)
