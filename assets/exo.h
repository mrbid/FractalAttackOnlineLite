#include <stddef.h>

#include "../inc/gl.h"

#ifndef exo_H
#define exo_H

extern GLfloat exo_vertices[];
extern GLfloat exo_colors[];
extern const GLuint exo_indices[];
extern const GLsizeiptr exo_numind;
extern const GLsizeiptr exo_numvert;
extern const size_t exo_vertices_size;
extern const size_t exo_colors_size;
extern const size_t exo_indices_size;

extern GLfloat ncube_vertices[];
extern const GLuint ncube_indices[];
extern const GLsizeiptr ncube_numind;
extern const GLsizeiptr ncube_numvert;
extern const size_t ncube_vertices_size;
extern const size_t ncube_indices_size;

extern GLfloat inner_colors[];
extern const size_t inner_colors_size;

#endif
