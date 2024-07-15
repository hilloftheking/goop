#include "editor.h"
#include "game.h"
#include "goop.h"

int main() {
  GoopEngine goop;
  goop_create(&goop);

  #ifdef GOOP_EDITOR
  editor_init(&goop);
  #else
  game_init(&goop);
  #endif

  goop_main_loop(&goop);

  goop_destroy(&goop);
  return 0;
}
