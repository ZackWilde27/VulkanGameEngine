-- This is where the GPU gets told what to do each frame, the idea is that I can add post process passes, and any kind of pre-pass to the rendering code without recompiling anything
-- The only problem is that this lua script is over 200 lines of gobbledygook, and that's after I've tried to make it as simple as possible, that's Vulkan for you

-- Available render targets (and their formats) from main rendering:
-- mainRenderTarget_C (RenderFmt) - Colour buffer
-- mainRenderTarget_D (DepthStencilFmt) - Depth buffer
-- mainRenderTarget_N (NormalFmt) - Normal buffer
-- mainRenderTarget_P (PositionFmt) - Position buffer
-- mainRenderTarget_G (GIFmt) - Baked GI buffer

lightResultFmt = VK_FORMAT_R8G8B8A8_UNORM
AOFmt = VK_FORMAT_R8_UNORM

-- Draws objects from the camera's perspective
local RPT_DEFAULT = 0
-- Draws a rectangle covering the screen
local RPT_POST = 1
-- Blasts one image to another image
local RPT_BLIT = 2
-- A special pass that takes the position render target from the base pass and does all the lighting calculations for each light
local RPT_SHADOW = 3

-- MSAA samples
SampleCount = VK_SAMPLE_COUNT_1_BIT

local ResolutionScale = 1.0

local RenderWidth = SwapChainWidth * ResolutionScale
local RenderHeight = SwapChainHeight * ResolutionScale

local AOWidth = SwapChainWidth / 2
local AOHeight = SwapChainHeight / 2

local IncludeDepthPrepass = true
-- If SSR is disabled the water shader will need to be changed to include the reflections
local EnableSSR = true
-- If SSAO is enabled, the post/post-fog shader will need to be updated to factor in the AO
local EnableSSAO = false

local SSRDownscale = 2


function CreateImage2D(format, width, height, mipLevels, sampleCount, imageTiling, usage, aspectFlags, magFilter, minFilter, addressMode)
    -- GPU is the VkPhysicalDevice
    return CreateImage(VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D, format, width, height, mipLevels, 1, sampleCount, imageTiling, usage, aspectFlags, magFilter, minFilter, addressMode)
end

function BasicDescription(format, sampleCount, toLayout)
    -- { fromLayout, toLayout, format, loadOp, storeOp }
    return { VK_IMAGE_LAYOUT_UNDEFINED, toLayout or VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, format, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, sampleCount }
end

function LoadDescription(format, sampleCount, fromLayout, toLayout)
    return { fromLayout, toLayout or VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, format, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE, sampleCount }
end

function CreateRenderTarget(format, width, height, sampleCount, flags)
    return CreateImage2D(format, width, height, 1, sampleCount, VK_IMAGE_TILING_OPTIMAL, flags or (VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT), VK_IMAGE_ASPECT_COLOR_BIT, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
end

function CreateDepthTarget(width, height, sampleCount, flags)
    return CreateImage2D(DepthFmt, width, height, 1, sampleCount, VK_IMAGE_TILING_OPTIMAL, flags or (VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT), VK_IMAGE_ASPECT_DEPTH_BIT, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
end

function CreateDepthStencilTarget(width, height, sampleCount, usage)
    return CreateImage2D(DepthStencilFmt, width, height, 1, sampleCount, VK_IMAGE_TILING_OPTIMAL, usage or (VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT), VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
end

function CreateStencilTarget(width, height, sampleCount, usage)
    return CreateImage2D(VK_FORMAT_S8_UINT, width, height, 1, sampleCount, VK_IMAGE_TILING_OPTIMAL, usage or VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_STENCIL_BIT, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
end

function CreateBlitImage(format, width, height, usage)
    return CreateImage2D(format, width, height, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, usage or (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT), VK_IMAGE_ASPECT_COLOR_BIT, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
end


-- The 2 post render targets, in order to do multiple post passes you gotta swap back and forth while referencing the previous one
postRenderTarget1 = CreateRenderTarget(PresentFmt, SwapChainWidth, SwapChainHeight, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
postRenderTarget2 = CreateRenderTarget(PresentFmt, SwapChainWidth, SwapChainHeight, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
-- If the render resolution is different from the swap chain resolution, there needs to be a depth buffer for post process passes
if ResolutionScale ~= 1.0 then
    postDepthTarget = CreateDepthStencilTarget(SwapChainWidth, SwapChainHeight, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
else
    postDepthTarget = mainRenderTarget_D
end

-- SSR Render Targets, since it happens at a lower resolution to make it as cheap as possible
-- SSR is not compatible with MSAA, since it needs the depth buffer, and rendering SSR with MSAA would make it even more expensive than it already is for no reason
local SSRRenderTarget = CreateRenderTarget(RenderFmt, RenderWidth / SSRDownscale, RenderHeight / SSRDownscale, VK_SAMPLE_COUNT_1_BIT)
local SSRStencilTarget = CreateDepthStencilTarget(RenderWidth / SSRDownscale, RenderHeight / SSRDownscale, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
local SSRPosTarget = CreateRenderTarget(PositionFmt, (RenderWidth / SSRDownscale) / 8, (RenderWidth / SSRDownscale) / 8, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)

function Append(table, item)
    table[#table + 1] = item
end

autoExposureBlitTargets = {}
local w = SwapChainWidth / 2
local h = SwapChainHeight / 2
while w > 1.0 and h > 1.0 do
    Append(autoExposureBlitTargets, CreateBlitImage(PresentFmt, w, h))
    w = w / 2
    h = h / 2
end

Append(autoExposureBlitTargets, CreateBlitImage(PresentFmt, 1, 1, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT))
defaultwhite_rgh = LoadImage("textures/default_white_rgh.png")

OneTimeBlit(defaultwhite_rgh, autoExposureBlitTargets[#autoExposureBlitTargets], VK_FILTER_LINEAR, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)

-- Bloom not only needs a render target to store just the bright pixels in, it also needs render targets for the downscaling/upscaling
bloomRenderTarget1 = CreateImage2D(PresentFmt, SwapChainWidth, SwapChainHeight, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
downScaleRenderTarget1 = CreateBlitImage(PresentFmt, SwapChainWidth / 2, SwapChainHeight / 2)
downScaleRenderTarget2 = CreateBlitImage(PresentFmt, SwapChainWidth / 4, SwapChainHeight / 4)
downScaleRenderTarget3 = CreateBlitImage(PresentFmt, SwapChainWidth / 8, SwapChainHeight / 8, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
downScaleRenderTarget4 = CreateBlitImage(PresentFmt, SwapChainWidth / 16, SwapChainHeight / 16)
downScaleRenderTarget5 = CreateBlitImage(PresentFmt, SwapChainWidth / 32, SwapChainHeight / 32)
downScaleRenderTarget6 = CreateBlitImage(PresentFmt, SwapChainWidth / 64, SwapChainHeight / 64, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
bloomBlurRenderTarget1 = CreateRenderTarget(PresentFmt, SwapChainWidth / 64, SwapChainHeight / 64, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
bloomBlurRenderTarget2 = CreateRenderTarget(PresentFmt, SwapChainWidth / 64, SwapChainHeight / 64, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)

AORenderTarget = CreateRenderTarget(AOFmt, AOWidth, AOHeight, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
AOBlurTarget1 = CreateRenderTarget(AOFmt, AOWidth, AOHeight, VK_SAMPLE_COUNT_1_BIT)
AOBlurTarget2 = CreateRenderTarget(AOFmt, AOWidth, AOHeight, VK_SAMPLE_COUNT_1_BIT)--, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
AODownscaleTarget1 = CreateBlitImage(AOFmt, AOWidth / 2, AOHeight / 2, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
AODownscaleTarget2 = CreateBlitImage(AOFmt, AOWidth / 4, AOHeight / 4, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)

-- This render target stores the results of the lighting pass, to be combined with the main image
lightResultTarget = CreateRenderTarget(lightResultFmt, SwapChainWidth, SwapChainHeight, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)



midFrame1Depends = {
    { VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_DEPENDENCY_BY_REGION_BIT }
}

-- CreateRenderPass(samples, descriptions, colRefs, depthRef, dependencies)
postRenderPass = CreateRenderPass(VK_SAMPLE_COUNT_1_BIT, { BasicDescription(PresentFmt, VK_SAMPLE_COUNT_1_BIT) }, { { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL } }, nil, nil, nil, midFrame1Depends)
postRenderPass_Depth = CreateRenderPass(VK_SAMPLE_COUNT_1_BIT, { BasicDescription(PresentFmt, VK_SAMPLE_COUNT_1_BIT), LoadDescription(DepthStencilFmt, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) }, { { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL } }, { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL }, nil, nil, midFrame1Depends)


postDepends = {
    { VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_DEPENDENCY_BY_REGION_BIT }
}

SSRCombineDescriptions = {
    BasicDescription(PresentFmt, VK_SAMPLE_COUNT_1_BIT),
    LoadDescription(DepthStencilFmt, SampleCount, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
}

postFramebuffer1 = CreateFrameBuffer({ postRenderTarget1 }, SwapChainWidth, SwapChainHeight, postRenderPass)
postFramebufferDepth1 = CreateFrameBuffer({ postRenderTarget1, postDepthTarget }, SwapChainWidth, SwapChainHeight, postRenderPass_Depth)

postFramebuffer2 = CreateFrameBuffer({ postRenderTarget2 }, SwapChainWidth, SwapChainHeight, postRenderPass)
postFramebufferDepth2 = CreateFrameBuffer({ postRenderTarget2, postDepthTarget }, SwapChainWidth, SwapChainHeight, postRenderPass_Depth)

-- CreateRenderPass(samples, descriptions, colRefs, depthRef, resolveRefs, nil, dependencies)
SSRCombineRenderPass = CreateRenderPass(VK_SAMPLE_COUNT_1_BIT, SSRCombineDescriptions, { { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL } }, { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL }, nil, nil, postDepends)
SSRCombineFramebuffer = CreateFrameBuffer({ postRenderTarget1, mainRenderTarget_D }, SwapChainWidth, SwapChainHeight, SSRCombineRenderPass)

--depthPrepassRenderPass = CreateRenderPass(SampleCount, { BasicDescription(DepthStencilFmt, SampleCount, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) }, {}, { 0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL }, nil, nil, mainDepends)
--depthPrepassFramebuffer1 = CreateFrameBuffer({ mainRenderTarget_D }, RenderWidth, RenderHeight, depthPrepassRenderPass)


SSRDescriptions = {
    BasicDescription(RenderFmt, SampleCount),
    LoadDescription(DepthStencilFmt, SampleCount, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
}


SSRTargets = {
    SSRRenderTarget,
    SSRStencilTarget
}


SSRRenderPass = CreateRenderPass(SampleCount,  SSRDescriptions, { { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL } }, { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL }, nil, nil, midFrame1Depends)
SSRFramebuffer = CreateFrameBuffer(SSRTargets, RenderWidth / SSRDownscale, RenderHeight / SSRDownscale, SSRRenderPass)

lastRenderPassDescription = {
    { VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, SwapChainFmt, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_SAMPLE_COUNT_1_BIT }
}
lastRenderPass = CreateRenderPass(VK_SAMPLE_COUNT_1_BIT, lastRenderPassDescription, { { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL } }, nil, nil, nil, postDepends)


bloomBlurXDescription = {
    BasicDescription(PresentFmt, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
}
bloomBlurXRenderPass = CreateRenderPass(VK_SAMPLE_COUNT_1_BIT, bloomBlurXDescription, { { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL } }, nil, nil, nil, postDepends)

bloomBlurYDescription = {
    BasicDescription(PresentFmt, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
}
bloomBlurYRenderPass = CreateRenderPass(VK_SAMPLE_COUNT_1_BIT, bloomBlurYDescription, { { 0, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL } }, nil, nil, nil, postDepends)

bloomBlurFramebuffer1 = CreateFrameBuffer({ bloomBlurRenderTarget1 }, SwapChainWidth / 64, SwapChainHeight / 64, bloomBlurXRenderPass)
bloomBlurFramebuffer2 = CreateFrameBuffer({ bloomBlurRenderTarget2 }, SwapChainWidth / 64, SwapChainHeight / 64, bloomBlurYRenderPass)

bloomFramebuffer1    = CreateFrameBuffer({ bloomRenderTarget1 }, SwapChainWidth, SwapChainHeight, postRenderPass)

lightResultRenderPass = CreateRenderPass(VK_SAMPLE_COUNT_1_BIT, { BasicDescription(lightResultFmt, VK_SAMPLE_COUNT_1_BIT) }, { { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL } }, nil, nil, nil, midFrame1Depends)
spotShadowDescriptions = {
    { VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, lightResultFmt, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE, VK_SAMPLE_COUNT_1_BIT }
}
spotShadowPassRenderPass = CreateRenderPass(VK_SAMPLE_COUNT_1_BIT, spotShadowDescriptions, { { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL } }, nil, nil, nil, midFrame1Depends)


lightResultFramebuffer = CreateFrameBuffer({ lightResultTarget }, SwapChainWidth, SwapChainHeight, lightResultRenderPass)

AORenderPass = CreateRenderPass(VK_SAMPLE_COUNT_1_BIT, { BasicDescription(AOFmt, VK_SAMPLE_COUNT_1_BIT) }, { { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL } }, nil, nil, nil, postDepends)
AOFramebuffer = CreateFrameBuffer({ AORenderTarget }, AOWidth, AOHeight, AORenderPass)
AOBlurFramebuffer1 = CreateFrameBuffer({ AOBlurTarget1 }, AOWidth, AOHeight, AORenderPass)
AOBlurFramebuffer2 = CreateFrameBuffer({ AOBlurTarget2 }, AOWidth, AOHeight, AORenderPass)

function ImageDescriptor(target, layout)
    return { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, target, layout or VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }
end

function PostDescriptors(colourTarget)
    return { ImageDescriptor(colourTarget) }
end

if EnableSSAO then
    postDescriptorSets_Start = {
        ImageDescriptor(mainRenderTarget_C),
        ImageDescriptor(lightResultTarget),
        ImageDescriptor(AOBlurTarget2),
        ImageDescriptor(mainRenderTarget_G)
        --ImageDescriptor(RTTexture, VK_IMAGE_LAYOUT_GENERAL)
    }
else
    postDescriptorSets_Start = {
        ImageDescriptor(mainRenderTarget_C),
        ImageDescriptor(lightResultTarget),
        ImageDescriptor(mainRenderTarget_G)
        --ImageDescriptor(RTTexture, VK_IMAGE_LAYOUT_GENERAL)
    }
end

postDescriptorSets_SSR = {
    ImageDescriptor(postRenderTarget2),
    ImageDescriptor(mainRenderTarget_N),
    ImageDescriptor(mainRenderTarget_P),
    ImageDescriptor(SSRPosTarget),
    ImageDescriptor(Cubemap)
}

postDescriptorSets_SSRCombine = {
    ImageDescriptor(postRenderTarget2),
    ImageDescriptor(SSRRenderTarget),
    ImageDescriptor(lightResultTarget)
}

postDescriptorSets_Mid1 = PostDescriptors(postRenderTarget1)
postDescriptorSets_Mid2 = PostDescriptors(postRenderTarget2)

bloomGatherDescriptorSets = {
    ImageDescriptor(postRenderTarget1)
}

bloomCombineDescriptors = {
    ImageDescriptor(downScaleRenderTarget3),
    ImageDescriptor(postRenderTarget1)
}

shadowPassDescriptors = {
    ImageDescriptor(mainRenderTarget_P),
    ImageDescriptor(mainRenderTarget_N)
}

postDescriptorSets_Fog = {
    ImageDescriptor(postRenderTarget1),
    ImageDescriptor(mainRenderTarget_P)
}

bloomBlurXDescriptorSets = {
    ImageDescriptor(downScaleRenderTarget6)
}

bloomBlurYDescriptorSets = {
    ImageDescriptor(bloomBlurRenderTarget1)
}

AODescriptorSets = {
    ImageDescriptor(mainRenderTarget_N),
    ImageDescriptor(mainRenderTarget_D)
}

AOBlurDescriptorSets_Start = {
    ImageDescriptor(AORenderTarget),
    ImageDescriptor(mainRenderTarget_D)
}

AOBlurDescriptorSets1 = {
    ImageDescriptor(AOBlurTarget1),
    ImageDescriptor(mainRenderTarget_D)
}

AOBlurDescriptorSets2 = {
    ImageDescriptor(AOBlurTarget2),
    ImageDescriptor(mainRenderTarget_D)
}

autoExposureDescriptorSets = {
    ImageDescriptor(autoExposureBlitTargets[#autoExposureBlitTargets]),
    ImageDescriptor(postRenderTarget2)
}



-- Helper function for making a post process pipeline from just the zlsl filename
function PostPipeline(zlsl, stencilTestVal)
    return {
        zlsl,
        string.sub(zlsl, 1, -6) .. "_pixl.spv",
        "shaders/post_vert.spv",
        SF_POSTPROCESS,
        Extent,
        VK_CULL_MODE_NONE,
        VK_SAMPLE_COUNT_1_BIT,
        BM_OPAQUE,
        stencilTestVal or "*0",
        0.0,
        false, -- depthTest
        false -- depthWrite
    }
end

postPipeline = PostPipeline("shaders/post.zlsl")

-- If SSR is disabled, the fog happens during the lighting combine instead of the SSR combine
postPipeline_Fog = PostPipeline("shaders/post-fog.zlsl")

hDOF = PostPipeline("shaders/depthoffield.zlsl")
vDOF = PostPipeline("shaders/depthoffield2.zlsl")
FXAAPipeline = PostPipeline("shaders/fxaa.zlsl")
motionBlurPipeline = PostPipeline("shaders/motionblur.zlsl")
otherEffectsPipeline = PostPipeline("shaders/othereffects.zlsl")
bloomPipeline = PostPipeline("shaders/bloom.zlsl")
bloomBlurXPipeline = PostPipeline("shaders/bloomblurX.zlsl")
bloomBlurYPipeline = PostPipeline("shaders/bloomblurY.zlsl")
blmCombPipeline = PostPipeline("shaders/bloomCombine.zlsl")

SSRCombinePipeline = PostPipeline("shaders/ssrcombine.zlsl")

AOPipeline = PostPipeline("shaders/ssao.zlsl")
AOBlurPipeline = PostPipeline("shaders/ssao-blur.zlsl")

autoExposurePipeline = PostPipeline("shaders/auto-exposure.zlsl")


-- The stencil test can be customized for post processing shaders
-- It starts with a symbol telling it the comparison to be made, followed by the number to check for
-- If the condition fails, that particular pixel is discarded
-- = is VK_COMPARE_OP_EQUAL
-- ! is VK_COMPARE_OP_NOT_EQUAL
-- > is VK_COMPARE_OP_GREATER
-- < is VK_COMPARE_OP_LESS
-- >= is VK_COMPARE_OP_GREATER_OR_EQUAL
-- <= is VK_COMPARE_OP_LESS_OR_EQUAL
-- * is VK_COMPARE_OP_ALWAYS
-- The water writes the stencil value 1, so the SSR will only happen on water pixels
-- It is one of the most expensive post-processing steps, second to the dynamic lighting, so it's only drawn where it's needed
SSRPipeline = PostPipeline("shaders/ssr.zlsl", "=1")

depthPassPipeline = {
    "shaders/core-light.zlsl",
    nil,
    "shaders/core-light_vert.spv",
    SF_DEFAULT,
    Extent,
    VK_CULL_MODE_BACK_BIT,
    SampleCount,
    BM_OPAQUE,
    "*0",
    1.0,
    true,
    true
}

postClearValues = {
    {0.0, 0.0, 0.0, 1.0 }
}

shadowPassClearValues = {
    { 0.0, 0.0, 0.0, 1.0 }
}

renderingProcess = {}

-- It uses the push_back() approach so that stages can be skipped conditionally
function AddToRenderProcess(stage)
    renderingProcess[#renderingProcess + 1] = stage
end

function AddBlitPass(srcImage, dstImage, filter, srcInitialLayout, srcFinalLayout, dstFinalLayout)
    AddToRenderProcess({ RPT_BLIT, srcImage, dstImage, filter, srcFinalLayout, dstFinalLayout, srcInitialLayout })
end

function AddPostPass(frameBuffer, clearValues, pipeline, descriptorSet)
    if not frameBuffer then
        AddToRenderProcess({ RPT_POST, lastRenderPass, nil, clearValues, pipeline, {}, descriptorSet, true })
    else
        AddToRenderProcess({ RPT_POST,  frameBuffer.renderPass, frameBuffer, clearValues, pipeline, {}, descriptorSet, true })
    end
end

postFrame = postFramebuffer1
postDesc = postDescriptorSets_Mid2

local postFlip = false
function FlipPostFrame()
    postFlip = not postFlip
    if postFlip then
        postFrame = postFramebuffer1
        postDesc = postDescriptorSets_Mid2
    else
        postFrame = postFramebuffer2
        postDesc = postDescriptorSets_Mid1
    end
end

-- A nil frame buffer means to render on the final swap chain image
-- A nil pipeline means to use the pipeline from the objects being drawn, otherwise it will override
-- Objects use an ID system to determine when and where they are drawn
-- The ID system could be used to only draw specific meshes in a shadow map or depth pre-pass for example
-- nil descriptors don't really mean anything, it's just that RPT_DEFAULT passes don't use them.

--  passType      renderPass                frameBuffer                 clearValues         pipeline          drawIDs             descriptorSets,       includeMaskedObjects

-- If the render resolution is different from the post-processing resolution, the depth buffer needs to be transferred to another buffer that matches the resolution, or the stencil test won't work right
if ResolutionScale ~= 1.0 then
    AddBlitPass(mainRenderTarget_D, postDepthTarget, VK_FILTER_NEAREST, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
end

-- Dynamic Lighting Pass
-- The RPT_SHADOW pass takes a position and normal buffer and uses it to shade the pixels for each light, also writes the fog value in the B channel while it's there, since sampling the position buffer is very expensive
AddToRenderProcess({ RPT_SHADOW,   lightResultRenderPass,    lightResultFramebuffer,     shadowPassClearValues,    nil, {},   shadowPassDescriptors, true   })

if EnableSSAO then
    AddPostPass(AOFramebuffer, postClearValues, AOPipeline, AODescriptorSets)

    AddPostPass(AOBlurFramebuffer1, postClearValues, AOBlurPipeline, AOBlurDescriptorSets_Start)
    AddPostPass(AOBlurFramebuffer2, postClearValues, AOBlurPipeline, AOBlurDescriptorSets1)
    AddPostPass(AOBlurFramebuffer1, postClearValues, AOBlurPipeline, AOBlurDescriptorSets2)
    AddPostPass(AOBlurFramebuffer2, postClearValues, AOBlurPipeline, AOBlurDescriptorSets1)
end

if EnableSSR then
    -- Combines lighting with colour, needs to be done before SSR
    AddPostPass(postFramebuffer2, postClearValues, postPipeline, postDescriptorSets_Start)

    -- Downscale the depth buffer, so the stencil can be used to mask out the water at a lower resolution
    AddBlitPass(mainRenderTarget_D, SSRStencilTarget, VK_FILTER_NEAREST, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
    -- Also downscales the position buffer, which has the depth in the alpha channel, in theory using a smaller buffer could make it easier to travel large distances when screen-tracing.
    AddBlitPass(mainRenderTarget_P, SSRPosTarget, VK_FILTER_LINEAR, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)

    -- Screen-Space Reflections
    AddPostPass(SSRFramebuffer, postClearValues, SSRPipeline, postDescriptorSets_SSR)

    -- SSR Combine + Fog
    AddPostPass(postFramebuffer1, postClearValues, SSRCombinePipeline, postDescriptorSets_SSRCombine)
else
    -- Combines lighting with colour + Fog
    AddPostPass(postFramebuffer1, postClearValues, postPipeline_Fog, postDescriptorSets_Start)
end

-- Depth of field (split up into 2 passes, one does the horizonal blur, the other does the vertical blur)
-- I've disabled it since there's no way to auto-focus, so it's more of a nuisance, but hey, the feature is there
--AddPostPass(postFramebuffer1, postClearValues, hDOF, postDescriptorSets_Mid2)
--AddPostPass(postFramebuffer2, postClearValues, vDOF, postDescriptorSets_Mid1)

-- FXAA
AddPostPass(postFramebuffer2, postClearValues, FXAAPipeline, postDescriptorSets_Mid1)

-- Auto Exposure
AddPostPass(postFramebuffer1, postClearValues, autoExposurePipeline, autoExposureDescriptorSets)

-- Auto Exposure Gather
-- To get the average brightness of the entire screen, it downscales the image until it's just a single pixel
-- It also does the gather after the exposure is calculated so it can react to what it did on the last frame
local blitSrc = postRenderTarget1
local blitInitialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
local blitSrcFinalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
local blitFinalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
for key, val in pairs(autoExposureBlitTargets) do
    if key == #autoExposureBlitTargets then blitFinalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL end
    AddBlitPass(blitSrc, val, VK_FILTER_LINEAR, blitInitialLayout, blitSrcFinalLayout, blitFinalLayout)
    blitSrc = val
    blitInitialLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
    blitSrcFinalLayout = nil
end

-- Bloom Gather
AddPostPass(bloomFramebuffer1, postClearValues, bloomPipeline, bloomGatherDescriptorSets)

-- Bloom blur passes
-- In order to blur the image as fast as possible, it's blit'd to smaller and smaller textures,
-- and then it does the reverse to blit it back up to full (well okay it only goes up to quarter) resolution again

-- BLIT passes need to transition both the source and destination to TRANSFER_SRC and TRANSFER_DST respectively, so they have the option to transition them back if you need

-- Downscaling
AddBlitPass(bloomRenderTarget1, downScaleRenderTarget2, VK_FILTER_LINEAR, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, nil, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
AddBlitPass(downScaleRenderTarget2, downScaleRenderTarget3, VK_FILTER_LINEAR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, nil, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
AddBlitPass(downScaleRenderTarget3, downScaleRenderTarget4, VK_FILTER_LINEAR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, nil, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
AddBlitPass(downScaleRenderTarget4, downScaleRenderTarget5, VK_FILTER_LINEAR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, nil, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
AddBlitPass(downScaleRenderTarget5, downScaleRenderTarget6, VK_FILTER_LINEAR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, nil, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)

-- Blur the downscaled buffer (I got the idea from UE4)
AddPostPass(bloomBlurFramebuffer1, postClearValues, bloomBlurXPipeline, bloomBlurXDescriptorSets)
AddPostPass(bloomBlurFramebuffer2, postClearValues, bloomBlurYPipeline, bloomBlurYDescriptorSets)

-- Upscaling
AddBlitPass(bloomBlurRenderTarget2, downScaleRenderTarget5, VK_FILTER_LINEAR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, nil, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
AddBlitPass(downScaleRenderTarget5, downScaleRenderTarget4, VK_FILTER_LINEAR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, nil, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
AddBlitPass(downScaleRenderTarget4, downScaleRenderTarget3, VK_FILTER_LINEAR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, nil, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)

-- Combining bloom with the original image
AddPostPass(postFramebuffer2, postClearValues, blmCombPipeline, bloomCombineDescriptors)

-- Motion blur
AddPostPass(postFramebuffer1, postClearValues, motionBlurPipeline, postDescriptorSets_Mid2)

-- Final FX (Vignette, Tone mapping, and Chromatic abberation)
AddPostPass(nil, postClearValues, otherEffectsPipeline, postDescriptorSets_Mid1)

-- The idea is that I can update uniform buffers in here, to control what information is sent to each shader
-- The thing is, I have no idea what would be the best way to construct C structs with lua
function EngineTick()

end