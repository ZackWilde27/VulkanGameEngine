# Sun Shadow Pass Shader Builder

numCascades = 3

def WriteCascade(tabs, depth):
    if not depth:
        return "\n" + tabs + "outColour.r = NormalBasedLighting(normal.xyz);\n" + tabs + "outColour.g = pow(outColour.r, coef);\n" + tabs + "outColour.ba = float2(0);\n" + tabs + "return;\n"
    else:
        index = numCascades - depth
        output = tabs + "screenPos = CascadeTransform(pbo.viewProj" + (str(index + 1) if index else "") + ", worldPos);\n"
        output += tabs + "if (OutsideCascade(screenPos.xy))\n" + tabs + "{\n"
        output += WriteCascade(tabs + "\t", depth - 1)
        output += tabs + "}\n" + tabs + "else\n" + tabs + "\tshadowDepth = texture(samplerShadowMap" + str(index + 1) + ", screenPos.xy).r;\n"
    return output

with open("C:/Users/zackw/source/repos/glfwtest/glfwtest/shaders/shadowPass-sun.zlsl", "w") as file:

    file.write('''#VertexShader "post.zlsl"

uniform PostBuffer {
	float4x4 viewProj;
	float2 velocity;
} ubo;

layout(set = 0, binding = 1) uniform sampler2D samplerPosition;
layout(set = 0, binding = 2) uniform sampler2D samplerNormal;

layout(set = 1, binding = 0) uniform PushBufferObject {
''')

    for i in range(numCascades):
        file.write("\tfloat4x4 viewProj" + (str(i + 1) if i else "") + ";\n")
            
    file.write("\tfloat3 lightDir;\n} pbo;\n\n")

    for i in range(numCascades):
        file.write("layout(set = 1, binding = " + str(i + 1) + ") uniform sampler2D samplerShadowMap" + str(i + 1) + ";\n")

    file.write('''\nlayout(location = 0) out float4 outColour;

bool OutsideCascade(float2 coord)
{
	return coord.x < 0.0f || coord.x > 1.0f || coord.y < 0.0f || coord.y > 1.0f;
}

float4 CascadeTransform(float4x4 mat, float4 pos)
{
	float4 coords = mat * pos;
	coords.xy *= 0.5f;
	coords.xy += 0.5f;
	return coords;
}

float NormalBasedLighting(float3 nrm)
{
	return saturate(-dot(nrm, pbo.lightDir));
}

const float shadowBias = 0.000005f;

PixelShader()
{
    float4 normal = texture(samplerNormal, UVs);
    float coef = lerp(150.0f, 25.0f, normal.a);

    float4 worldPos = float4(texture(samplerPosition, UVs).rgb, 1);
    float4 screenPos;

    float shadowDepth;

''')

    tabs = "\t"
    file.write(WriteCascade("\t", numCascades))


    file.write('''
    if ((screenPos.z - shadowBias) < shadowDepth)
        outColour.r = 1;
    else
        outColour.r = 0;

    float d = dot(normal.xyz, pbo.lightDir);
    //outColour.r = lerp(1, outColour.r, pow(absoluteD, absoluteD < 0.45 ? 8.0 : 1.0));

    d = saturate(-d);

    outColour.g = outColour.r * pow(d, coef);
    outColour.r *= d;
    outColour.ba = float2(0);
}''')
