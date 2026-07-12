
struct PerFrameData {
    float4x4 mvp;
    float4x4 normal;
    uint texture_id;
};

[[vk::push_constant]]
PerFrameData per_frame;

// [[vk::binding(0, 0)]]
// Texture2D kTextures2D[] : register(t0);

// [[vk::binding(1, 0)]]
// SamplerState kSamplers[] : register(s0);

// Bindless array of combined image-samplers
[[vk::binding(0, 0)]]
Sampler2D kSamplers2D[] : register(t0);

struct VSInput {
    float3 position;
    float3 normal;
    float4 color;
    float2 uv;
};

struct v2p
{
    float4 position : SV_Position;
    float3 normal;
    float4 color;
    float2 uv;
};

// float4 textureBindless2D(uint texture_id, uint sampler_id, float2 uv)
// {
//     return kTextures2D[NonUniformResourceIndex(texture_id)].Sample(kSamplers[NonUniformResourceIndex(sampler_id)], uv);
// }

float4 textureBindless2D(uint texture_id, float2 uv)
{
    return kSamplers2D[NonUniformResourceIndex(texture_id)].Sample(uv);
}

[shader("vertex")]
v2p main(VSInput input)
{
    v2p output;

    output.position = mul(per_frame.mvp, float4(input.position, 1.));
    output.normal = normalize(mul((float3x3)per_frame.normal, input.normal));
    output.color = input.color;
    output.uv = input.uv;

    return output;
}

[shader("pixel")]
float4 main(v2p input) : SV_Target
{
    // Simple headlight: Lambert term against a fixed view-space direction, with a small
    // ambient floor so back-facing detail stays readable. Modulated by the glTF vertex color.
    const float3 light_dir = normalize(float3(0.3, 0.6, 1.0));
    float ndotl = max(dot(normalize(input.normal), light_dir), 0.);
    float lighting = 0.2 + 0.8 * ndotl;

    float4 color = textureBindless2D(per_frame.texture_id, input.uv);
    return float4(color.rgb * lighting, color.a);
}
