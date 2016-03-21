GCC = gcc
CFLAGS = -g -Wall 
LIBS = -lsmv_lib -lpthread

all: tc_ribbon_create tc_memdom_create \
	 privs_t1

tc_ribbon_create:
	$(GCC) $(CFLAGS) $@.c -o $@.o $(LIBS)
tc_memdom_create:
	$(GCC) $(CFLAGS) $@.c -o $@.o $(LIBS)
privs_t1:
	$(GCC) $(CFLAGS) $@.c -o $@.o $(LIBS)
clean:
	rm *.o
