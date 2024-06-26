#pragma once

typedef struct HRSRC__ *HRSRC;

typedef struct Resource {
  HRSRC hrsrc;
  void *hdata;
  unsigned long data_size;
  void *data;
} Resource;

void resource_load(Resource *r, int rid, const char *type);
void resource_destroy(Resource *r);