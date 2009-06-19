CFLAGS=-Wall -g

all: ogldump.so stl_scale stl_merge


ogldump.so:ogldump.c
	$(CC) $(CFLAGS) -shared -ldl -o $@ $^

stl_scale: stl_scale.c
	$(CC) $(CFLAGS) $^ -o $@

stl_merge: stl_scale
	ln -s stl_scale stl_merge

clean:
	rm ogldump.so

test:ogldump.so
	LD_PRELOAD=$(PWD)/ogldump.so glxgears

eltest:ogldump.so
	LD_PRELOAD=$(PWD)/ogldump.so ~/CODE/el/elc/el.x86.linux.bin
