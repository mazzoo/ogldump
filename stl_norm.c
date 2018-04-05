/*
 * stl_norm.c - normalize multiple STL files
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * authors:
 * (C) 2010  Matthias Wenzel <reprap at mazzoo dot de>
 *
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>


#define BB_SZ 4.0 /* bounding box max scale */

char stl_header[80] =
"Hi stranger. I am the STL header, "
"my contents are normalized "
"but len is 80 bytes";

FILE     * f;
uint32_t   n_file = 0;    /* number of triangles in the file */
float    * t_file = NULL; /* triagles from file */

#if 0
int opt_scale = 0;
int opt_x_off = 0;
int opt_y_off = 0;
int opt_z_off = 0;
#endif

int file_count = 0;


void read_file(char * fname)
{
    int ret;
    f = fopen(fname, "r+");
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

void scale_file(void)
{
    float * p = t_file;

    float x_max = -1E6, y_max = -1E6, z_max = -1E6;
    float x_min =  1E6, y_min =  1E6, z_min =  1E6;

    float x_off = 0.0, y_off = 0.0, z_off = 0.0;

    float max_w;
    float scale;

    int i;
    for (i=0; i<n_file; i++) {
        p += 3; /* skip normals */
        if (*p > x_max) x_max = *p;
        if (*p < x_min) x_min = *p;
        p++;
        if (*p > y_max) y_max = *p;
        if (*p < y_min) y_min = *p;
        p++;
        if (*p > z_max) z_max = *p;
        if (*p < z_min) z_min = *p;
        p++;
        if (*p > x_max) x_max = *p;
        if (*p < x_min) x_min = *p;
        p++;
        if (*p > y_max) y_max = *p;
        if (*p < y_min) y_min = *p;
        p++;
        if (*p > z_max) z_max = *p;
        if (*p < z_min) z_min = *p;
        p++;
        if (*p > x_max) x_max = *p;
        if (*p < x_min) x_min = *p;
        p++;
        if (*p > y_max) y_max = *p;
        if (*p < y_min) y_min = *p;
        p++;
        if (*p > z_max) z_max = *p;
        if (*p < z_min) z_min = *p;
        p++;
        p = (float *)((char *)p + 2);  /* skip unused */
    }
#if 0
    printf("\t[%2.2f, %2.2f] [%2.2f, %2.2f] [%2.2f, %2.2f] bounding box\n",
            x_min, x_max,
            y_min, y_max,
            z_min, z_max
          );
#endif
    x_off = x_max - x_min; /* not yet */
    y_off = y_max - y_min; /* the     */
    z_off = z_max - z_min; /* offset  */

    max_w = x_off;
    if (y_off > max_w) max_w = y_off;
    if (z_off > max_w) max_w = z_off;
    scale = BB_SZ / max_w;
#if 0
    printf("\t[%2.2f] [%2.2f] [%2.2f] dimensions (max=%2.2f) (scale=%2.2f)\n",
            x_off,
            y_off,
            z_off,
            max_w,
            scale
          );
#endif
    x_off /= 2.0;
    y_off /= 2.0;
    z_off /= 2.0;

    x_off += x_min; /* x and y         */
    y_off += y_min; /* around 0, but   */
    z_off  = z_min; /* positive z only */

    p = t_file;
    for (i=0; i<n_file; i++) {
        p += 3; /* skip normals */
        *p -= x_off;
        *p *= scale;
        p++;
        *p -= y_off;
        *p *= scale;
        p++;
        *p -= z_off;
        *p *= scale;
        p++;
        *p -= x_off;
        *p *= scale;
        p++;
        *p -= y_off;
        *p *= scale;
        p++;
        *p -= z_off;
        *p *= scale;
        p++;
        *p -= x_off;
        *p *= scale;
        p++;
        *p -= y_off;
        *p *= scale;
        p++;
        *p -= z_off;
        *p *= scale;
        p++;
        p = (float *)((char *)p + 2);  /* skip unused */
    }
}

void write_file(char * fname)
{
    int ret;

    printf("+++ writing STL file %s with %d triangles\n",
            fname, n_file);
    fseek(f, 0, SEEK_SET);
    if (80 != fwrite(stl_header, 1, 80, f)) {
        printf("!!! couldn't fwrite(%s) 1: %s\n",
                fname, strerror(errno));
        exit(1);
    }
    if (1 != fwrite(&n_file, 4, 1, f)) {
        printf("!!! couldn't fwrite(%s) 2: %s\n",
                fname, strerror(errno));
        exit(1);
    }
    // FIXME multiple fwrites
    if (n_file != (ret = fwrite(t_file, 50, n_file, f))) {
        printf("ret = %d / %d\n", ret, n_file);
        printf("!!! couldn't fwrite(%s) 3: %s\n",
                fname, strerror(errno));
        exit(1);
    }
}

void free_file(void)
{
    fclose(f);
    free(t_file);
    t_file = NULL;
    f = NULL;
    file_count++;
}

void usage(void)
{
    printf("\nstl_norm - normalize .stl files\ni\n");
    printf("usage: stl_norm inputfile(s)\n");

}

void stl_norm(int argc, char ** argv)
{
    int i;
    if (!argc) {
        /* FIXME all .stl files in cwd */
    } else {
        for (i=0; i<argc; i++) {
            read_file(argv[i]);
            scale_file();
            write_file(argv[i]);
            free_file();
        }
    }
}

int main(int argc, char ** argv)
{
#if 0
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
#endif

    stl_norm(argc-optind, &argv[optind]);
    return 0;
}

