#pragma once

#include "common/macro.h"
#include "common/int_types.h"
#include "containers/array.h"

namespace Quake
{
    // dpackheader_t
    // common.c 1231
    // must match exactly for binary compatibility
    struct DPackHeader
    {
        char id[4]; // "PACK"
        i32 offset;
        i32 length;
    };

    // dpackfile_t
    // common.c 1225
    // must match exactly for binary compatibility
    struct DPackFile
    {
        char name[56];
        i32 offset;
        i32 length;
    };

    struct Pack
    {
        char path[PIM_PATH];
        const u8* ptr;
        i32 bytes;
        const DPackHeader* header;
        const DPackFile* files;
        i32 count;
    };

    struct Folder
    {
        char path[PIM_PATH];
        Array<Pack> packs;
    };

    Pack LoadPack(cstrc dir, AllocType allocator);
    void FreePack(Pack& pack);

    Folder LoadFolder(cstrc dir, AllocType allocator);
    void FreeFolder(Folder& folder);
};
