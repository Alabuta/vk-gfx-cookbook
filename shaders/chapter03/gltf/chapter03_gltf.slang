
struct PerFrameData {
    float4x4 mvp;
    float4x4 model;
    float4x4 view;
    float4x4 normal;
};

[[vk::push_constant]]
PerFrameData per_frame;

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

    return float4(input.color.rgb * lighting, input.color.a);
}
