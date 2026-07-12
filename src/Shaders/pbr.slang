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

  // shadow calculation
  float shadow = 1.0;
  if (frameUBO.lightColor.w > 0.5)
  {
    float4 lightSpacePos = mul(frameUBO.lightViewProj, float4(input.worldPos, 1.0));
    float3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords.xy = projCoords.xy * 0.5 + 0.5;
    projCoords.y = 1.0 - projCoords.y;

    if (projCoords.x >= 0.0 && projCoords.x <= 1.0 &&
        projCoords.y >= 0.0 && projCoords.y <= 1.0 &&
        projCoords.z >= 0.0 && projCoords.z <= 1.0)
    {
      float currentDepth = projCoords.z;
      float bias = max(0.005 * (1.0 - dot(N, L)), 0.001);

      // PCF 3x3
      float2 texelSize = 1.0 / 2048.0;
      shadow = 0.0;
      for (int x = -1; x <= 1; ++x)
      {
        for (int y = -1; y <= 1; ++y)
        {
          float pcfDepth = shadowMap.Sample(samp, projCoords.xy + float2(x, y) * texelSize).r;
          shadow += currentDepth - bias > pcfDepth ? 0.1 : 1.0;
        }
      }
      shadow /= 9.0;
    }
  }

  float NdotL = max(dot(N, L), 0.0);
  float3 direct = (kD * albedo.rgb / PI + specular) * radiance * NdotL * shadow;

  // --- IBL or hemispheric ambient ---
  float3 ambientIBL = float3(0, 0, 0);
  if (frameUBO.ambientGround.w > 0.5)
  {
    // diffuse IBL from irradiance map
    float3 irradiance = irradianceMap.Sample(samp, N).rgb;
    float3 diffuseIBL = irradiance * albedo.rgb * (1.0 - metallic);

    // specular IBL from pre-filtered env map + BRDF LUT
    float NdotV = max(dot(N, V), 0.0);
    float3 R = reflect(-V, N);
    float3 prefilteredColor = prefilteredMap.SampleLevel(samp, R, roughness * 4.0).rgb;
    float2 brdf = brdfLUT.Sample(samp, float2(NdotV, roughness)).rg;
    float3 specularIBL = prefilteredColor * (F * brdf.x + brdf.y);

    ambientIBL = (diffuseIBL + specularIBL);
  }
  else
  {
    float3 skyColor = frameUBO.ambientSky.rgb * frameUBO.ambientSky.w;
    float3 groundColor = frameUBO.ambientGround.rgb;
    float NdotUp = dot(N, float3(0, 1, 0)) * 0.5 + 0.5;
    ambientIBL = lerp(groundColor, skyColor, NdotUp) * albedo.rgb;
  }

  float occlusion = 1.0;
  if (material.hasOcclusionMap != 0)
    occlusion = 1.0 - material.occlusionStrength * (1.0 - occlusionMap.Sample(samp, input.uv).r);

  float3 emissive = material.emissiveFactor.rgb;
  if (material.hasEmissiveMap != 0)
    emissive *= emissiveMap.Sample(samp, input.uv).rgb;

  float3 color = ambientIBL * occlusion + direct * occlusion + emissive;

  float alpha = albedo.a;
  if (material.alphaMode == 1)
  {
    if (alpha < material.alphaCutoff)
      discard;
    alpha = 1.0;
  }

  return float4(color, alpha);
}
