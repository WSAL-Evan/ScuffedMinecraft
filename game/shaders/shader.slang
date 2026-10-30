struct VSInput {
    float3 inPosition;
    float2 inTexCoord;
    uint32_t inLightData;
};

struct UniformBuffer {
    float4x4 view;
    float4x4 proj;
};
ConstantBuffer<UniformBuffer> ubo;

struct PushConstants {
    float4x4 model;
};
[[vk::push_constant]] PushConstants objectInfo;

struct VSOutput {
    float4 pos : SV_Position;
    float2 fragTexCoord;
    float lightValue;
};

float adjustLightValue(float value)
{
    return value / (2.0 - 1.0 * value);
}

float getAOMultiplier(float ao)
{
    return 1.0 - ao * 0.3;
}

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    output.pos = mul(ubo.proj, mul(ubo.view, mul(objectInfo.model, float4(input.inPosition, 1.0))));
    output.fragTexCoord = input.inTexCoord;

    // Calculate lighting
    int blockLight = input.inLightData & 0xFF;
    int skyLight = (input.inLightData >> 8) & 0xFF;
    int ao = (input.inLightData >> 16) & 0x3;

    output.lightValue = max(adjustLightValue(max(blockLight, skyLight) / 255.0), 0.02) * getAOMultiplier(ao);

    return output;
}

Sampler2D texture;

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_Target
{
    return texture.Sample(vertIn.fragTexCoord) * vertIn.lightValue;
}