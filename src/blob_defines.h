// This file can be used by shaders and C sources

#ifndef BLOB_DEFINES_H
#define BLOB_DEFINES_H

#define BLOB_MAX_RADIUS 1.0f

#define BLOB_SMOOTH 0.6f
#define BLOB_CHAR_SMOOTH 0.1f

#define BLOB_MAT_COUNT 5
#define BLOB_RADIUS_MULT 10000.0f

#define BLOB_COL_LIQUID 0.41f, 0.04f, 0.06f
#define BLOB_COL_SOLID 0.8f, 0.8f, 0.7f

#define BLOB_SDF_RES 128
#define BLOB_SDF_LOCAL_GROUPS 4
#define BLOB_SDF_MAX_DIST 2.0f
#define BLOB_CHAR_SDF_MAX_DIST 2.0f
#define BLOB_SDF_MIN_DIST -0.25f

#define BLOB_SDF_SIZE 16.0f, 16.0f, 16.0f
#define BLOB_SDF_START -8.0f, 0.0f, -8.0f

#endif