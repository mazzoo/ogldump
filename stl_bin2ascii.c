/*
 * stl_bin2ascii.c - convert STL files from binary to ASCII format
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * authors:
 * (C) 2009  Matthias Wenzel <reprap at mazzoo dot de>
 *
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

char  * fname;
FILE  * f;
float * t;     /* triangles */
int     n_tri; /* number of triangles */

void usage(void)
{
    printf("usage: anders\n");
    exit(1);
}

int main(int argc, char ** argv)
{
    int ret, i;
    f = fopen(argv[1], "r");
    if (!f) {
        printf("!!! couldn't fopen(%s) : %s\n",
                argv[1], strerror(errno));
        exit(1);
    }
    ret = fseek(f, 80, SEEK_SET);
    if (ret) {
        printf("!!! couldn't fseek(%s) to 80 : %s\n",
                argv[1], strerror(errno));
        fclose(f);
        exit(1);
    }
    ret = fread(&n_tri, 4, 1, f);
    if (ret!=1) {
        printf("!!! couldn't fread(%s): %s\n",
                argv[1], strerror(errno));
        fclose(f);
        exit(1);
    }
    t = malloc(n_tri*50);
    ret = fread(t, 50, n_tri, f);
    if (ret!=n_tri) {
        printf("!!! couldn't fread(%s) triangles: %s\n",
                argv[1], strerror(errno));
        fclose(f);
        exit(1);
    }

    printf("solid %s\n", argv[1]);
    float * p;
    for (p=t, i=0; i<n_tri; i++) {
        printf("facet normal %E %E %E\n", p[0], p[1], p[2]);
        printf(" outer loop\n");
        printf("  vertex %E %E %E\n", p[3], p[4], p[5]);
        printf("  vertex %E %E %E\n", p[6], p[7], p[8]);
        printf("  vertex %E %E %E\n", p[9], p[10], p[11]);
        printf(" endloop\n");
        printf("endfacet\n");
        p = (float *)((char *)p+50);
    }
    printf("endsolid %s\n", argv[1]);

    fclose(f);
    return 0;
}
