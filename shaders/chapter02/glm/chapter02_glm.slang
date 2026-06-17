import slang;

struct PerFrameData {
    float4x4 mvp;
};

[[vk::push_constant]]
PerFrameData per_frame;

[[vk::constant_id(0)]]
const bool is_wireframe = false;

struct VSInput {
    float3 position;
    float3 color;
};

struct v2p
{
    float4 position : SV_Position;
    float3 color;
};

[shader("vertex")]
v2p main(VSInput input)
{
    v2p output;
    output.position = mul(per_frame.mvp, float4(input.position, 1.));
    output.color = is_wireframe ? float3(1., .647, 0.) : input.color;
    return output;
}

[shader("pixel")]
float4 main(v2p input) : SV_Target
{
    return float4(input.color, 1.);
}
