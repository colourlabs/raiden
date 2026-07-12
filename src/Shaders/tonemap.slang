[[vk::binding(0, 0)]]
Texture2D<float4> hdrImage;
[[vk::binding(1, 0)]]
SamplerState hdrSampler;

struct VertexOutput {
  float4 position : SV_Position;
  float2 uv : TEXCOORD0;
};

[shader("vertex")]
VertexOutput vertexMain(uint vertexID : SV_VertexID) {
  VertexOutput output;
  output.uv = float2((vertexID << 1) & 2, vertexID & 2);
  output.position = float4(output.uv * 2.0 - 1.0, 0.0, 1.0);
  return output;
}

float3 acesToneMapping(float3 x) {
  float a = 2.51;
  float b = 0.03;
  float c = 2.43;
  float d = 0.59;
  float e = 0.14;
  return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target {
  float3 color = hdrImage.Sample(hdrSampler, input.uv).rgb;
  color = acesToneMapping(color);
  return float4(color, 1.0);
}
