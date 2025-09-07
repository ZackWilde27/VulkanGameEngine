#version 450

#include "zutils.glsl"
layout(location = 2) in float timer;
layout(location = 1) in float3 camPos;
layout(location = 0) in float3 pos;



layout(set = 2, binding = 0) uniform samplerCUBE cubeSampler;
layout(set = 2, binding = 1) uniform sampler2D noiseSampler;

const float2 speed1 = float2(-1.5, 1.5);
const float2 speed2 = float2(-1.2, -1.2);

const int layers = 16;

const float3 cloudColour = float3(0.8f, 0.9f, 1.0f);

const float cloudHeight = 128.0f;
const float cloudDarkening = 0.5f;
const float cloudMinDarkness = 0.2f;
const float cloudDensity = 3.0f;
const float cloudMaxDensity = 0.8f;
const float cloudDetailStrength = 0.5f;
const float cloudCoverage = 1.0f;

const float cloudScale1 = 5600;
const float cloudScale2 = 200;

bool RayPlane(float3 rayOrigin, float3 rayDir, float3 planeOrigin, float3 planeNormal, out float dist)
{
	float d = dot(planeNormal, rayDir);
	if (d < -0.001f)
	{
		dist = dot(planeOrigin - rayOrigin, planeNormal) / d;
		return dist >= 0;
	}
	return false;
}

float3 Clouds(float3 incident, float3 colour, float timer)
{
    float dist;

	if (RayPlane(camPos, incident, float3(0, 0, 200), float3(0, 0, -1), dist))
	{
        float density = 0;
        float brightness = 0.6f;
        float hitDensity;
        float3 hit = camPos + (incident * dist);

        float cloudStrength = 1-saturate(dot(incident, float3(0.0f, 0.0f, 1.0f)));  
        cloudStrength = max(cloudStrength, 0.05f);

        for (int i = 0; i < layers; i++)
        {
            hit += (incident * (cloudHeight / layers) * cloudStrength);
            hitDensity = tex2D(noiseSampler, (speed1 * timer + hit.xy + (float2(i) / layers)) / cloudScale1).r - (tex2D(noiseSampler, (speed2 * timer + hit.xy + (float2(i) / layers)) / cloudScale2).g * cloudDetailStrength);
            hitDensity = saturate(hitDensity);
            density += hitDensity * cloudDensity;
            brightness = max(brightness * saturate(1-(hitDensity * (cloudDarkening / layers))), cloudMinDarkness);
        }
        return lerp(colour, cloudColour * brightness, min(density / layers, cloudMaxDensity));
	}
	return colour;
}

layout(location = 0) out float4 outColor;
layout(location = 1) out float4 outNormal;
layout(location = 2) out float4 outPos;
layout(location = 3) out float4 outGI;

void main()
{
	outPos = float4(pos, 5001.f);
    float3 incident = normalize(pos - camPos);

    outNormal = float4(-incident, 1);

    outGI = float4(1.0f);

    outColor = float4(Clouds(incident, texture(cubeSampler, incident).rgb, timer), 1.0f);
}