all: ogldump.so


ogldump.so:ogldump.c
	$(CC) -Wall -g -shared -ldl -o $@ $^

clean:
	rm ogldump.so

test:ogldump.so
	LD_PRELOAD=$(PWD)/ogldump.so glxgears

eltest:ogldump.so
	LD_PRELOAD=$(PWD)/ogldump.so ~/CODE/el/elc/el.x86.linux.bin
