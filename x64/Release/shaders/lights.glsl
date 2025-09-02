struct Light
{
    float3 position;
    float3 colour;
};

const Light LIGHTS[] = {
    { float3(0.0f, 0.0f, 0.0f), float3(1.0f, 1.0f, 1.0f) }
};

const int NUMLIGHTS = 1;