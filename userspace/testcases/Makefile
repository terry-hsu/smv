GCC = gcc
CFLAGS = -g -Wall 
LIBS = -lsmv_lib -lpthread

all: tc_ribbon_create tc_memdom_create \
	 privs_t1 tc_ribbon_thread_create \
	 tc_shared_mm tc_memdom_alloc \
	 tc_mutex tc_cond_var 


tc_ribbon_create:
	$(GCC) $(CFLAGS) $@.c -o $@.o $(LIBS)

tc_memdom_create:
	$(GCC) $(CFLAGS) $@.c -o $@.o $(LIBS)

privs_t1:
	$(GCC) $(CFLAGS) $@.c -o $@.o $(LIBS)

tc_ribbon_thread_create:
	$(GCC) $(CFLAGS) $@.c -o $@.o $(LIBS)

tc_shared_mm:
	$(GCC) $(CFLAGS) $@.c -o $@.o $(LIBS)

tc_mutex:
	$(GCC) $(CFLAGS) $@.c -o $@.o $(LIBS)

tc_cond_var:
	$(GCC) $(CFLAGS) $@.c -o $@.o $(LIBS)

tc_memdom_alloc:
	$(GCC) $(CFLAGS) $@.c -o $@.o $(LIBS)

clean:
	rm *.o
