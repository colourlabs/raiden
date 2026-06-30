#include "include/common.slangh"

// set 1, sampler shared across all texture slots
[[vk::binding(0, 1)]]
SamplerState samp;

struct VertexInput
{
  [[vk::location(0)]] float3 pos : POSITION;
  [[vk::location(1)]] float3 normal : NORMAL;
  [[vk::location(2)]] float3 color : COLOR;
  [[vk::location(3)]] float2 uv : TEXCOORD;
};

struct VertexOutput
{
  float4 position : SV_Position;
  [[vk::location(0)]] float3 normal : NORMAL;
  [[vk::location(1)]] float3 color : COLOR;
  [[vk::location(2)]] float2 uv : TEXCOORD; // <--- Force this to 2
  [[vk::location(3)]] float3 worldPos : TEXCOORD1;
};

[[vk::push_constant]]
cbuffer PushConstants {
  float4x4 model;
};

[shader("vertex")]
VertexOutput vertexMain(VertexInput input)
{
  VertexOutput output;

  float4 worldPos = mul(model, float4(input.pos, 1.0));
  output.worldPos = worldPos.xyz;
  output.position = mul(frameUBO.projection, mul(frameUBO.view, worldPos));

  // transform normal into world space
  // using model matrix, good enough for right now
  output.normal = normalize(mul((float3x3)frameUBO.model, input.normal));

  output.color = input.color;

  // pass through UV directly (material transform is applied via material path)
  output.uv = input.uv;

  return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
  return albedoMap.Sample(samp, input.uv);
}