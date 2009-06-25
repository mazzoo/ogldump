/*
 * ogldump.c / ogldump.so - an OpenGL-to-STL recorder
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
#include <stdlib.h>
//#include <netdb.h>
//#include <unistd.h>
#include <string.h>
//#include <sys/types.h>
//#include <sys/stat.h>
//#include <fcntl.h>
#include <dlfcn.h>

#include <GL/gl.h>
#include <GL/glx.h>

#define FNAME_PREFIX "/usr/matze"
char stl_header[80] =
	"Hi stranger. I am the STL header, "
	"my contents are pointless, "
	"but len is 80 bytes";

/**************************************************************/
/* recording opengl primitives */

char * prim_type_name[0x0a] = {
	"GL_POINTS",
	"GL_LINES",
	"GL_LINE_LOOP",
	"GL_LINE_STRIP",
	"GL_TRIANGLES",
	"GL_TRIANGLE_STRIP",
	"GL_TRIANGLE_FAN",
	"GL_QUADS",
	"GL_QUAD_STRIP",
	"GL_POLYGON"
};

struct vertex_t {
	float x;
	float y;
	float z;
};

struct triangle_t {
	float xn;
	float yn;
	float zn;
	float x1;
	float y1;
	float z1;
	float x2;
	float y2;
	float z2;
	float x3;
	float y3;
	float z3;
};

int nNorm = 0;
struct N3_t {
	struct N3_t   * next;

	struct vertex_t v;
} * norm_last, * norm;

struct V3_t {
	struct V3_t   * next;
	struct N3_t   * norm;

	struct vertex_t v;
};

struct vertexpointer_t {
	struct vertexpointer_t  * next;

	/* arguments from glVertexPointer() */
	GLint                   size;
	GLenum                  type;
	GLsizei                 stride;
	const GLvoid          * ptr;

	void                  * ptr_copy;
	int                     sizeof_type;
	int                     max_index;
	struct triangle_t     * tri;
} * vertexpointer, * all_vertexpointer;

struct drawelements_t {
	struct drawelements_t  * next;

	GLenum                   mode;
	GLsizei                  count;
	GLenum                   type;
	GLvoid                 * indices;

	int                      sizeof_type;
	struct N3_t            * norm;
	struct vertexpointer_t * vertexpointer;
} * drawelements, * all_drawelements;

int nPrim = 0;
struct prim_t {
	struct prim_t * next;
	int              nV3;
	struct V3_t    * V3;
	int              type; /* one of the primitives GL_POINTS, GL_LINES, ... */
} * all_prims;

void enomem(void)
{
	printf("out of memory\n");
	exit(1);
}

struct prim_t * current_prim = NULL;
struct prim_t * new_prim(GLenum type)
{
	struct prim_t * p = all_prims;
	if (!p) {
		p = malloc(sizeof(*p));
		all_prims = p;
	} else {
		while(p->next)
			p = p->next;
		p->next = malloc(sizeof(*p));
		p = p->next;
	}
	if (!p) enomem();
	p->next = NULL;
	p->nV3  = 0;
	p->V3   = NULL;
	p->type = type;
	nPrim++;
	return p;
}

void new_V3(
		float x,
		float y,
		float z)
{
	if (!current_prim) {
		printf("!!! ignoring V3 outside of prim\n");
		return;
	}
	struct prim_t * p = current_prim;
	struct V3_t * v = p->V3;
	if (!v) {
		v = malloc(sizeof(*v));
		p->V3 = v;
	} else {
		while (v->next)
			v = v->next;
		v->next = malloc(sizeof(*v));
		v = v->next;
	}
	if (!v) enomem();
	v->next = NULL;
	v->norm = norm_last;
	v->v.x = x;
	v->v.y = y;
	v->v.z = z;
	p->nV3++;
}

void new_N3(
		float x,
		float y,
		float z)
{
	struct N3_t * p = norm_last;
	if (!p) {
		p = malloc(sizeof(*p));
		norm = p;
	} else {
		p->next = malloc(sizeof(*p));
		p = p->next;
	}
	if (!p) enomem();
	p->next = NULL;
	p->v.x = x;
	p->v.y = y;
	p->v.z = z;
	norm_last = p;
}

void copy_vertexpointer(void)
{
	struct vertexpointer_t * v = vertexpointer;

	if (!v)
		return;
	if (!v->max_index)
		return;
	if (!v->ptr)
		return;

	v->ptr_copy = malloc((v->max_index + 1) * v->stride);
	if (!v->ptr_copy)
		enomem();
	memcpy(v->ptr_copy, v->ptr, (v->max_index + 1) * v->stride);
}

void new_DrawElements( GLenum mode, GLsizei count,
		GLenum type, const GLvoid *indices )
{

	if (!indices) return;
	if (indices < (void *)0x08000000) return; /* fuckyou, OpenGL, FUCK FUCK FUCK you, fuck YOU! */

	if (mode != GL_TRIANGLES) {
		printf("!!! FIXME: support DrawElements() mode %d\n", mode);
		return;
	}

	if (type != GL_UNSIGNED_SHORT) {
		printf("!!! FIXME: support DrawElements() type %d\n", type);
		return;
	}

	struct drawelements_t * p = drawelements;
	struct drawelements_t * p_prev = NULL;
	if (!p) {
		p = malloc(sizeof(*p));
		all_drawelements = p;
	} else {
		p->next = malloc(sizeof(*p));
		p_prev = p;
		p = p->next;
	}
	if (!p) enomem();
	p->next = NULL;
	p->norm = norm_last;

	p->mode  = mode;
	p->count = count;
	p->type  = type;

	p->vertexpointer = vertexpointer;

	switch (mode) {
		case GL_TRIANGLES:
			{
			int sizeof_type = 0;
			switch (type) {
				case GL_UNSIGNED_BYTE:
					sizeof_type = 1;
					break;
				case GL_UNSIGNED_SHORT:
					sizeof_type = 2;
					break;
				case GL_UNSIGNED_INT:
					sizeof_type = 4;
					break;
				default:
					printf("!!! ERROR: glDrawElements() wit unknown type 0x%x\n",
						type);
					exit(1);
			}
			p->sizeof_type = sizeof_type;
			p->indices = malloc( sizeof_type * count );
			if (!p->indices) enomem();
			memcpy(p->indices, indices, sizeof_type * count );

#if 0
			printf("indices = { ");
			int i;
			for (i=0; i< count; i++) {
				short * s = p->indices;
				printf("%d, ", s[i]);
			}
			printf(indices = "};\n");
#endif
			int i;
			for (i=0; i<count; i++) {
				short * s = p->indices;
				if (s[i] > p->vertexpointer->max_index)
				{
					p->vertexpointer->max_index = s[i];
					//printf("new_max: %d\n", p->vertexpointer->max_index);
				}
			}

			break;
			}
		default:
			printf("!!! FIXME: support DrawElements(%s / %d)\n",
				prim_type_name[mode], mode);
			free(p);
			if (p_prev) {
				p_prev->next = NULL;
			} else {
				all_drawelements = NULL;
			}
			p = NULL;
			return;
	}

	copy_vertexpointer();
	drawelements = p;
}

void new_VertexPointer( GLint size, GLenum type,
		GLsizei stride, const GLvoid *ptr )
{
	if (size != 3) {
		printf("!!! unsupported new_VertexPointer() size %d\n", size);
		return;
	}
	if (type != GL_FLOAT) {
		printf("!!! unsupported new_VertexPointer() type %d\n", type);
		return;
	}

	struct vertexpointer_t * p = vertexpointer;

	if (!p) {
		p = malloc(sizeof(*p));
		all_vertexpointer = p;
	} else {
		p->next = malloc(sizeof(*p));
		p = p->next;
	}
	if (!p) enomem();
	p->next = NULL;

	p->size   = size;
	p->type   = type;
	p->stride = stride;
	p->ptr    = ptr;

	p->max_index = 0;

	p->sizeof_type = 0;
	switch (type) {
		case GL_SHORT:
			p->sizeof_type = 2;
			break;
		case GL_INT:
			p->sizeof_type = 4;
			break;
		case GL_FLOAT:
			p->sizeof_type = 4;
			break;
		case GL_DOUBLE:
			p->sizeof_type = 8;
			break;
		default:
			printf("!!! ERROR: unknown type %d in glVertexPointer()\n", type);
			exit(1);
	}

	vertexpointer = p;
}

/**************************************************************/
/* processing opengl primitives to STL normals and triangles */

int n_stl_triangles = 0;
void emit_stl_triangle(FILE * f, struct triangle_t t)
{
	n_stl_triangles++;

	fwrite(&t.xn, 4, 1, f);
	fwrite(&t.yn, 4, 1, f);
	fwrite(&t.zn, 4, 1, f);

	fwrite(&t.x1, 4, 1, f);
	fwrite(&t.y1, 4, 1, f);
	fwrite(&t.z1, 4, 1, f);
	fwrite(&t.x2, 4, 1, f);
	fwrite(&t.y2, 4, 1, f);
	fwrite(&t.z2, 4, 1, f);
	fwrite(&t.x3, 4, 1, f);
	fwrite(&t.y3, 4, 1, f);
	fwrite(&t.z3, 4, 1, f);

	uint16_t unused = 0;
	fwrite(&unused, 1, 2, f);
}

void fixup_stl(FILE * f, int n_triangles)
{
	fseek(f, 80, SEEK_SET);
	fwrite(&n_triangles, 1, 4, f);
	n_stl_triangles = 0;
}

/* consume N gl vertices, emit N/2 triangles */
/* N needs to be a factor of 4 */
void do_gl_quads(FILE * f, struct prim_t * prim)
{
	if (!prim) return;

	struct V3_t * p = prim->V3;
	int i;

	struct vertex_t   v[4];
	struct triangle_t t;

	while (p) {
		t.xn = p->norm->v.x;
		t.yn = p->norm->v.y;
		t.zn = p->norm->v.z;
		for (i=0; i<4; i++) {

			if (!p) {
				printf("!!! ERROR do_gl_quads()\n");
				exit(1); /* FIXME: be more gracefully */
			}

			v[i].x = p->v.x;
			v[i].y = p->v.y;
			v[i].z = p->v.z;

			p = p->next;
		}
		t.x1 = v[0].x;
		t.y1 = v[0].y;
		t.z1 = v[0].z;
		t.x2 = v[1].x;
		t.y2 = v[1].y;
		t.z2 = v[1].z;
		t.x3 = v[2].x;
		t.y3 = v[2].y;
		t.z3 = v[2].z;

		emit_stl_triangle(f, t);

		/* GL_QUADS have a strange sequence */
		t.x2 = v[3].x;
		t.y2 = v[3].y;
		t.z2 = v[3].z;

		emit_stl_triangle(f, t);
	}
}

void do_gl_quad_strip(FILE * f, struct prim_t * prim)
{
	if (!prim) return;

	struct V3_t * p = prim->V3;
	struct vertex_t   v[3];
	struct triangle_t t;

	/* get 1st two vertices */
	v[1].x = p->v.x;
	v[1].y = p->v.y;
	v[1].z = p->v.z;
	printf("got 1 v\n");
	p = p->next;
	if (!p) {
		printf("!!! ERROR do_gl_quad_strip()\n");
		exit(1); /* FIXME: be more gracefully */
	}
	v[2].x = p->v.x;
	v[2].y = p->v.y;
	v[2].z = p->v.z;
	printf("got 2 v\n");
	p = p->next;
	if (!p) {
		printf("!!! ERROR do_gl_quad_strip()\n");
		exit(1); /* FIXME: be more gracefully */
	}
	while (p) {
		memcpy(&v[0], &v[1], 12);
		memcpy(&v[1], &v[2], 12);
		v[2].x = p->v.x;
		v[2].y = p->v.y;
		v[2].z = p->v.z;

		t.xn = p->norm->v.x;
		t.yn = p->norm->v.y;
		t.zn = p->norm->v.z;

		t.x1 = v[0].x;
		t.y1 = v[0].y;
		t.z1 = v[0].z;
		t.x2 = v[1].x;
		t.y2 = v[1].y;
		t.z2 = v[1].z;
		t.x3 = v[2].x;
		t.y3 = v[2].y;
		t.z3 = v[2].z;

		emit_stl_triangle(f, t);

		p = p->next;
	}
}

void do_gl_triangles(FILE * f, struct prim_t * prim)
{
	if (!prim) return;

	struct V3_t * p = prim->V3;
	struct vertex_t   v[3];
	struct triangle_t t;

	while (p) {

		int i;
		for (i=0; i<4; i++) {

			if (!p) {
				printf("!!! ERROR do_gl_quads()\n");
				exit(1); /* FIXME: be more gracefully */
			}

			v[i].x = p->v.x;
			v[i].y = p->v.y;
			v[i].z = p->v.z;

			p = p->next;
		}

		t.xn = p->norm->v.x;
		t.yn = p->norm->v.y;
		t.zn = p->norm->v.z;

		t.x1 = v[0].x;
		t.y1 = v[0].y;
		t.z1 = v[0].z;
		t.x2 = v[1].x;
		t.y2 = v[1].y;
		t.z2 = v[1].z;
		t.x3 = v[2].x;
		t.y3 = v[2].y;
		t.z3 = v[2].z;

		emit_stl_triangle(f, t);

		p = p->next;
	}
}

void do_gl_triangle_fan(FILE * f, struct prim_t * prim)
{
	if (!prim) return;

	struct V3_t * p = prim->V3;
	struct vertex_t   v[3];
	struct triangle_t t;

	/* get 1st two vertices */
	v[0].x = p->v.x;
	v[0].y = p->v.y;
	v[0].z = p->v.z;
	p = p->next;
	if (!p) {
		printf("!!! ERROR do_gl_triangle_fan()\n");
		exit(1); /* FIXME: be more gracefully */
	}
	v[1].x = p->v.x;
	v[1].y = p->v.y;
	v[1].z = p->v.z;
	p = p->next;
	if (!p) {
		printf("!!! ERROR do_gl_triangle_fan()\n");
		exit(1); /* FIXME: be more gracefully */
	}
	while (p) {
		memcpy(&v[1], &v[2], 12);
		v[2].x = p->v.x;
		v[2].y = p->v.y;
		v[2].z = p->v.z;

		t.xn = p->norm->v.x;
		t.yn = p->norm->v.y;
		t.zn = p->norm->v.z;

		t.x1 = v[0].x;
		t.y1 = v[0].y;
		t.z1 = v[0].z;
		t.x2 = v[1].x;
		t.y2 = v[1].y;
		t.z2 = v[1].z;
		t.x3 = v[2].x;
		t.y3 = v[2].y;
		t.z3 = v[2].z;

		emit_stl_triangle(f, t);

		p = p->next;
	}
}

void switch_gl_primitive(int n, struct prim_t * prim)
{
	char fnamebuf[256];
	sprintf(fnamebuf, "%s/prim_%d.stl", FNAME_PREFIX, n);
	FILE * f = fopen(fnamebuf, "wb");
	if (!f) {
		printf("!!! couldn't fopen(%s)\n", fnamebuf);
		return;
	}

	fwrite(stl_header, 1, 80, f);
	int vertices = 0; /* will be written again by fixup_stl() */
	fwrite(&vertices, 4, 1, f);

	switch(prim->type) {
#if 0
		case GL_POINTS:
			do_gl_quads(f, prim);
			break;
#endif
		case GL_QUADS:
			do_gl_quads(f, prim);
			break;
		case GL_QUAD_STRIP:
			do_gl_quad_strip(f, prim);
			break;
		case GL_TRIANGLES:
			do_gl_triangles(f, prim);
			break;
		case GL_TRIANGLE_FAN:
			do_gl_triangle_fan(f, prim);
			break;
#if 0
		case GL_LINE_LOOP:
			do_gl_triangle_fan(f, prim);
			break;
#endif
		default:
			printf("!!! FIXME implement type %d / %s\n",
				prim->type, prim_type_name[prim->type]);
	}
	fixup_stl(f, n_stl_triangles);
	fclose(f);
}

void do_file_DrawElements(int n, struct drawelements_t * p)
{
	char fnamebuf[256];
	sprintf(fnamebuf, "%s/drawelements_%d.stl", FNAME_PREFIX, n);
	FILE * f = fopen(fnamebuf, "wb");
	if (!f) {
		printf("!!! couldn't fopen(%s)\n", fnamebuf);
		return;
	}

	fwrite(stl_header, 1, 80, f);

	int vertices = 0; /* will be written again by fixup_stl() */
	fwrite(&vertices, 4, 1, f);

	int n_drawelements = 0;
	void * pv = p->vertexpointer->ptr_copy;

#if 0
	static int didd=0;
	if (didd< 20)
	{
		int i;
		printf("DE stride = %d\n", p->vertexpointer->stride);
		for (i=0; i<p->vertexpointer->max_index; i++)
		{
			if (!(i%(p->vertexpointer->stride/4)))
				printf("\n");
			printf("%+4.2f ", ((float *)pv)[i]);
		}

		didd++;
		printf("\n");
	}
#endif
	switch (p->mode) {
		case GL_TRIANGLES:
			{
			struct triangle_t t;
			int stride  = p->vertexpointer->stride / 4;
			short * ind = p->indices;
			int i;
			for (i=0; i<p->count ; i+=3) {

				t.xn = p->norm->v.x;
				t.yn = p->norm->v.y;
				t.zn = p->norm->v.z;

				t.x1 = ((float *)pv)[0 + stride*ind[i+0]];
				t.y1 = ((float *)pv)[1 + stride*ind[i+0]];
				t.z1 = ((float *)pv)[2 + stride*ind[i+0]];
				t.x2 = ((float *)pv)[0 + stride*ind[i+1]];
				t.y2 = ((float *)pv)[1 + stride*ind[i+1]];
				t.z2 = ((float *)pv)[2 + stride*ind[i+1]];
				t.x3 = ((float *)pv)[0 + stride*ind[i+2]];
				t.y3 = ((float *)pv)[1 + stride*ind[i+2]];
				t.z3 = ((float *)pv)[2 + stride*ind[i+2]];

#if 0
	if (didd< 20) {
				printf("emit_stl_triangle %d  %d\n", n, n_drawelements);
				printf("%+2.2f %+2.2f %+2.2f\n", t.x1, t.y1, t.z1);
				printf("%+2.2f %+2.2f %+2.2f\n", t.x2, t.y2, t.z2);
				printf("%+2.2f %+2.2f %+2.2f\n", t.x3, t.y3, t.z3);
	}
#endif
				n_drawelements++;
				emit_stl_triangle(f, t);
			}
			n++;
			break;
			} 
		default:
			// FIXME
			printf("!!! FIXME implement DrawElements() mode %d\n", p->mode);
	}

	fixup_stl(f, n_drawelements);
	fclose(f);
}

void do_DrawElements(void)
{
	struct drawelements_t * p = all_drawelements;
	int n = 0;
	while (p) {
//		printf("+++ writing DrawElement %d\n", n);
		do_file_DrawElements(n, p);
		n++;
		p = p->next;
	}
	printf("+++ wrote a total of %d DrawElements\n", n);
}

/**************************************************************/
/* init */

void ogldump_exit(void)
{

	printf("+++ ogldump report\n");

	printf("+++ got %d prims\n", nPrim);

	int n=0;
	int large=0;
	struct prim_t * p = all_prims;
	while (p) {
//		if (p->nV3 > 256) {
		if (p->nV3 > 0) {
			printf("+++ prim %d has %d vertices\n", n, p->nV3);
			large++;
			//dump_prim(n, p);
			switch_gl_primitive(n, p);
		}
		n++;
		p = p->next;
	}

	do_DrawElements();

	printf("+++ wrote %d large prims\n", large);
	printf("+++ byebye from ogldump.\n\n");
}

static int is_initialized = 0;

static inline void init(void)
{
	if (is_initialized)
		return;
	atexit(ogldump_exit);

	all_prims         = NULL;
	norm              = NULL;
	norm_last         = NULL;
	drawelements      = NULL;
	all_drawelements  = NULL;
	all_vertexpointer = NULL;
	vertexpointer     = NULL;

	/* an initial default normal */
	new_N3(0.0, 0.0, 1.0);

	is_initialized++;
}

/**************************************************************/
/* hijacked functions */

//#define DO_2D_VERTEX
//#define DO_3D_VERTEX
//#define DO_4D_VERTEX
#define DO_3D_NORMAL
#define DO_DRAW_ELEMENTS

#define glvoid __attribute__((visibility("default"))) void

#if 0
glvoid glNewList( GLuint list, GLenum mode )
{
	init();
	static void (*func)(GLuint, GLenum) = NULL;
	if (!func)
		func = (void (*)(GLuint, GLenum)) dlsym(RTLD_NEXT, "glNewList");

	printf("glNewList(%d, %d);\n", list, mode);

	func(list, mode);
}
#endif

#if defined DO_2D_VERTEX || defined DO_3D_VERTEX || defined DO_4D_VERTEX
glvoid glBegin( GLenum mode )
{
	init();
	static void (*func)(GLenum) = NULL;
	if (!func)
		func = (void (*)(GLenum)) dlsym(RTLD_NEXT, "glBegin");

	printf("glBegin(%s);", prim_type_name[mode]);

	current_prim = new_prim(mode);

	printf(" /* [%d] */\n", nPrim-1);

	func(mode);
}

glvoid glEnd( void )
{
	init();
	static void (*func)(void) = NULL;
	if (!func)
		func = (void (*)(void)) dlsym(RTLD_NEXT, "glEnd");

	printf("glEnd();\n");

	current_prim = NULL;

	func();
}
#endif

#ifdef DO_2D_VERTEX
glvoid glVertex2d( GLdouble x, GLdouble y )
{
	init();
	static void (*func)(GLdouble, GLdouble) = NULL;
	if (!func)
		func = (void (*)(GLdouble, GLdouble)) dlsym(RTLD_NEXT, "glVertex2d");

	printf("glVertex2d(%f, %f);\n", x, y);

	func(x, y);
}

glvoid glVertex2f( GLfloat x, GLfloat y )
{
	init();
	static void (*func)(GLfloat, GLfloat) = NULL;
	if (!func)
		func = (void (*)(GLfloat, GLfloat)) dlsym(RTLD_NEXT, "glVertex2f");

	printf("glVertex2f(%f, %f);\n", x, y);

	func(x, y);
}

glvoid glVertex2i( GLint x, GLint y )
{
	init();
	static void (*func)(GLint, GLint) = NULL;
	if (!func)
		func = (void (*)(GLint, GLint)) dlsym(RTLD_NEXT, "glVertex2i");

	printf("glVertex2i(%d, %d);\n", x, y);

	func(x, y);
}

glvoid glVertex2s( GLshort x, GLshort y )
{
	init();
	static void (*func)(GLshort, GLshort) = NULL;
	if (!func)
		func = (void (*)(GLshort, GLshort)) dlsym(RTLD_NEXT, "glVertex2s");

	printf("glVertex2s(%d, %d);\n", x, y);

	func(x, y);
}
#endif

#ifdef DO_3D_VERTEX
glvoid glVertex3d( GLdouble x, GLdouble y, GLdouble z )
{
	init();
	static void (*func)(GLdouble, GLdouble, GLdouble) = NULL;
	if (!func)
		func = (void (*)(GLdouble, GLdouble, GLdouble)) dlsym(RTLD_NEXT, "glVertex3d");

	printf("glVertex3d(%f, %f, %f);\n", x, y, z);
	new_V3(x, y, z);

	func(x, y, z);
}

glvoid glVertex3f( GLfloat x, GLfloat y, GLfloat z )
{
	init();
	static void (*func)(GLfloat, GLfloat, GLfloat) = NULL;
	if (!func)
		func = (void (*)(GLfloat, GLfloat, GLfloat)) dlsym(RTLD_NEXT, "glVertex3f");

	printf("glVertex3f(%f, %f, %f);\n", x, y, z);
	new_V3(x, y, z);

	func(x, y, z);
}

glvoid glVertex3i( GLint x, GLint y, GLint z )
{
	init();
	static void (*func)(GLint, GLint, GLint) = NULL;
	if (!func)
		func = (void (*)(GLint, GLint, GLint)) dlsym(RTLD_NEXT, "glVertex3i");

	printf("glVertex3i(%d, %d, %d);\n", x, y, z);
	new_V3(x, y, z);

	func(x, y, z);
}

glvoid glVertex3s( GLshort x, GLshort y, GLshort z )
{
	init();
	static void (*func)(GLshort, GLshort, GLshort) = NULL;
	if (!func)
		func = (void (*)(GLshort, GLshort, GLshort)) dlsym(RTLD_NEXT, "glVertex3s");

	printf("glVertex3s(%d, %d, %d);\n", x, y, z);
	new_V3(x, y, z);

	func(x, y, z);
}
#endif

#ifdef DO_4D_VERTEX
glvoid glVertex4d( GLdouble x, GLdouble y, GLdouble z, GLdouble w )
{
	init();
	static void (*func)(GLdouble, GLdouble, GLdouble, GLdouble) = NULL;
	if (!func)
		func = (void (*)(GLdouble, GLdouble, GLdouble, GLdouble)) dlsym(RTLD_NEXT, "glVertex4d");

	printf("glVertex4d(%f, %f, %f, %f);\n", x, y, z, w);

	func(x, y, z, w);
}

glvoid glVertex4f( GLfloat x, GLfloat y, GLfloat z, GLfloat w )
{
	init();
	static void (*func)(GLfloat, GLfloat, GLfloat, GLfloat) = NULL;
	if (!func)
		func = (void (*)(GLfloat, GLfloat, GLfloat, GLfloat)) dlsym(RTLD_NEXT, "glVertex4f");

	printf("glVertex4f(%f, %f, %f, %f);\n", x, y, z, w);

	func(x, y, z, w);
}

glvoid glVertex4i( GLint x, GLint y, GLint z, GLint w )
{
	init();
	static void (*func)(GLint, GLint, GLint, GLint) = NULL;
	if (!func)
		func = (void (*)(GLint, GLint, GLint, GLint)) dlsym(RTLD_NEXT, "glVertex4i");

	printf("glVertex4i(%d, %d, %d, %d);\n", x, y, z, w);

	func(x, y, z, w);
}

glvoid glVertex4s( GLshort x, GLshort y, GLshort z, GLshort w )
{
	init();
	static void (*func)(GLshort, GLshort, GLshort, GLshort) = NULL;
	if (!func)
		func = (void (*)(GLshort, GLshort, GLshort, GLshort)) dlsym(RTLD_NEXT, "glVertex4s");

	printf("glVertex4s(%d, %d, %d, %d);\n", x, y, z, w);

	func(x, y, z, w);
}
#endif

#ifdef DO_2D_VERTEX
glvoid glVertex2dv( const GLdouble *v )
{
	init();
	static void (*func)(const GLdouble *) = NULL;
	if (!func)
		func = (void (*)(const GLdouble *)) dlsym(RTLD_NEXT, "glVertex2dv");

	printf("glVertex2dv(%f, %f);\n", v[0], v[1]);

	func(v);
}

glvoid glVertex2fv( const GLfloat *v )
{
	init();
	static void (*func)(const GLfloat *) = NULL;
	if (!func)
		func = (void (*)(const GLfloat *)) dlsym(RTLD_NEXT, "glVertex2fv");

	printf("glVertex2fv(%f, %f);\n", v[0], v[1]);

	func(v);
}

glvoid glVertex2iv( const GLint *v )
{
	init();
	static void (*func)(const GLint *) = NULL;
	if (!func)
		func = (void (*)(const GLint *)) dlsym(RTLD_NEXT, "glVertex2iv");

	printf("glVertex2iv(%d, %d);\n", v[0], v[1]);

	func(v);
}

glvoid glVertex2sv( const GLshort *v )
{
	init();
	static void (*func)(const GLshort *) = NULL;
	if (!func)
		func = (void (*)(const GLshort *)) dlsym(RTLD_NEXT, "glVertex2sv");

	printf("glVertex2sv(%d, %d);\n", v[0], v[1]);

	func(v);
}
#endif

#ifdef DO_3D_VERTEX
glvoid glVertex3dv( const GLdouble *v )
{
	init();
	static void (*func)(const GLdouble *) = NULL;
	if (!func)
		func = (void (*)(const GLdouble *)) dlsym(RTLD_NEXT, "glVertex3dv");

	printf("glVertex3dv(%f, %f, %f);\n", v[0], v[1], v[2]);
	new_V3(v[0], v[1], v[2]);

	func(v);
}

glvoid glVertex3fv( const GLfloat *v )
{
	init();
	static void (*func)(const GLfloat *) = NULL;
	if (!func)
		func = (void (*)(const GLfloat *)) dlsym(RTLD_NEXT, "glVertex3fv");

	printf("glVertex3fv(%f, %f, %f);\n", v[0], v[1], v[2]);
	new_V3(v[0], v[1], v[2]);

	func(v);
}

glvoid glVertex3iv( const GLint *v )
{
	init();
	static void (*func)(const GLint *) = NULL;
	if (!func)
		func = (void (*)(const GLint *)) dlsym(RTLD_NEXT, "glVertex3iv");

	printf("glVertex3iv(%d, %d, %d);\n", v[0], v[1], v[2]);
	new_V3(v[0], v[1], v[2]);

	func(v);
}

glvoid glVertex3sv( const GLshort *v )
{
	init();
	static void (*func)(const GLshort *) = NULL;
	if (!func)
		func = (void (*)(const GLshort *)) dlsym(RTLD_NEXT, "glVertex3sv");

	printf("glVertex3sv(%d, %d, %d);\n", v[0], v[1], v[2]);
	new_V3(v[0], v[1], v[2]);

	func(v);
}
#endif

#ifdef DO_4D_VERTEX
glvoid glVertex4dv( const GLdouble *v )
{
	init();
	static void (*func)(const GLdouble *) = NULL;
	if (!func)
		func = (void (*)(const GLdouble *)) dlsym(RTLD_NEXT, "glVertex4dv");

	printf("glVertex4dv(%f, %f, %f, %f);\n", v[0], v[1], v[2], v[4]);

	func(v);
}

glvoid glVertex4fv( const GLfloat *v )
{
	init();
	static void (*func)(const GLfloat *) = NULL;
	if (!func)
		func = (void (*)(const GLfloat *)) dlsym(RTLD_NEXT, "glVertex4fv");

	printf("glVertex4fv(%f, %f, %f, %f);\n", v[0], v[1], v[2], v[4]);

	func(v);
}

glvoid glVertex4iv( const GLint *v )
{
	init();
	static void (*func)(const GLint *) = NULL;
	if (!func)
		func = (void (*)(const GLint *)) dlsym(RTLD_NEXT, "glVertex4iv");

	printf("glVertex4iv(%d, %d, %d, %d);\n", v[0], v[1], v[2], v[4]);

	func(v);
}

glvoid glVertex4sv( const GLshort *v )
{
	init();
	static void (*func)(const GLshort *) = NULL;
	if (!func)
		func = (void (*)(const GLshort *)) dlsym(RTLD_NEXT, "glVertex4sv");

	printf("glVertex4sv(%d, %d, %d, %d);\n", v[0], v[1], v[2], v[4]);

	func(v);
}
#endif

#ifdef DO_3D_NORMAL
glvoid glNormal3b( GLbyte x, GLbyte y, GLbyte z )
{
	init();
	static void (*func)(GLbyte, GLbyte, GLbyte) = NULL;
	if (!func)
		func = (void (*)(GLbyte, GLbyte, GLbyte)) dlsym(RTLD_NEXT, "glNormal3b");

	printf("glNormal3b(%d, %d, %d);\n", x, y, z);
	new_N3(x, y, z);

	func(x, y, z);
}

glvoid glNormal3d( GLdouble x, GLdouble y, GLdouble z )
{
	init();
	static void (*func)(GLdouble, GLdouble, GLdouble) = NULL;
	if (!func)
		func = (void (*)(GLdouble, GLdouble, GLdouble)) dlsym(RTLD_NEXT, "glNormal3d");

	printf("glNormal3d(%f, %f, %f);\n", x, y, z);
	new_N3(x, y, z);

	func(x, y, z);
}

glvoid glNormal3f( GLfloat x, GLfloat y, GLfloat z )
{
	init();
	static void (*func)(GLfloat, GLfloat, GLfloat) = NULL;
	if (!func)
		func = (void (*)(GLfloat, GLfloat, GLfloat)) dlsym(RTLD_NEXT, "glNormal3f");

	printf("glNormal3f(%f, %f, %f);\n", x, y, z);
	new_N3(x, y, z);

	func(x, y, z);
}

glvoid glNormal3i( GLint x, GLint y, GLint z )
{
	init();
	static void (*func)(GLint, GLint, GLint) = NULL;
	if (!func)
		func = (void (*)(GLint, GLint, GLint)) dlsym(RTLD_NEXT, "glNormal3i");

	printf("glNormal3i(%d, %d, %d);\n", x, y, z);
	new_N3(x, y, z);

	func(x, y, z);
}

glvoid glNormal3s( GLshort x, GLshort y, GLshort z )
{
	init();
	static void (*func)(GLshort, GLshort, GLshort) = NULL;
	if (!func)
		func = (void (*)(GLshort, GLshort, GLshort)) dlsym(RTLD_NEXT, "glNormal3s");

	printf("glNormal3s(%d, %d, %d);\n", x, y, z);
	new_N3(x, y, z);

	func(x, y, z);
}

glvoid glNormal3bv( const GLbyte *v )
{
	init();
	static void (*func)(const GLbyte *) = NULL;
	if (!func)
		func = (void (*)(const GLbyte *)) dlsym(RTLD_NEXT, "glNormal3bv");

	printf("glNormal3bv(%d, %d, %d);\n", v[0], v[1], v[2]);
	new_N3(v[0], v[1], v[2]);

	func(v);
}

glvoid glNormal3dv( const GLdouble *v )
{
	init();
	static void (*func)(const GLdouble *) = NULL;
	if (!func)
		func = (void (*)(const GLdouble *)) dlsym(RTLD_NEXT, "glNormal3dv");

	printf("glNormal3dv(%f, %f, %f);\n", v[0], v[1], v[2]);
	new_N3(v[0], v[1], v[2]);

	func(v);
}

glvoid glNormal3fv( const GLfloat *v )
{
	init();
	static void (*func)(const GLfloat *) = NULL;
	if (!func)
		func = (void (*)(const GLfloat *)) dlsym(RTLD_NEXT, "glNormal3fv");

	printf("glNormal3fv(%f, %f, %f);\n", v[0], v[1], v[2]);
	new_N3(v[0], v[1], v[2]);

	func(v);
}

glvoid glNormal3iv( const GLint *v )
{
	init();
	static void (*func)(const GLint *) = NULL;
	if (!func)
		func = (void (*)(const GLint *)) dlsym(RTLD_NEXT, "glNormal3iv");

	printf("glNormal3iv(%d, %d, %d);\n", v[0], v[1], v[2]);
	new_N3(v[0], v[1], v[2]);

	func(v);
}

glvoid glNormal3sv( const GLshort *v )
{
	init();
	static void (*func)(const GLshort *) = NULL;
	if (!func)
		func = (void (*)(const GLshort *)) dlsym(RTLD_NEXT, "glNormal3sv");

	printf("glNormal3sv(%d, %d, %d);\n", v[0], v[1], v[2]);
	new_N3(v[0], v[1], v[2]);

	func(v);
}
#endif


#ifdef DO_DRAW_ELEMENTS
void glVertexPointer( GLint size, GLenum type,
		GLsizei stride, const GLvoid *ptr )
{
	init();
	static void (*func)(GLint, GLenum, GLsizei, const GLvoid *) = NULL;
	if (!func)
		func = (void (*)(GLint, GLenum, GLsizei, const GLvoid *)) dlsym(RTLD_NEXT, "glVertexPointer");

	printf("glVertexPointer(%d, %d, %d, 0x%8.8x);\n",
		size, type, stride, (unsigned int) ptr);
	new_VertexPointer( size, type, stride, ptr);

	func(size, type, stride, ptr);
}

void glDrawElements( GLenum mode, GLsizei count,
		GLenum type, const GLvoid *indices )
{
	init();
	static void (*func)(GLenum, GLsizei, GLenum, const GLvoid *) = NULL;
	if (!func)
		func = (void (*)(GLenum, GLsizei, GLenum, const GLvoid *)) dlsym(RTLD_NEXT, "glDrawElements");

	printf("glDrawElements(%s, %d, 0x%x, 0x%8.8x);\n",
		prim_type_name[mode], count, type, (unsigned int) indices);

	new_DrawElements(mode, count, type, indices);

	func(mode, count, type, indices);
}
#endif

