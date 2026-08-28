struct VSInput
{
    float3 inPos;
    float3 inColor;
    float2 inTexCoord;
};

struct UniformBuffer
{
    float4x4 view;
    float4x4 proj;
};
ConstantBuffer<UniformBuffer> ubo;

struct MeshPushConstants
{
    float4x4 model;
};
[vk::push_constant] ConstantBuffer<MeshPushConstants> pushConstants;

struct VSOutput
{
    float4 pos : SV_Position;
    float3 fragColor;
    float2 fragTexCoord;
};

[shader("vertex")]
VSOutput vertMain(VSInput input)
{
    VSOutput output;
    output.pos = mul(ubo.proj, mul(ubo.view, mul(pushConstants.model, float4(input.inPos, 1.0))));
    output.fragColor = input.inColor;
    output.fragTexCoord = input.inTexCoord;
    return output;
}

Sampler2D texture;
[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET
{
   return texture.Sample(vertIn.fragTexCoord);
}
