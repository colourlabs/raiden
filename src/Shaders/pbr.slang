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

  // Transform by the inverse-transpose matrix. Slang does not expose a matrix
  // inverse intrinsic here, so build its cofactor rows directly; normalization
  // removes the common determinant factor.
  float3x3 model3 = (float3x3)model;
  float3 cofactor0 = cross(model3[1], model3[2]);
  float3 cofactor1 = cross(model3[2], model3[0]);
  float3 cofactor2 = cross(model3[0], model3[1]);
  output.normal = normalize(float3(dot(cofactor0, input.normal),
                                   dot(cofactor1, input.normal),
                                   dot(cofactor2, input.normal)));

  output.color = input.color;
  output.uv = input.uv;

  return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
  float2 uv = input.uv * material.uvScale + material.uvOffset;
  if (material.triplanarMapping != 0)
  {
    float3 absNormal = abs(normalize(input.normal));
    if (absNormal.x >= absNormal.y && absNormal.x >= absNormal.z)
      uv = float2(input.worldPos.z, input.worldPos.y);
    else if (absNormal.z >= absNormal.y)
      uv = float2(input.worldPos.x, input.worldPos.y);
    else
      uv = float2(input.worldPos.x, input.worldPos.z);
    uv = uv * material.uvScale + material.uvOffset;
  }
  float4 albedo = albedoMap.Sample(samp, uv) * material.baseColorFactor;

  float3 N = calcNormal(normalMap, samp, uv, input.normal, input.worldPos);
  float3 V = normalize(frameUBO.cameraPos.xyz - input.worldPos);
  float3 L = normalize(frameUBO.lightDir.xyz);
  float3 H = normalize(V + L);

  float metallic = material.metallicFactor;
  float roughness = material.roughnessFactor;
  if (material.hasMetallicRoughnessMap != 0)
  {
    // glTF and the demo's ARM textures pack roughness in G and metallic in B.
    float4 metallicRoughness = metallicRoughnessMap.Sample(samp, uv);
    roughness *= metallicRoughness.g;
    metallic *= metallicRoughness.b;
  }
  roughness = clamp(roughness, 0.04, 1.0);

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

    // fade shadow at frustum edges to avoid hard boundary
    float2 fadeXY = smoothstep(0.0, 0.05, projCoords.xy) * (1.0 - smoothstep(0.95, 1.0, projCoords.xy));
    float frustumFade = fadeXY.x * fadeXY.y;
    float zFade = smoothstep(0.0, 0.05, projCoords.z) * (1.0 - smoothstep(0.95, 1.0, projCoords.z));
    frustumFade *= zFade;

    if (projCoords.x >= 0.0 && projCoords.x <= 1.0 &&
        projCoords.y >= 0.0 && projCoords.y <= 1.0 &&
        projCoords.z >= 0.0 && projCoords.z <= 1.0)
    {
      float currentDepth = projCoords.z;
      float bias = max(0.005 * (1.0 - dot(N, L)), 0.001);

      // PCF 5x5
      float2 texelSize = 1.0 / frameUBO.shadowMapSize;
      shadow = 0.0;
      for (int x = -2; x <= 2; ++x)
      {
        for (int y = -2; y <= 2; ++y)
        {
          float pcfDepth = shadowMap.Sample(samp, projCoords.xy + float2(x, y) * texelSize).r;
          shadow += currentDepth - bias > pcfDepth ? 0.45 : 1.0;
        }
      }
      shadow /= 25.0;
      shadow = lerp(1.0, shadow, frustumFade);
    }
  }

  float NdotL = max(dot(N, L), 0.0);
  float3 direct = (kD * albedo.rgb / PI + specular) * radiance * NdotL * shadow;

  // point light accumulation
  for (uint i = 0; i < frameUBO.pointLightCount; ++i)
  {
    PointLight pl = pointLights[i];
    float3 plPos = pl.positionRange.xyz;
    float plRange = pl.positionRange.w;
    float3 plColor = pl.colorIntensity.rgb;
    float plIntensity = pl.colorIntensity.w;

    float3 Lp = plPos - input.worldPos;
    float dist = length(Lp);
    if (dist > plRange) continue;

    // Inverse-square attenuation with a smooth cutoff at the authored range.
    float ratio = dist / plRange;
    float rangeFade = saturate(1.0 - ratio * ratio * ratio * ratio);
    float attenuation = (rangeFade * rangeFade) / max(dist * dist, 1.0);

    float3 Lpl = normalize(Lp);
    float3 kD_pl;
    float3 specular_pl = cookTorrance(N, V, Lpl, albedo.rgb, metallic, roughness, F0, kD_pl);

    float plNdotL = max(dot(N, Lpl), 0.0);
    direct += (kD_pl * albedo.rgb / PI + specular_pl) * plColor * plIntensity * attenuation * plNdotL;
  }

  // Always retain hemispheric ambient. IBL is optional enrichment: replacing
  // this fallback with an unavailable or black IBL makes the entire scene
  // black except for direct-light highlights.
  float3 skyColor = frameUBO.ambientSky.rgb * frameUBO.ambientSky.w;
  float3 groundColor = frameUBO.ambientGround.rgb;
  float NdotUp = dot(N, float3(0, 1, 0)) * 0.5 + 0.5;
  float3 ambientIBL = lerp(groundColor, skyColor, NdotUp) * albedo.rgb;

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

    ambientIBL += diffuseIBL + specularIBL;
  }

  float occlusion = 1.0;
  if (material.hasOcclusionMap != 0)
    occlusion = 1.0 - material.occlusionStrength * (1.0 - occlusionMap.Sample(samp, uv).r);

  float3 emissive = material.emissiveFactor.rgb;
  if (material.hasEmissiveMap != 0)
    emissive *= emissiveMap.Sample(samp, uv).rgb;

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
