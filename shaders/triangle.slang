#include "include/common.slangh"

[[vk::binding(0, 1)]] Texture2D<float4> tex;
[[vk::binding(0, 1)]] SamplerState samp;

struct VertexInput {
  float2 pos : POSITION;
  float3 color : COLOR;
  float2 uv : TEXCOORD;
};

struct VertexOutput {
  float4 position : SV_Position;
  float3 color : COLOR;
  float2 uv : TEXCOORD;
};

[shader("vertex")]
VertexOutput vertexMain(VertexInput input) {
  VertexOutput output;
  float angle = frameUBO.extra.x * 0.5;
  float s = sin(angle);
  float c = cos(angle);
  float2x2 rot = float2x2(c, -s, s, c);
  float3 pos = float3(mul(rot, input.pos), 0.0);
  output.position = mul(frameUBO.projection, mul(frameUBO.view, float4(pos, 1.0)));
  output.color = input.color;
  output.uv = input.uv;
  return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target {
  float4 texel = tex.Sample(samp, input.uv);
  return float4(texel.rgb * input.color, 1.0);
}
