struct VertexInput
{
  [[vk::location(0)]] float3 pos : POSITION;
  [[vk::location(1)]] float4 color : COLOR;
};

struct VertexOutput
{
  float4 position : SV_Position;
  [[vk::location(0)]] float4 color : COLOR;
};

struct PushConstants
{
  float4x4 viewProj;
  float4x4 model;
};

[[vk::push_constant]]
PushConstants pc;

[shader("vertex")]
VertexOutput vertexMain(VertexInput input)
{
  VertexOutput output;
  output.position = mul(pc.viewProj, mul(pc.model, float4(input.pos, 1.0)));
  output.color = input.color;
  return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target0
{
  return input.color;
}
