#include "io/fnd.h"

#include <io.h>

static pim_thread_local i32 ms_errno;

i32 fnd_errno(void)
{
    i32 rv = ms_errno;
    ms_errno = 0;
    return rv;
}

static i32 NotNeg(i32 x)
{
    if (x < 0)
    {
        ms_errno = 1;
    }
    return x;
}

static void* NotNull(void* x)
{
    if (!x)
    {
        ms_errno = 1;
    }
    return x;
}

static i32 IsZero(i32 x)
{
    if (x)
    {
        ms_errno = 1;
    }
    return x;
}

i32 fnd_first(fnd_t* fdr, fnd_data_t* data, const char* spec)
{
    ASSERT(fdr);
    ASSERT(data);
    ASSERT(spec);
    fdr->handle = _findfirst64(spec, (struct __finddata64_t*)data);
    return fnd_isopen(*fdr);
}

i32 fnd_next(fnd_t* fdr, fnd_data_t* data)
{
    ASSERT(fdr);
    ASSERT(data);
    if (fnd_isopen(*fdr))
    {
        if (!_findnext64(fdr->handle, (struct __finddata64_t*)data))
        {
            return 1;
        }
    }
    return 0;
}

void fnd_close(fnd_t* fdr)
{
    ASSERT(fdr);
    if (fnd_isopen(*fdr))
    {
        _findclose(fdr->handle);
        fdr->handle = -1;
    }
}

i32 fnd_iter(fnd_t* fdr, fnd_data_t* data, const char* spec)
{
    ASSERT(fdr);
    if (fnd_isopen(*fdr))
    {
        if (fnd_next(fdr, data))
        {
            return 1;
        }
        fnd_close(fdr);
        return 0;
    }
    else
    {
        return fnd_first(fdr, data, spec);
    }
}
