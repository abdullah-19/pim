#pragma once

#include "common/macro.h"
#include "math/types.h"

PIM_C_BEGIN

typedef struct texture_s
{
    u64 version;
    int2 size;
    u32* pim_noalias texels;
} texture_t;
static const u32 texture_t_hash = 2393313439u;

typedef struct textureid_s
{
    u64 version;
    void* handle;
} textureid_t;
static const u32 textureid_t_hash = 3843543442u;

textureid_t texture_load(const char* path);
textureid_t texture_create(texture_t* src);
bool texture_destroy(textureid_t id);
bool texture_current(textureid_t id);
bool texture_get(textureid_t id, texture_t* dst);

bool texture_register(const char* name, textureid_t id);
textureid_t texture_lookup(const char* name);

textureid_t texture_unpalette(const u8* bytes, int2 size);

textureid_t texture_lumtonormal(textureid_t src, float scale);

PIM_C_END
