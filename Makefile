CFLAGS=-Wall -g

all: ogldump.so stl_process


ogldump.so:ogldump.c
	$(CC) $(CFLAGS) -shared -ldl -o $@ $^

stl_process: stl_process.c
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm ogldump.so stl_process

test:ogldump.so
	LD_PRELOAD=$(PWD)/ogldump.so glxgears

eltest:ogldump.so
	LD_PRELOAD=$(PWD)/ogldump.so ~/CODE/el/elc/el.x86.linux.bin
