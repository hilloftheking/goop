// An inside out cube

#pragma once

extern unsigned int cube_vao, cube_vbo, cube_ibo;

void cube_create_buffers();

// Draws the cube with the current shader program
void cube_draw();