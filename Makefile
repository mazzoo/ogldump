CFLAGS=-Wall -g

all: ogldump.so stl_process stl_bin2ascii


ogldump.so:ogldump.c
	$(CC) $(CFLAGS) -shared -ldl -o $@ $^

#stl_process: stl_process.c
#	$(CC) $(CFLAGS) $^ -o $@
#stl_bin2ascii: stl_bin2ascii.c
#	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f ogldump.so stl_process stl_bin2ascii

test:ogldump.so
	LD_PRELOAD=$(PWD)/ogldump.so glxgears

eltest:ogldump.so
	LD_PRELOAD=$(PWD)/ogldump.so ~/CODE/el/elc/el.x86.linux.bin
