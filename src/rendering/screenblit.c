#include "screenblit.h"
#include "rendering/vulkan/vkr.h"
#include "rendering/vulkan/vkr_texture.h"
#include "rendering/vulkan/vkr_buffer.h"
#include "rendering/vulkan/vkr_mem.h"
#include "rendering/vulkan/vkr_image.h"
#include "rendering/vulkan/vkr_pipeline.h"
#include "rendering/vulkan/vkr_renderpass.h"
#include "rendering/vulkan/vkr_compile.h"
#include "rendering/vulkan/vkr_shader.h"
#include "rendering/vulkan/vkr_context.h"
#include "rendering/vulkan/vkr_cmd.h"
#include "rendering/vulkan/vkr_swapchain.h"
#include "rendering/vulkan/vkr_sync.h"
#include "rendering/constants.h"
#include "common/console.h"
#include "math/types.h"
#include "allocator/allocator.h"
#include "common/profiler.h"
#include <string.h>

static const float2 kScreenMesh[] =
{
    { -1.0f, -1.0f },
    { 1.0f, -1.0f },
    { 1.0f,  1.0f },
    { 1.0f,  1.0f },
    { -1.0f,  1.0f },
    { -1.0f, -1.0f },
};

static vkrBuffer ms_blitMesh;
static vkrBuffer ms_stageBuf;
static vkrPipeline ms_blitProgram;
static vkrImage ms_image;

// ----------------------------------------------------------------------------

bool screenblit_init(void)
{
    bool success = true;

    const u32 queueFamilies[] =
    {
        g_vkr.queues[vkrQueueId_Gfx].family,
    };
    const VkImageCreateInfo imageInfo =
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags = 0x0,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .extent.width = kDrawWidth,
        .extent.height = kDrawHeight,
        .extent.depth = 1,
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage =
            VK_IMAGE_USAGE_TRANSFER_DST_BIT |
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = NELEM(queueFamilies),
        .pQueueFamilyIndices = queueFamilies,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    if (!vkrImage_New(&ms_image, &imageInfo, vkrMemUsage_GpuOnly, PIM_FILELINE))
    {
        ASSERT(false);
        success = false;
        goto cleanup;
    }
    if (!vkrBuffer_New(
        &ms_blitMesh,
        sizeof(kScreenMesh),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        vkrMemUsage_CpuToGpu,
        PIM_FILELINE))
    {
        ASSERT(false);
        success = false;
        goto cleanup;
    }
    if (!vkrBuffer_New(
        &ms_stageBuf,
        kDrawWidth * kDrawHeight * 4,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        vkrMemUsage_CpuOnly,
        PIM_FILELINE))
    {
        ASSERT(false);
        success = false;
        goto cleanup;
    }

cleanup:
    if (!success)
    {
        screenblit_shutdown();
    }
    return success;
}

void screenblit_shutdown(void)
{
    vkrImage_Release(&ms_image, NULL);
    vkrBuffer_Release(&ms_blitMesh, NULL);
    vkrBuffer_Release(&ms_stageBuf, NULL);
    vkrPipeline_Del(&ms_blitProgram);
}

ProfileMark(pm_blit, screenblit_blit)
void screenblit_blit(
    VkCommandBuffer cmd,
    const u32* texels,
    i32 width,
    i32 height)
{
    ProfileBegin(pm_blit);

    const vkrSwapchain* swapchain = &g_vkr.chain;
    VkImage dstImage = swapchain->images[swapchain->imageIndex];
    VkImage srcImage = ms_image.handle;
    VkBuffer stageBuf = ms_stageBuf.handle;

    // copy input data to stage buffer
    {
        const i32 bytes = width * height * sizeof(texels[0]);
        ASSERT(bytes == ms_stageBuf.size);
        void* dst = vkrBuffer_Map(&ms_stageBuf);
        ASSERT(dst);
        if (dst)
        {
            memcpy(dst, texels, bytes);
        }
        vkrBuffer_Unmap(&ms_stageBuf);
        vkrBuffer_Flush(&ms_stageBuf);
    }

    // transition buffer to xfer src
    {
        const VkBufferMemoryBarrier barrier = 
        {
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .srcAccessMask = 0x0,
            .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer = stageBuf,
            .size = VK_WHOLE_SIZE,
        };
        vkrCmdBufferBarrier(
            cmd,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            &barrier);
    }
    // transition image to xfer dst
    {
        const VkImageMemoryBarrier imgBarrier =
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = 0x0,
            .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = srcImage,
            .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
        };
        vkrCmdImageBarrier(cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            &imgBarrier);
    }
    // copy buffer to image
    {
        const VkBufferImageCopy bufferRegion =
        {
            .imageSubresource =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .layerCount = 1,
            },
            .imageExtent.width = width,
            .imageExtent.height = height,
            .imageExtent.depth = 1,
        };
        vkCmdCopyBufferToImage(
            cmd,
            stageBuf,
            srcImage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &bufferRegion);
    }
    // transition image to xfer src
    {
        const VkImageMemoryBarrier imgBarrier =
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = 0x0,
            .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = srcImage,
            .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
        };
        vkrCmdImageBarrier(cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            &imgBarrier);
    }
    // transition dst image to xfer dst
    {
        const VkImageMemoryBarrier imgBarrier =
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = 0x0,
            .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = dstImage,
            .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
        };
        vkrCmdImageBarrier(cmd,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            &imgBarrier);
    }
    // blit image to target
    {
        const VkImageBlit region =
        {
            .srcOffsets[0] = { 0, height, 0 },
            .srcOffsets[1] = { width, 0, 1 },
            .dstOffsets[1] = { swapchain->width, swapchain->height, 1 },
            .srcSubresource =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .layerCount = 1,
            },
            .dstSubresource =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .layerCount = 1,
            },
        };
        vkCmdBlitImage(
            cmd,
            srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &region,
            VK_FILTER_LINEAR);
    }
    // transition dst image to present src
    {
        const VkImageMemoryBarrier imgBarrier =
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = 0x0,
            .dstAccessMask = 0x0,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = dstImage,
            .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
        };
        vkrCmdImageBarrier(cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            &imgBarrier);
    }

    ProfileEnd(pm_blit);
}
