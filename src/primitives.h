#pragma once

extern unsigned int cube_vao, cube_vbo, cube_ibo, quad_vao, quad_vbo, quad_ibo;

void primitives_create_buffers();

// Draws a cube with the current shader program
void cube_draw();

// Draws a quad with the current shader program
void quad_draw();