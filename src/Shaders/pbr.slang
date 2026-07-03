#include "include/common.slangh"

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
  [[vk::location(2)]] float2 uv : TEXCOORD;
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

  output.normal = normalize(mul((float3x3)model, input.normal));

  output.color = input.color;
  output.uv = input.uv;

  return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
  float4 albedo = albedoMap.Sample(samp, input.uv) * material.baseColorFactor;

  float3 N = calcNormal(normalMap, samp, input.uv, input.normal, input.worldPos);
  float3 V = normalize(frameUBO.cameraPos.xyz - input.worldPos);
  float3 L = normalize(frameUBO.lightDir.xyz);
  float3 H = normalize(V + L);

  float metallic = material.metallicFactor;
  float roughness = material.roughnessFactor;

  float3 F0 = float3(0.04, 0.04, 0.04);
  F0 = lerp(F0, albedo.rgb, metallic);

  float3 radiance = frameUBO.lightColor.rgb * frameUBO.lightDir.w;

  float NDF = distributionGGX(N, H, roughness);
  float G = geometrySmith(N, V, L, roughness);
  float3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

  float3 kS = F;
  float3 kD = (1.0 - kS) * (1.0 - metallic);

  float3 numerator = NDF * G * F;
  float denom = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
  float3 specular = numerator / max(denom, 0.001);

  float NdotL = max(dot(N, L), 0.0);
  float3 direct = (kD * albedo.rgb / PI + specular) * radiance * NdotL;
  float occlusion = 1.0;
  if (material.hasOcclusionMap != 0)
    occlusion = 1.0 - material.occlusionStrength * (1.0 - occlusionMap.Sample(samp, input.uv).r);

  float3 emissive = material.emissiveFactor.rgb;
  if (material.hasEmissiveMap != 0)
    emissive *= emissiveMap.Sample(samp, input.uv).rgb;

  float hemi = 0.5 + 0.5 * N.y;
  float3 ambient = lerp(frameUBO.ambientGround.rgb, frameUBO.ambientSky.rgb, hemi) * albedo.rgb * frameUBO.ambientSky.w;
  float3 color = ambient + direct * occlusion + emissive;

  if (material.vertexColorMode == 1)
    color *= input.color;
  else if (material.vertexColorMode == 2)
    color += input.color;

  color = color / (color + float3(1.0, 1.0, 1.0));

  float alpha = albedo.a;
  if (material.alphaMode == 1)
  {
    if (alpha < material.alphaCutoff)
      discard;
    alpha = 1.0;
  }

  return float4(color, alpha);
}
