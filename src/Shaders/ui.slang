struct VertexInput
{
  [[vk::location(0)]] float2 pos : POSITION;
  [[vk::location(1)]] float2 uv : TEXCOORD;
  [[vk::location(2)]] float4 color : COLOR;
};

struct VertexOutput
{
  float4 position : SV_Position;
  [[vk::location(0)]] float2 uv : TEXCOORD;
  [[vk::location(1)]] float4 color : COLOR;
};

[[vk::push_constant]]
cbuffer PushConstants { float2 screenSize; };

[[vk::binding(0, 2)]] Texture2D uTexture;
[[vk::binding(0, 1)]] SamplerState uSampler;

[shader("vertex")]
VertexOutput vertexMain(VertexInput input)
{
  VertexOutput output;
  float2 ndc;
  ndc.x = (input.pos.x / screenSize.x) * 2.0 - 1.0;
  ndc.y = -(input.pos.y / screenSize.y) * 2.0 + 1.0;
  output.position = float4(ndc, 0.0, 1.0);
  output.uv = input.uv;
  output.color = input.color;
  return output;
}

float3 srgbToLinear(float3 c)
{
  float3 lo = c / 12.92;
  float3 hi = pow((c + 0.055) / 1.055, 2.4);
  return lerp(lo, hi, step(0.04045, c));
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
  float4 texel = uTexture.Sample(uSampler, input.uv);
  float3 linearColor = srgbToLinear(input.color.rgb) * texel.rgb;
  return float4(linearColor, input.color.a * texel.a);
}