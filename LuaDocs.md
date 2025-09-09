# Engine.lua Documentation

The post processing pipeline uses Lua so that stages can be added, removed, or changed without recompiling the source code

To make this possible, a ton of Vulkan stuff is exposed to Lua, so I guess this is going to be more of me explaining how Vulkan works

Opening ```engine.lua``` you'll find it's quite a long file with complicated mumbo jumbo (that's Vulkan for you), but hopefully I can try to make more sense of it here.

<br>

There are 2 kinds of post processing passes in Lua:
- Post Pass
- Blit Pass

The post pass is the main one you'd use, it uses a shader to draw on textures.

The blit pass transfers one image to another really quickly, the main use-case is to downscale an image by blitting it to a smaller texture, or do the reverse to upscale

<br>

# Post Pass

To make a post processing pass, you'll need 3 things:
- A frame buffer to draw on
- A descriptor set describing which textures are going to be sampled in the shader
- The shader to use


<br>

The frame buffer is by far the most complicated one, and it should also be done first, so strap in

<br>

## The Framebuffer

A frame buffer at the most basic level is a collection of images to be rendered on, so you'll have to start by creating those textures

Let's say I wanted a texture to write a colour-corrected image, and another texture to write a fog value or something

Use ```CreateRenderTarget()``` to create the textures
```lua
local colourFormat = VK_FORMAT_R8G8B8A8_SRGB
-- I only need the 1 channel to store my fog density so I'll go with this format
local fogFormat = VK_FORMAT_R8_UNORM

-- CreateRenderTarget(format, width, height, sampleCount, usage)
-- usage is optional
local colourImage = CreateRenderTarget(colourFormat, SwapChainWidth, SwapChainHeight, VK_SAMPLE_COUNT_1_BIT)
local fogImage = CreateRenderTarget(fogFormat, SwapChainWidth, SwapChainHeight, VK_SAMPLE_COUNT_1_BIT)

-- The default usage is VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
-- which means it can be rendered to, and then sampled by another shader in a later pass
-- You'll have to change that if you want to use it for transferring in a blit pass
```

<br>

Now that the images have been made, we need a render pass

In Vulkan, render passes describe how the textures in our frame buffer are going to be handled when drawing
```lua
-- The descriptions as their name implies describe each of the textures used in the frame-buffer
local descriptions = {
--  Starting Layout            Final Layout                              Format        Load Op                      Store Op                      MSAA Samples
  { VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, colourFormat, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_SAMPLE_COUNT_1_BIT },
  { VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, fogFormat,    VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_SAMPLE_COUNT_1_BIT }

  -- If the texture is being cleared, it should have a starting layout of UNDEFINED
  -- We want the textures to be read by another shader down the line, so the final layout should be READ_ONLY
  -- The format is the same used to make the texture
  -- Both textures should be cleared before use (but that's not a requirement)
  -- Store Op should be STORE, so our changes apply
  -- Samples just leave at 1 (MSAA off)
}

local refs = {
--  Index into 'descriptions'    Image layout while drawing
  { 0,                           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL },
  { 1,                           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }
  -- Here's where my Vulkan knowledge starts to get shaky
  -- I'm pretty sure this means the layout while it's drawing
  -- So it'll start as UNDEFINED, then become ATTACHMENT_OPTIMAL, before being READ_ONLY at the end
}

-- CreateRenderPass(sampleCount, descriptions, refs, depthRef, resolveAttachments, dependencies)
-- The sample count is a left-over, leave it at 1 sample
-- depthRef is nil since we aren't using a depth buffer
-- resolveAttachments is nil since we aren't using MSAA
-- dependencies is quite complicated and even I barely understand it, just use 'postDepends'
local myRenderPass = CreateRenderPass(VK_SAMPLE_COUNT_1_BIT, descriptions, refs, nil, nil, postDepends)

-- We won't need these just yet until we add our post pass,
-- but I think this is a good spot to put them
-- These are the values that the textures will be cleared to before drawing, since we specified the CLEAR load op
local myClearValues = {
  { 0.0, 0.0, 0.0, 0.0 },
  -- Even though there's only 1 channel in the fogImage, you still need 4 clear values (the other 3 will be ignored)
  { 0.0, 0.0, 0.0, 0.0 }
}
```

<br>

That's the hard part done, now we can make the frame buffer
```lua
local renderTargets = {
  colourImage,
  fogImage
}

-- CreateFrameBuffer(targets, width, height, renderPass)
local myFrameBuffer = CreateFrameBuffer(renderTargets, SwapChainWidth, SwapChainHeight, myRenderPass)
```

<br>

## The Descriptor Set
This part is really easy, we need to know which textures are going to be sampled in our shader

```lua
-- I'll use the main colour texture from rendering, as well as the position texture which has the depth value in the W component
-- The reason I'm using the position buffer instead of the depth buffer is to keep it compatible with MSAA
-- In Vulkan 1.0, depth buffers cannot be resolved, so it can't be sampled in a shader if MSAA is enabled
myDescriptorSet = {
  ImageDescriptor(mainRenderTarget_C),
  ImageDescriptor(mainRenderTarget_P)
}
```

And now we're ready for the shader


<br>

## The Shader

Writing post process shaders is a lot easier than writing shaders for objects, here's the shader I'll be using
```hlsl
#VertexShader "post.zlsl"

// This uniform buffer is automatically included
// If there's no layout(), it will assume set 0, and the binding is based on the order they are defined
uniform PostBuffer {
	float4x4 viewProj;
	float4x4 view;
	float3 camPos;
	float2 velocity;
} ubo;


sampler2D mainRenderTarget_C;
sampler2D mainRenderTarget_P;

layout(location = 0) out float4 colourImage;
layout(location = 1) out float fogImage;

const float4 tintVal = float4(1, 0.85, 0.8, 1);

// With the post shader, the UVs parameter is already implied, no need to add it
// It's as if you had this:
// PixelShader(float2 UVs)

PixelShader()
{
  colourImage = tex2D(mainRenderTarget_C, UVs) * tintVal;

  fogImage = tex2D(mainRenderTarget_P, UVs).w / 5000.0;
}
```
Check out ```ShaderDocs.md``` for more information on writing ZLSL shaders

<br>

The simplest way to add a shader is to call the ```PostPipeline()``` function with the path to the ZLSL file

```lua
-- 'Pipeline' in this case refers to a shader
local myShader = PostPipeline("shaders/myEffect.zlsl")
```

That will give you the default settings, which is
- Assuming that you are re-using the vertex shader from ```post.zlsl```
- No culling
- Opaque blend mode
- No stencil test

For more customization, you'll have to make the table manually, I'll put that after adding the pass

<br>

## Adding the pass
Now we can add our post pass to the pipeline

```lua
AddPostPass(myFrameBuffer, myClearValues, myShader, myDescriptorSet)
```

Given that the shader uses the colour buffer from the main drawing, it should be inserted before the first post pass, and then the next pass will need its descriptor set edited to use our new colour-corrected image instead of mainRenderTarget_C

<br><br>

# Custom Shader Table
A shader in this case is just a table describing the shader's recipe, which will be made as everything is read in.

```lua
local myShader = {
  "path-to-zlsl.zlsl", -- ZLSL filename
  "path-to-pixel-shader_pixl.spv", -- Pixel Shader SPV
  "shaders/post_vert.spv", -- "Vertex Shader SPV"
  SF_POSTPROCESS, -- Shader Type
  Extent, -- Frame-buffer Extent
  VK_CULL_MODE_NONE, -- Cull Mode
  VK_SAMPLE_COUNT_1_BIT, -- MSAA Samples
  BM_OPAQUE, -- Blend Mode
  "*0", -- Stencil Test Value
  0.0, -- Depth Bias
  false, -- Depth Test
  false -- Depth Write
}
```

### ZLSL Filename
The filename of the ZLSL where the pixel/vertex shader originated from

### Pixel Shader SPV
The path to the .SPV for the pixel shader

### Vertex Shader SPV
The path to the .SPV for the vertex shader

### Shader Type
It's engine-specific stuff that I don't really need to go into, but you want SF_POSTPROCESS so that it draws a single triangle covering the whole screen

### Frame-buffer Extent
It's a left-over, just leave it as Extent

### Cull Mode
Can be one of the following:
- VK_CULL_MODE_NONE (Don't Cull)
- VK_CULL_MODE_FRONT_BIT (Cull triangles facing the camera)
- VK_CULL_MODE_BACK_BIT (Cull triangles facing away from the camera)
- VK_CULL_MODE_FRONT_AND_BACK (Don't draw anything, honestly why would you ever do this)

I use NONE just because the triangle is guarunteed to face the camera so there's no reason to even check it

### MSAA Samples
Also a left-over, post processing only happens at 1 sample (MSAA off)

### Blend Mode
Can be one of the following:
- BM_OPAQUE (No blending)
- BM_TRANSPARENT (Blend based on alpha)
- BM_ADDITIVE (Blend by adding)
- BM_MAX (Chooses the brightest colour)

Usually you'd stick with BM_OPAQUE, but depending on what it's doing you might need the other ones

### Stencil Test Value
It's a string, the symbol at the start represents the test being done, and the rest is the value to test for

I don't need to tell you most of them, you already know what <, >, <=, and >= mean
- ```*``` is ALWAYS, the stencil test will pass regardless of the value
- ```!``` is NOT_EQUAL
- ```=``` is EQUAL

So if I wanted this post process shader to run on pixels with a stencil value greater than 2 I'd put in ```">2"```

Or if I wanted to check if the stencil value is not equal to 3 I'd write ```"!3"```

### Depth Bias
Another left-over, leave it at 0.0

### Depth Test
Whether or not pixels get culled if they are behind something, this one is another left-over, you should probably leave it at false

### Depth Write
Whether or not the depth of new pixels is written to the depth buffer, yet another left-over, leave it at false as well

<br><br>

# Blit Passes
Compared to post passes, blit passes are super easy

All we need is a texture to transfer from, and a texture we want it transferred to

```lua
-- Let's say I wanted to transfer the colour-corrected image from our post pass to a new texture that's half the size

-- Go back up to where 'colourImage' was created and change the usage to include the TRANSFER_SRC_BIT
colourImage = CreateRenderTarget(colourFormat, SwapChainWidth, SwapChainHeight, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)

-- Then the destination texture needs the TRANSFER_DST_BIT
local destTexture = CreateRenderTarget(colourFormat, SwapChainWidth / 2, SwapChainHeight / 2, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
```

Now to add the blit pass after the post pass
```lua
AddPostPass(myFrameBuffer, myClearValues, myShader, myDescriptorSet)

-- AddBlitPass(source, destination, filter, source initial layout, source final layout, destination final layout)

-- The filter should be LINEAR so it blurs the image instead of pixelating it
-- The initial layout of our source image is READ_ONLY, since that's how the render pass left it
-- The final layout being nil means 'don't care', for cases where the source won't be read again
-- The final layout of the destination image will be READ_ONLY so we can sample it in a future post pass
AddBlitPass(sourceTexture, destTexture, VK_FILTER_LINEAR, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, nil, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
```
