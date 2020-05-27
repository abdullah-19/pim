#include "math/ambcube.h"
#include "rendering/path_tracer.h"
#include "math/float3_funcs.h"

i32 AmbCube_Bake(
    const struct pt_scene_s* scene,
    AmbCube_t* pCube,
    float4 origin,
    i32 samples,
    i32 prevSampleCount,
    i32 bounces)
{
    ASSERT(scene);
    ASSERT(pCube);
    ASSERT(samples >= 0);
    ASSERT(prevSampleCount >= 0);
    ASSERT(bounces >= 0);

    ray_t ray = { .ro = origin };

    pt_raygen_t* task = pt_raygen(scene, ray, ptdist_sphere, samples, bounces);
    task_sys_schedule();
    task_await((task_t*)task);

    const float3* pim_noalias colors = task->colors;
    const float4* pim_noalias directions = task->directions;

    AmbCube_t cube = *pCube;
    i32 s = prevSampleCount;
    float w = 6.0f / (1.0f + samples);
    w = w / (1.0f + s);
    for (i32 i = 0; i < samples; ++i)
    {
        cube = AmbCube_Fit(cube, w, directions[i], f3_f4(colors[i], 1.0f));
    }
    *pCube = cube;

    return prevSampleCount + 1;
}
