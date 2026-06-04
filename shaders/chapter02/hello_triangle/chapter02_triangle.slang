static const float2 position[3] = {
    float2(-0.6, -0.4),
    float2(0.6, -0.4),
    float2(0.0, 0.6)
};

static const float3 color[3] = {
    float3(1.0, 0.0, 0.0),
    float3(0.0, 1.0, 0.0),
    float3(0.0, 0.0, 1.0)
};

struct v2p
{
    float4 position : SV_Position;
    float3 color : COLOR;
};

[shader("vertex")]
v2p main(uint vertex_index : SV_VertexID)
{
    v2p output;
    output.position = float4(position[vertex_index], 0.0, 1.0);
    output.color = color[vertex_index];
    return output;
}

[shader("pixel")]
float4 main(v2p input) : SV_Target
{
    return float4(input.color, 1.0);
}
