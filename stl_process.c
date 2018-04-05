/*
 * stl_process.c - merge, scale, add offsets to multiple STL files
 *                 resulting in out.stl
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * authors:
 * (C) 2009  Matthias Wenzel <reprap at mazzoo dot de>
 *
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

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

int opt_scale = 0;
int opt_x_off = 0;
int opt_y_off = 0;
int opt_z_off = 0;

double scale = 1.0;
int x_off = 0;
int y_off = 0;
int z_off = 0;
int file_count = 0;


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
    int i, j;
    float * p;

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

    if (opt_x_off) {
        p = t_file;
        for (i=0; i<n_file; i++) {
            p += 3; /* skip normals */
            for (j=0; j<3; j++){
                *p += x_off * file_count;
                p  += 3;
            }
            p = (float *)((char *)p + 2);  /* skip unused */
        }
    }

    if (opt_y_off) {
        p = t_file;
        p++;
        for (i=0; i<n_file; i++) {
            p += 3; /* skip normals */
            for (j=0; j<3; j++){
                *p += y_off * file_count;
                p  += 3;
            }
            p = (float *)((char *)p + 2);  /* skip unused */
        }
    }

    if (opt_z_off) {
        p = t_file;
        p++;
        p++;
        for (i=0; i<n_file; i++) {
            p += 3; /* skip normals */
            for (j=0; j<3; j++){
                *p += z_off * file_count;
                p  += 3;
            }
            p = (float *)((char *)p + 2);  /* skip unused */
        }
    }

    memcpy((char *)t_tot + n_old * 50, t_file, n_file * 50);
}

void free_file(void)
{
    fclose(f);
    free(t_file);
    t_file = NULL;
    f = NULL;
    file_count++;
}

void write_file(void)
{
    int ret, i, j;
    float * p;

    if (opt_scale) {
        printf("+++ scaling by %lf\n", scale);
        p = t_tot;
        for (i=0; i<n_tot; i++) {
            p += 3; /* skip normals */
            for (j=0; j<9; j++)
                *p++ *= scale;
            p = (float *)((char *)p + 2);  /* skip unused */
        }
    }

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
    printf("\nstl_process - scale and merge .stl files\n");
    printf("              the input file(s) won't be modified,\n");
    printf("              all is written to %s\n\n", OUT_FNAME);
    printf("usage: stl_process [options] inputfile(s)\n");
    printf("options:\n");
    printf("\t-s factor : scale output by factor\n");
    printf("\t-x x_off  : when merging, add x_off per input file in the output\n");
    printf("\t-y y_off  : when merging, add y_off per input file in the output\n");
    printf("\t-z z_off  : when merging, add z_off per input file in the output\n");
    printf("\t-h        : show this help\n");

}

void stl_process(int argc, char ** argv)
{
    int i;
    if (!argc) {
        /* FIXME all .stl files in cwd */
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
    char optchar;
    char * endptr;

    while ((optchar = getopt (argc, argv, "s:x:y:z:h")) != -1)
    {
        switch (optchar) {
            case 's':
                opt_scale = 1;
                scale = strtod(optarg, &endptr);
                if (endptr == optarg) {
                    usage();
                    exit(1);
                }
                break;
            case 'x':
                opt_x_off = 1;
                x_off = strtol(optarg, &endptr, 0);
                if (endptr == optarg) {
                    usage();
                    exit(1);
                }
                printf("+++ using x offset %d\n", x_off);
                break;
            case 'y':
                opt_y_off = 1;
                y_off = strtol(optarg, &endptr, 0);
                if (endptr == optarg) {
                    usage();
                    exit(1);
                }
                printf("+++ using y offset %d\n", y_off);
                break;
            case 'z':
                opt_z_off = 1;
                z_off = strtol(optarg, &endptr, 0);
                if (endptr == optarg) {
                    usage();
                    exit(1);
                }
                printf("+++ using z offset %d\n", z_off);
                break;
            case 'h':
                usage();
                exit(0);
                break;
        }
    }

    stl_process(argc-optind, &argv[optind]);
    return 0;
}

