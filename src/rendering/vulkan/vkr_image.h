#pragma once

#include "rendering/vulkan/vkr.h"

PIM_C_BEGIN

bool vkrImage_New(
    vkrImage* image,
    const VkImageCreateInfo* info,
    vkrMemUsage memUsage,
    const char* tag);
void vkrImage_Del(vkrImage* image);
void* vkrImage_Map(const vkrImage* image);
void vkrImage_Unmap(const vkrImage* image);
void vkrImage_Flush(const vkrImage* image);
// destroys the resource after kFramesInFlight
// if fence is provided, it is used instead
void vkrImage_Release(vkrImage* image, VkFence fence);

VkImageView vkrImageView_New(
    VkImage image,
    VkImageViewType type,
    VkFormat format,
    VkImageAspectFlags aspect,
    i32 baseMip, i32 mipCount,
    i32 baseLayer, i32 layerCount);
void vkrImageView_Del(VkImageView view);
void vkrImageView_Release(VkImageView view, VkFence fence);

VkSampler vkrSampler_New(
    VkFilter filter,
    VkSamplerMipmapMode mipMode,
    VkSamplerAddressMode addressMode,
    float aniso);
void vkrSampler_Del(VkSampler sampler);
void vkrSampler_Release(VkSampler sampler, VkFence fence);

PIM_C_END
