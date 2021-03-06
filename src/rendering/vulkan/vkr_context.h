#pragma once

#include "rendering/vulkan/vkr.h"

PIM_C_BEGIN

vkrFrameContext* vkrContext_Get(void);

bool vkrContext_New(vkrContext* ctx);
void vkrContext_Del(vkrContext* ctx);

bool vkrThreadContext_New(vkrThreadContext* ctx);
void vkrThreadContext_Del(vkrThreadContext* ctx);

bool vkrFrameContext_New(vkrFrameContext* ctx);
void vkrFrameContext_Del(vkrFrameContext* ctx);

void vkrContext_GetCmd(
    vkrFrameContext* ctx,
    vkrQueueId id,
    VkCommandBuffer* cmdOut,
    VkFence* fenceOut,
    VkQueue* queueOut);
void vkrContext_GetSecCmd(
    vkrFrameContext* ctx,
    vkrQueueId id,
    VkCommandBuffer* cmdOut,
    VkFence* fenceOut,
    VkQueue* queueOut);

PIM_C_END
