CFLAGS=-Wall -g -O2

all: ogldump.so stl_process stl_bin2ascii stl_norm


ogldump.so:ogldump.c
	$(CC) $(CFLAGS) -fPIC -shared -ldl -o $@ $^

clean:
	rm -f ogldump.so stl_process stl_bin2ascii

test:ogldump.so
	LD_PRELOAD=$(PWD)/ogldump.so glxgears
