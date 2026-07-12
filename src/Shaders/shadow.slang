#include "include/common.slangh"

[[vk::push_constant]]
cbuffer PushConstants {
  float4x4 model;
};

struct VertexInput
{
  [[vk::location(0)]] float3 pos : POSITION;
};

struct VertexOutput
{
  float4 position : SV_Position;
};

[shader("vertex")]
VertexOutput vertexMain(VertexInput input)
{
  VertexOutput output;
  float4 worldPos = mul(model, float4(input.pos, 1.0));
  output.position = mul(frameUBO.lightViewProj, worldPos);
  return output;
}
