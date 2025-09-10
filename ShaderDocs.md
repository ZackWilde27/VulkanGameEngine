# ZLSL Shader Documentation

This is the README for writing shaders

In order to make writing shaders easier for myself, I made a frankenstein monster of GLSL and HLSL which I have dubbed ZLSL

It combines both the vertex shader and pixel shader into a single file, though you can re-use vertex shaders if needed

Deep down, it's actually GLSL, and you could write the whole thing in it if you want to

<br>

## Types
Types from HLSL have been ported to GLSL through macros

- float4x4 == mat4
- float3x3 == mat3
- float4 == vec4
- float3 == vec3
- float2 == vec2
- int4 == ivec4
- int3 == ivec3
- int2 == ivec2
- uint4 == uvec4
- uint3 == uvec3
- uint2 == uvec2
- samplerCUBE == samplerCube

## Intrinsics
Certain intrinsics also have the option to use their HLSL name

- lerp() == mix()
- mad() == fma()
- tex2D() == texture()
- texCUBE() == texture()
- frac() == fract()
- rsqrt() == inverse_sqrt()

<br>

## Layout
The basic structure of a ZLSL file looks like this:
```hlsl
// There is no need to specify the return value, it's always void

// The inputs of the vertex shader represent the vertex data
VertexShader(float3 inPosition, float3 inNormal, float3 inUV, float3 inTangent)
{
  that = float4(0, 0, 1, 1);
  //...
}

// The inputs of the pixel shader are interpolators that the vertex shader can write to
PixelShader(float4 that)
{
  //...
}
```

To re-use a vertex shader from a different ZLSL file, you can add ```#VertexShader "path-to-shader.zlsl"```
```hlsl
#VertexShader "other-shader.zlsl"

// The pixel shader needs to have the same inputs as the other-shader
PixelShader(float4 that)
{
  //...
}
```

<br>

### The Pixel Shader

To write to a texture in the pixel shader, you need to add it like you would in GLSL
```hlsl
// Frame buffers are a collection of render targets to draw on, so the layout needs to specify the index into the colour buffers
layout(location = 0) out float4 outColour;

PixelShader(float4 that)
{
  outColour = that;
}
```

Because the frame-buffer may contain several different textures to draw on, you'll have to add each one
```hlsl
// Shaders that draw objects need to have all of these, but you can choose whether or not to write to them
// Also, the index ignores the depth/stencil buffer, it's only considering colour buffers
layout(location = 0) out float4 outColour;
layout(location = 1) out float4 outNormal;
layout(location = 2) out float4 outPosition;
layout(location = 3) out float4 outGI;

PixelShader(float4 that)
{
  //...
}
```

<br>

Adding textures to sample from is a bit more complicated, you have to specify the set and binding they came from in the layout
```hlsl
// The set is the index into the descriptor sets that have been bound
// while the binding is the index in that descriptor set

// To make sense of the set and bindings for drawing objects, I'll have to explain how my engine works a bit:
//
// The first set is bound at the very start of each frame and doesn't change, so it contains per-frame data
// There's a uniform buffer at binding 0, containing stuff like the viewProj matrix, and camera's location
// Then there's a combined light-map for all objects at binding 1
//
// The second set is bound per-object
// It has a storage buffer containing an array of world matrices for each instance at binding 0
// Then another storage buffer containing light-map scales and offsets for each instance at binding 1
//
// The third set is bound per-material, it has all the textures used for that material (and the cubemap)

// I want access to the first set's light-map, so that's set 0, binding 1
layout(set = 0, binding = 1) uniform sampler2D aoSampler;

// Then I want all the textures from the material, so that's set 2, bindings 0-2
layout(set = 2, binding = 0) uniform sampler2D samplerCol;
layout(set = 2, binding = 1) uniform sampler2D samplerNrm;
layout(set = 2, binding = 2) uniform samplerCUBE cubeSampler;

PixelShader(float4 that)
{
  //...
}
```
It's much easier when writing post process shaders, drawing objects is a lot more hard-coded

<br>

### The Vertex Shader
The vertex shader needs to write to ```gl_Position``` at some point for it to work
```hlsl
VertexShader(float3 inPosition)
{
  gl_Position = float4(inPosition, 1);
}
```

<br>

In my engine, there's 2 types of data that can be given to the vertex shader
- Uniform Buffers
- Storage Buffers

Uniform buffers are just structs, a collection of various things

Storage buffers are structs that contain an array with an unknown length. I use them for instancing, so the same shader can handle any amount of instances being drawn

<br>

To add a uniform buffer to a shader, it's just like GLSL
```hlsl
// Going back to how my engine works, the first set has the per-frame data stored in a uniform buffer at binding 0
layout(set = 0, binding = 0) uniform UniformBufferObject {
  float4x4 viewProj;
  float3 CAMERA;
  float time;
} ubo;
```

<br>

Adding a storage buffer is a little different
```hlsl
// The second set contains an array of matrices for each instance at binding 0
layout(set = 1, binding = 0) readonly buffer MatrixArray {
  float4x4 data[];
} matrices;

VertexShader()
{
  // It will be indexed into by the gl_InstanceIndex
  float4x4 myWorldMatrix = matrices.data[gl_InstanceIndex];
}
```

<br>

Here's an example of a full ZLSL file
```hlsl
layout(set = 0, binding = 0) uniform UniformBufferObject {
    float4x4 viewProj;
    float3 CAMERA; // position of the camera, in world space
    float time; // amount of time passed
} ubo;

layout(set = 1, binding = 0) readonly buffer MatrixArray{
    float4x4 data[];
} matrices;

layout(set = 1, binding = 1) readonly buffer ShadowMapOffsetArray{
    float4 data[];
} shadowMapOffsets;


#define worldMatrix (matrices.data[gl_InstanceIndex])

VertexShader(float3 inPosition, float3 inNormal, float4 inUV, float3 inTangent)
{
    nrm = normalize(float3x3(worldMatrix) * inNormal);

    float4 worldPosition = worldMatrix * float4(inPosition, 1.0);

    gl_Position = ubo.viewProj * worldPosition;

    UVs = inUV.xy;
	lightmapUV = (inUV.zw * shadowMapOffsets.data[gl_InstanceIndex].zw) + shadowMapOffsets.data[gl_InstanceIndex].xy;
	tangent = normalize(float3x3(worldMatrix) * inTangent);
    pos = worldPosition.xyz;
    camPos = ubo.CAMERA;
}

layout(location = 0) out float4 outColor;
layout(location = 1) out float4 outNormal;
layout(location = 2) out float4 outPosition;
layout(location = 3) out float4 outGI;

layout(set = 0, binding = 1) uniform sampler2D aoSampler;

layout(set = 2, binding = 0) uniform sampler2D texSampler;
layout(set = 2, binding = 1) uniform sampler2D nrmSampler;
layout(set = 2, binding = 2) uniform samplerCUBE cubeSampler;


PixelShader(float3 nrm, float2 UVs, float2 lightmapUV, float3 pos, float3 tangent, float3 camPos)
{
	float3 viewLine = pos - camPos;
	outPosition = float4(pos, length(viewLine));

	float3 normal = (sqrt(texture(nrmSampler, UVs).rgb) - 0.5) * 2.0;

	float3 view = normalize(viewLine);

	float3 reflectionVector = reflect(view, nrm);

	float3 binormal = cross(nrm, tangent);

	float3 worldNormal = TangentToWorld(tangent, binormal, nrm, normal);

	float rgh = 1.0;

	outNormal = float4(worldNormal, rgh);

	float4 col = texture(texSampler, UVs);

	float3 reflection = CubeLod(cubeSampler, reflectionVector, rgh).rgb;

	reflection = Desaturate(reflection, 0.4f) * Fresnel(nrm, view, 3.0f) * 0.6f;

	outGI = texture(aoSampler, lightmapUV);
	outColor = float4(col.rgb + reflection, 1);
}
```

<br>

## Compiling Shaders
### Windows
Run ```compile_all_shaders.bat``` located under ```x64/Release/shaders/```

### Linux
Open a terminal in ```x64/Release/shaders``` and type in this:
```console
./compile_all_shaders.sh
```

<br>

Those scripts will automate everything, removing old .SPV, .FRAG, and .VERT files, running the source-to-source compiler, and then using glslc to compile all the new .FRAG and .VERT files

The engine will detect when shaders have changed while the game is running, and will automatically re-compile them, giving you a live-preview
