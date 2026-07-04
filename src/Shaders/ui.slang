struct VertexInput
{
  [[vk::location(0)]] float2 pos : POSITION;
  [[vk::location(1)]] float2 uv : TEXCOORD;
  [[vk::location(2)]] float4 color : COLOR;
};

struct VertexOutput
{
  float4 position : SV_Position;
  [[vk::location(0)]] float4 color : COLOR;
};

[[vk::push_constant]]
cbuffer PushConstants {
  float2 screenSize;
};

[shader("vertex")]
VertexOutput vertexMain(VertexInput input)
{
  VertexOutput output;
  float2 ndc;
  ndc.x = (input.pos.x / screenSize.x) * 2.0 - 1.0;
  ndc.y = -(input.pos.y / screenSize.y) * 2.0 + 1.0;
  output.position = float4(ndc, 0.0, 1.0);
  output.color = input.color;
  return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
  return input.color;
}
