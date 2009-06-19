#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#define OUT_FNAME "out.stl"

char stl_header[80] =
	"Hi stranger. I am the STL header, "
	"my contents are pointless, "
	"but len is 80 bytes";

FILE     * f;
uint32_t   n_file = 0;    /* number of triangles in the file */
uint32_t   n_tot  = 0;    /* number of triangles, all of them */
float    * t_file = NULL; /* triagles from file */
float    * t_tot  = NULL; /* triagles, all of them */

void read_file(char * fname)
{
	int ret;
	f = fopen(fname, "rw");
	if (!f) {
		printf("!!! couldn't fopen(%s): %s\n",
			fname,
			strerror(errno));
		exit(1);
	}
	ret = fseek(f, 80, SEEK_SET);
	if (ret) {
		printf("!!! couldn't wind 80 bytes into %s: %s\n",
			fname,
			strerror(errno));
		exit(1);
	}
	if (1 != fread(&n_file, 4, 1, f)) {
		printf("!!! couldn't read byte 80-84 of %s: %s\n",
			fname,
			strerror(errno));
		exit(1);
	}
	printf("+++ %s has %d triangles\n", fname, n_file);
	t_file = malloc(n_file * 50);
	if (!t_file) {
		printf("!!! couldn't malloc %d bytes: %s\n",
			n_file * 50,
			strerror(errno));
		exit(1);
	}
	// FIXME multiple freads
	if (n_file != fread(t_file, 50, n_file, f)) {
		printf("!!! couldn't read %d bytes of %s: %s\n",
			n_file * 50,
			fname,
			strerror(errno));
		free(t_file);
		exit(1);
	}
}

void append_file(void)
{
	int n_old = n_tot;
	if (!t_tot) {
		t_tot = malloc(50 * n_file);
	} else {
		t_tot = realloc(t_tot, (n_tot + n_file)*50);
	}
	if (!t_tot) {
		printf("!!! couldn't (re)alloc %d bytes to %d bytes: %s\n",
			n_tot,
			(n_tot + n_file) * 50,
			strerror(errno));
		exit(1);
	}
	n_tot += n_file;
	memcpy((char *)t_tot + n_old * 50, t_file, n_file * 50);
}

void free_file(void)
{
	fclose(f);
	free(t_file);
	t_file = NULL;
	f = NULL;
}
void write_file(void)
{
	int ret;

	printf("+++ writing STL file %s with %d triangles\n",
		OUT_FNAME, n_tot);
	f = fopen(OUT_FNAME, "w");
	if (!f) {
		printf("!!! couldn't fopen(%s): %s\n",
			OUT_FNAME, strerror(errno));
		exit(1);
	}
	if (80 != fwrite(stl_header, 1, 80, f)) {
		printf("!!! couldn't fwrite(%s) 1: %s\n",
			OUT_FNAME, strerror(errno));
		exit(1);
	}
	if (1 != fwrite(&n_tot, 4, 1, f)) {
		printf("!!! couldn't fwrite(%s) 2: %s\n",
			OUT_FNAME, strerror(errno));
		exit(1);
	}
	// FIXME multiple fwrites
	if (n_tot != (ret = fwrite(t_tot, 50, n_tot, f))) {
		printf("ret = %d / %d\n", ret, n_tot);
		printf("!!! couldn't fwrite(%s) 3: %s\n",
			OUT_FNAME, strerror(errno));
		exit(1);
	}
	fclose(f);

}

void usage(void)
{
	printf("usage: you not this way.\n");
}

void stl_scale(int argc, char ** argv)
{
	printf("stl_scale\n");
}

void stl_merge(int argc, char ** argv)
{
	int i;
	printf("stl_merge\n");
	if (!argc) {
		/* all .stl files in cwd */
	} else {
		for (i=0; i<argc; i++) {
			read_file(argv[i]);
			append_file();
			free_file();
		}
		write_file();
	}
}

int main(int argc, char ** argv)
{
	char * base = basename(argv[0]);
	if (!strncmp(base, "stl_scale", 9))
		stl_scale(argc-1, &argv[1]);
	else if (!strncmp(base, "stl_merge", 9))
		stl_merge(argc-1, &argv[1]);
	else
		usage();
	return 0;
}

