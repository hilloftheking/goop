#include <stdio.h>
#include <stdlib.h>

#include <GLFW/glfw3.h>

#include "core.h"

Global global;

void exit_fatal_error() {
  fprintf(stderr, "Exiting due to fatal error\n");
  __debugbreak();

  // If GLFW isn't terminated, the window will be frozen
  glfwTerminate();

  getchar();
  exit(-1);
}