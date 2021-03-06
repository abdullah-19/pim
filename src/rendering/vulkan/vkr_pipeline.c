#include "rendering/vulkan/vkr_pipeline.h"
#include "rendering/vulkan/vkr_renderpass.h"
#include "allocator/allocator.h"
#include "math/types.h"
#include <string.h>

i32 vkrVertTypeSize(vkrVertType type)
{
    switch (type)
    {
    default:
        ASSERT(false);
        return 0;
    case vkrVertType_float:
        return sizeof(float);
    case vkrVertType_float2:
        return sizeof(float2);
    case vkrVertType_float3:
        return sizeof(float3);
    case vkrVertType_float4:
        return sizeof(float4);
    case vkrVertType_int:
        return sizeof(i32);
    case vkrVertType_int2:
        return sizeof(int2);
    case vkrVertType_int3:
        return sizeof(int3);
    case vkrVertType_int4:
        return sizeof(int4);
    }
}

VkFormat vkrVertTypeFormat(vkrVertType type)
{
    switch (type)
    {
    default:
        ASSERT(false);
        return 0;
    case vkrVertType_float:
        return VK_FORMAT_R32_SFLOAT;
    case vkrVertType_float2:
        return VK_FORMAT_R32G32_SFLOAT;
    case vkrVertType_float3:
        return VK_FORMAT_R32G32B32_SFLOAT;
    case vkrVertType_float4:
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    case vkrVertType_int:
        return VK_FORMAT_R32_SINT;
    case vkrVertType_int2:
        return VK_FORMAT_R32G32_SINT;
    case vkrVertType_int3:
        return VK_FORMAT_R32G32B32_SINT;
    case vkrVertType_int4:
        return VK_FORMAT_R32G32B32A32_SINT;
    }
}

void vkrPipelineLayout_New(vkrPipelineLayout* layout)
{
    ASSERT(layout);
    memset(layout, 0, sizeof(*layout));
}

void vkrPipelineLayout_Del(vkrPipelineLayout* layout)
{
    if (layout)
    {
        if (layout->handle)
        {
            vkDestroyPipelineLayout(g_vkr.dev, layout->handle, NULL);
        }
        const i32 setCount = layout->setCount;
        VkDescriptorSetLayout* sets = layout->sets;
        for (i32 i = 0; i < setCount; ++i)
        {
            VkDescriptorSetLayout set = sets[i];
            if (set)
            {
                vkDestroyDescriptorSetLayout(g_vkr.dev, set, NULL);
            }
        }
        pim_free(layout->sets);
        pim_free(layout->ranges);
        memset(layout, 0, sizeof(*layout));
    }
}

bool vkrPipelineLayout_AddSet(
    vkrPipelineLayout* layout,
    i32 bindingCount,
    const VkDescriptorSetLayoutBinding* pBindings,
    VkDescriptorSetLayoutCreateFlags flags)
{
    ASSERT(layout);
    ASSERT(!layout->handle);
    ASSERT(pBindings || !bindingCount);
    ASSERT(bindingCount >= 0);
    VkDescriptorSetLayout set = NULL;
    const VkDescriptorSetLayoutCreateInfo createInfo =
    {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .flags = flags,
        .bindingCount = bindingCount,
        .pBindings = pBindings,
    };
    VkCheck(vkCreateDescriptorSetLayout(g_vkr.dev, &createInfo, NULL, &set));
    ASSERT(set);
    if (set)
    {
        layout->setCount += 1;
        PermReserve(layout->sets, layout->setCount);
        layout->sets[layout->setCount - 1] = set;
    }
    return set != NULL;
}

void vkrPipelineLayout_AddRange(
    vkrPipelineLayout* layout,
    VkPushConstantRange range)
{
    ASSERT(layout);
    ASSERT(!layout->handle);
    i32 back = layout->rangeCount++;
    PermReserve(layout->ranges, back+1);
    layout->ranges[back] = range;
}

bool vkrPipelineLayout_Compile(vkrPipelineLayout* desc)
{
    ASSERT(desc);
    if (desc->handle)
    {
        return true;
    }
    const VkPipelineLayoutCreateInfo createInfo =
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = desc->setCount,
        .pSetLayouts = desc->sets,
        .pushConstantRangeCount = desc->rangeCount,
        .pPushConstantRanges = desc->ranges,
    };
    VkPipelineLayout handle = NULL;
    VkCheck(vkCreatePipelineLayout(g_vkr.dev, &createInfo, NULL, &handle));
    ASSERT(handle);
    desc->handle = handle;
    return handle != NULL;
}

bool vkrPipeline_NewGfx(
    vkrPipeline* pipeline,
    const vkrFixedFuncs* fixedfuncs,
    const vkrVertexLayout* vertLayout,
    vkrPipelineLayout* layout,
    VkRenderPass renderPass,
    i32 subpass,
    i32 shaderCount,
    const VkPipelineShaderStageCreateInfo* shaders)
{
    ASSERT(pipeline);
    ASSERT(fixedfuncs);
    ASSERT(vertLayout);
    ASSERT(layout);
    ASSERT(renderPass);

    memset(pipeline, 0, sizeof(*pipeline));

    if (!vkrPipelineLayout_Compile(layout))
    {
        return false;
    }

    const i32 vertStreamCount = vertLayout->streamCount;
    VkVertexInputBindingDescription* vertBindings = tmp_calloc(sizeof(vertBindings[0]) * vertStreamCount);
    VkVertexInputAttributeDescription* vertAttribs = tmp_calloc(sizeof(vertAttribs[0]) * vertStreamCount);
    for (i32 i = 0; i < vertStreamCount; ++i)
    {
        vkrVertType vtype = vertLayout->types[i];
        i32 size = vkrVertTypeSize(vtype);
        VkFormat format = vkrVertTypeFormat(vtype);
        vertBindings[i].binding = i;
        vertBindings[i].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        vertBindings[i].stride = size;
        vertAttribs[i].binding = i;
        vertAttribs[i].format = format;
        vertAttribs[i].location = i;
        vertAttribs[i].offset = 0;
    }
    const VkPipelineVertexInputStateCreateInfo vertexInputInfo =
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = vertLayout->streamCount,
        .pVertexBindingDescriptions = vertBindings,
        .vertexAttributeDescriptionCount = vertLayout->streamCount,
        .pVertexAttributeDescriptions = vertAttribs,
    };
    const VkPipelineInputAssemblyStateCreateInfo inputAssembly =
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = fixedfuncs->topology,
    };
    const VkPipelineViewportStateCreateInfo viewportState =
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &fixedfuncs->viewport,
        .scissorCount = 1,
        .pScissors = &fixedfuncs->scissor,
    };
    const VkPipelineRasterizationStateCreateInfo rasterizer =
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = fixedfuncs->depthClamp,
        .polygonMode = fixedfuncs->polygonMode,
        .cullMode = fixedfuncs->cullMode,
        .frontFace = fixedfuncs->frontFace,
        .lineWidth = 1.0f,
    };
    const VkPipelineMultisampleStateCreateInfo multisampling =
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };
    const VkPipelineDepthStencilStateCreateInfo depthStencil =
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = fixedfuncs->depthTestEnable,
        .depthWriteEnable = fixedfuncs->depthWriteEnable,
        .depthCompareOp = fixedfuncs->depthCompareOp,
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 1.0f,
    };
    const i32 attachCount = fixedfuncs->attachmentCount;
    VkPipelineColorBlendAttachmentState* colorBlendAttachments = tmp_calloc(sizeof(colorBlendAttachments[0]) * attachCount);
    for (i32 i = 0; i < attachCount; ++i)
    {
        // visual studio formatter fails at C syntax here :(
        colorBlendAttachments[i] = (VkPipelineColorBlendAttachmentState)
        {
            .blendEnable = fixedfuncs->attachments[i].blendEnable,
                .srcColorBlendFactor = fixedfuncs->attachments[i].srcColorBlendFactor,
                .dstColorBlendFactor = fixedfuncs->attachments[i].dstColorBlendFactor,
                .colorBlendOp = fixedfuncs->attachments[i].colorBlendOp,
                .srcAlphaBlendFactor = fixedfuncs->attachments[i].srcAlphaBlendFactor,
                .dstAlphaBlendFactor = fixedfuncs->attachments[i].dstAlphaBlendFactor,
                .alphaBlendOp = fixedfuncs->attachments[i].alphaBlendOp,
                .colorWriteMask = fixedfuncs->attachments[i].colorWriteMask,
        };
    }
    const VkPipelineColorBlendStateCreateInfo colorBlending =
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = fixedfuncs->attachmentCount,
        .pAttachments = colorBlendAttachments,
    };
    const VkDynamicState dynamicStates[] =
    {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };
    const VkPipelineDynamicStateCreateInfo dynamicStateInfo =
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = NELEM(dynamicStates),
        .pDynamicStates = dynamicStates,
    };

    const VkGraphicsPipelineCreateInfo pipelineInfo =
    {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = shaderCount,
        .pStages = shaders,
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pTessellationState = NULL,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = &depthStencil,
        .pColorBlendState = &colorBlending,
        .pDynamicState = &dynamicStateInfo,
        .layout = layout->handle,
        .renderPass = renderPass,
        .subpass = subpass,
    };

    VkPipeline handle = NULL;
    VkCheck(vkCreateGraphicsPipelines(g_vkr.dev, NULL, 1, &pipelineInfo, NULL, &handle));
    ASSERT(handle);
    if (handle)
    {
        pipeline->handle = handle;
        pipeline->bindpoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        pipeline->layout = *layout;
        memset(layout, 0, sizeof(*layout));
        pipeline->subpass = subpass;
    }

    return handle != NULL;
}

void vkrPipeline_Del(vkrPipeline* pipeline)
{
    if (pipeline)
    {
        if (pipeline->handle)
        {
            vkDestroyPipeline(g_vkr.dev, pipeline->handle, NULL);
        }
        vkrPipelineLayout_Del(&pipeline->layout);
        memset(pipeline, 0, sizeof(*pipeline));
    }
}
