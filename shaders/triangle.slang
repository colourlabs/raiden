#include "include/common.slangh"

// set 1, sampler shared across all texture slots
[[vk::binding(0, 1)]]
SamplerState samp;

struct VertexInput
{
  float3 pos : POSITION;
  float3 normal : NORMAL;
  float3 color : COLOR;
  float2 uv : TEXCOORD;
};

struct VertexOutput
{
  float4 position : SV_Position;
  float3 normal : NORMAL;
  float3 color : COLOR;
  float2 uv : TEXCOORD;
  float3 worldPos : TEXCOORD1;
};

[shader("vertex")]
VertexOutput vertexMain(VertexInput input)
{
  VertexOutput output;

  float4 worldPos = mul(frameUBO.model, float4(input.pos, 1.0));
  output.worldPos = worldPos.xyz;
  output.position = mul(frameUBO.projection, mul(frameUBO.view, worldPos));

  // transform normal into world space
  // using model matrix, good enough for right now
  output.normal = normalize(mul((float3x3)frameUBO.model, input.normal));

  output.color = input.color;

  // apply UV transform from material
  float s = sin(material.uvRotation);
  float c = cos(material.uvRotation);
  float2x2 rot = float2x2(c, -s, s, c);
  output.uv = mul(rot, input.uv * material.uvScale + material.uvOffset);

  return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
  float2 uv = input.uv;

  // sample albedo
  float4 albedo = albedoMap.Sample(samp, uv) * material.baseColorFactor;

  // vertex color blending
  if (material.vertexColorMode == 1)      // Multiply
    albedo.rgb *= input.color;
  else if (material.vertexColorMode == 2) // Add
    albedo.rgb += input.color;

  // alpha
  if (material.alphaMode == 1)
  { 
    // mask
    if (albedo.a < material.alphaCutoff)
      discard;
  }

  // occlusion
  float ao = 1.0;
  if (material.hasOcclusionMap != 0)
    ao = occlusionMap.Sample(samp, uv).r * material.occlusionStrength;

  // emissive
  float3 emissive = material.emissiveFactor.rgb;
  if (material.hasEmissiveMap != 0)
    emissive *= emissiveMap.Sample(samp, uv).rgb;

  // basic lighting, directional light, no PBR yet
  // normal is already in world space from vertex shader
  float3 N = normalize(input.normal);
  float3 L = normalize(float3(0.5, 1.0, 0.5)); // hardcoded sun direction for now
  float NdotL = max(dot(N, L), 0.0);
  float3 ambient = albedo.rgb * 0.15 * ao;
  float3 diffuse = albedo.rgb * NdotL * ao;

  float3 finalColor = ambient + diffuse + emissive;

  return float4(finalColor, albedo.a);
}