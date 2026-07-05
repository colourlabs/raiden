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

[[vk::binding(0, 1)]] SamplerState uSampler;
[[vk::binding(0, 2)]] Texture2D uTexture;

float3 srgbToLinear(float3 c)
{
  float3 lo = c / 12.92;
  float3 hi = pow((c + 0.055) / 1.055, 2.4);
  return lerp(lo, hi, step(0.04045, c));
}

[shader("vertex")]
VertexOutput vertexMain(VertexInput input)
{
  VertexOutput output;
  float2 ndc;
  ndc.x = (input.pos.x / screenSize.x) * 2.0 - 1.0;
  ndc.y = (input.pos.y / screenSize.y) * 2.0 - 1.0;

  output.position = float4(ndc, 0.0, 1.0);
  output.uv = input.uv;

  output.color.rgb = srgbToLinear(input.color.rgb);
  output.color.a = input.color.a;

  return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target0
{
    float coverage = uTexture.Sample(uSampler, input.uv).a;
    return float4(input.color.rgb * input.color.a * coverage,
                  input.color.a * coverage);
}
