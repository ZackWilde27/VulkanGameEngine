// Making GLSL Feel like HLSL, since that's what I'm used to

// HLSL Types
#define float4x4 mat4
#define float3x3 mat3
#define samplerCUBE samplerCube
#define float2 vec2
#define float3 vec3
#define float4 vec4
#define int2 ivec2
#define int3 ivec3
#define int4 ivec4
#define uint2 uvec2
#define uint3 uvec3
#define uint4 uvec4
#define bool2 bvec2
#define bool3 bvec3
#define bool4 bvec4
#define tex2D texture
#define texCUBE texture

// HLSL Intrinsics
#define frac(x) fract(x)
#define saturate(x) clamp(x, 0.0f, 1.0f)
#define mad(a, b, c) fma(a, b, c)
#define lerp(a, b, c) mix(a, b, c)
#define rcp(x) inverse(x)
#define rsqrt(x) inverse_sqrt(x)

float3 fma(float3 a, float b, float c)
{
	return a * b + c;
}

float3 Desaturate(float3 c, float blend)
{
	float bw = lerp(min(c.r, min(c.g, c.b)), max(c.r, max(c.g, c.b)), 0.5);

	return lerp(c, float3(bw), blend);
}

float Fresnel(in float3 n, in float3 v, in float power)
{
	float f = -dot(n, v);
	f = 1-f;
	f = pow(f, power);
	return f * 0.5f + 0.5f;
}


const float ROUGHNESS_THRESHOLD = 0.05f;

float4 CubeLod(samplerCUBE s, float3 uv, float roughness)
{
	if (roughness > ROUGHNESS_THRESHOLD)
	{
		float lod = int(sqrt(roughness) * textureQueryLevels(s));

		int lowLod = int(floor(lod));
		int highLod = int(ceil(lod));

		float4 sample1 = textureLod(s, uv, lowLod);
		float4 sample2 = textureLod(s, uv, highLod);

		return lerp(sample1, sample2, frac(lod));
	}
	return texture(s, uv);
}

vec3 pow(in vec3 v, in float e)
{
	vec3 o;
	o.x = pow(v.x, e);
	o.y = pow(v.y, e);
	o.z = pow(v.z, e);
	return o;
}

#define ConvertDepth(x) (x * 5000.0)

#define UEContrast(x, contrast) saturate(lerp(-contrast, contrast + 1.0f, x))

#define RGBToNormal(x) ((pow(x, 2.0f)) - 0.5f) * 2.0f
#define NormalToRGB(x) fma((x), 0.5f, 0.5f)

const float tangentXYStrength = 1;//30;

vec3 TangentToWorld(in vec3 t, in vec3 b, in vec3 n, in vec3 v)
{
	vec3 tng = (-t * v.x) + (-b * v.y) + (n * v.z);
	return normalize(tng);
}

float SampleLighting(in vec3 normal, in vec3 light)
{
	float lambert = dot(normal, light);
	return saturate(lambert) + ((lambert * 0.5 + 0.5) * 0.5);
}

#define SampleDepth distance(pos, camPos)

const float3 fogCol = float3(1.0f, 224.0/255.0, 204.0/255.0);

const float FogFade = 0.0002f;

const float densMul = 1.0f;
const float steps = 64;
const float dens = densMul / steps;
const float2 fogSpeed = float2(0.5, -0.5);

//#define VOLUMETRIC_FOG

float3 HandleFog(float3 x, float3 pos, float3 camPos, in sampler2D s, in float timer)
{
	return x;
	float3 ray = pos - camPos;
	float dist = length(ray);
	float density = 0.0f;

	#ifdef VOLUMETRIC_FOG
		ray /= steps;
		float3 rayPos = camPos;


		float4 n1;
		float4 n2;
		float h;
		float2 samplePoint;
		for (int i = 0; i < steps; i++)
		{
			rayPos += ray;

			h = rayPos.z * 10.0;

			samplePoint = timer * fogSpeed + rayPos.xy;

			n1 = texture(s, samplePoint / 500);
			n2 = texture(s, samplePoint / 50);
			density += (n1.r - n2.g - (rayPos.z / 25)) * dens;
		}
	#endif

	density += (dist / 150);

	return lerp(x, fogCol, saturate(density));
}