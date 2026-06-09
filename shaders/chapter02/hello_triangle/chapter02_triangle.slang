struct VSInput {
    float2 position;
    float2 uv;
};

struct v2p
{
    float4 position : SV_Position;
    float3 color : COLOR;
};

[shader("vertex")]
v2p main(VSInput input)
{
    v2p output;
    output.position = float4(input.position, 1., 1.);
    // output.color = float3(.2, .4, .1); // color[vertex_index];
    output.color = float3(input.uv, 0.);
    return output;
}

[shader("pixel")]
float4 main(v2p input) : SV_Target
{
    return float4(input.color, 1.);
}
