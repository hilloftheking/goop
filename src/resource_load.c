#include <stdio.h>
#include <stdlib.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "core.h"
#include "resource_load.h"

static void exit_resource_fail(int rid, const char *type) {
  printf("Failed to load %s resource %d\n", type, rid);
  exit_fatal_error();
}

void resource_load(Resource* r, int rid, const char *type) {
  r->hrsrc = FindResourceA(NULL, MAKEINTRESOURCEA(rid), type);
  if (!r->hrsrc) {
    exit_resource_fail(rid, type);
  }

  r->data_size = SizeofResource(NULL, r->hrsrc);
  r->hdata = LoadResource(NULL, r->hrsrc);
  if (!r->hdata) {
    exit_resource_fail(rid, type);
  }

  r->data = LockResource(r->hdata);
}

void resource_destroy(Resource *r) { FreeResource(r->hdata); }