
struct PerFrameData {
    float4x4 mvp;
    float4x4 model;
    float4x4 view;
    float4x4 normal;
};

[[vk::push_constant]]
PerFrameData per_frame;

[[vk::constant_id(0)]]
const bool is_wireframe = false;

struct VSInput {
    float3 position;
    float2 normal;
};

struct v2p
{
    float4 position : SV_Position;
    float3 color;
};

// https://www.shadertoy.com/view/llfcRl
float3 decode_oct_to_unit(in float2 v)
{
    float3 n = float3(v, 1. - abs(v.x) - abs(v.y));     // Rune Stubbe's version,
    float t = max(-n.z, 0.);                            // much faster than original
    n.x += (n.x > 0.) ? -t : t;                         // implementation of this
    n.y += (n.y > 0.) ? -t : t;                         // technique

    // n.xy = n.z < 0. ? ((1. - abs(n.yx)) * (all(lessThan(n.xy, float2(0.))) ? -1. : +1.)) : n.xy;
    return normalize(n);
}

float3 unpack_attribute_normal_rg16i_norm(in float2 a)
{
    return decode_oct_to_unit(a);
}


[shader("vertex")]
v2p main(VSInput input)
{
    v2p output;

    output.position = mul(per_frame.mvp, float4(input.position, 1.));
    // output.color = is_wireframe ? float3(1., .647, 0.) : input.color;
    output.color = normalize(mul((float3x3)per_frame.normal, unpack_attribute_normal_rg16i_norm(input.normal)));

    return output;
}

[shader("pixel")]
float4 main(v2p input) : SV_Target
{
    return float4(input.color, 1.);
}
