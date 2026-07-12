static const float PI = 3.14159265;
static const uint SAMPLE_COUNT = 1024u;

[[vk::binding(0, 0)]]
TextureCube<float4> environmentMap;
[[vk::binding(1, 0)]]
SamplerState envMapSampler;

[[vk::push_constant]]
cbuffer PushConstants {
  float4x4 inverseFaceVP;
  float4 iblParams; // x=roughness
};

struct VertexOutput {
  float4 position : SV_Position;
  float3 localPos : TEXCOORD0;
};

[shader("vertex")]
VertexOutput vertexMain(uint vertexID : SV_VertexID) {
  VertexOutput output;
  float2 uv = float2((vertexID << 1) & 2, vertexID & 2);
  output.position = float4(uv * 2.0 - 1.0, 0.0, 1.0);
  float4 worldPos = mul(inverseFaceVP, float4(uv * 2.0 - 1.0, 0.0, 1.0));
  output.localPos = worldPos.xyz / worldPos.w;
  return output;
}

float2 hammersley(uint i, uint N) {
  uint bits = i;
  bits = (bits << 16u) | (bits >> 16u);
  bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
  bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
  bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
  bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
  float rdi = float(bits) * 2.3283064365386963e-10;
  return float2(float(i) / float(N), rdi);
}

float3 importanceSampleGGX(float2 Xi, float3 N, float roughness) {
  float a = roughness * roughness;
  float phi = 2.0 * PI * Xi.x;
  float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
  float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

  float3 H = float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);

  float3 up = abs(N.y) < 0.999 ? float3(0, 1, 0) : float3(1, 0, 0);
  float3 tangent = normalize(cross(up, N));
  float3 bitangent = cross(N, tangent);

  return normalize(tangent * H.x + bitangent * H.y + N * H.z);
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target {
  float roughness = iblParams.x;
  float3 N = normalize(input.localPos);
  float3 V = N;

  float totalWeight = 0.0;
  float3 prefilteredColor = float3(0, 0, 0);

  for (uint i = 0u; i < SAMPLE_COUNT; ++i) {
    float2 Xi = hammersley(i, SAMPLE_COUNT);
    float3 H = importanceSampleGGX(Xi, N, roughness);
    float3 L = normalize(2.0 * dot(V, H) * H - V);

    float NdotL = max(dot(N, L), 0.0);
    if (NdotL > 0.0) {
      prefilteredColor += environmentMap.Sample(envMapSampler, L).rgb * NdotL;
      totalWeight += NdotL;
    }
  }

  prefilteredColor /= max(totalWeight, 0.001);
  return float4(prefilteredColor, 1.0);
}
