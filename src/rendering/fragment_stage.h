#pragma once

#include "common/macro.h"
#include "math/types.h"

PIM_C_BEGIN

struct task_s* Drawables_Fragment(struct tables_s* tables, struct framebuf_s* frontBuf, const struct framebuf_s* backBuf);

AmbCube_t VEC_CALL AmbCube_Get(void);
void VEC_CALL AmbCube_Set(AmbCube_t cube);

PIM_C_END
