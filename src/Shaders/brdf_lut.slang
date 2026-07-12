static const float PI = 3.14159265;
static const uint SAMPLE_COUNT = 1024u;

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

float geometrySchlickGGX(float NdotV, float roughness) {
  float r = roughness + 1.0;
  float k = (r * r) / 8.0;
  return NdotV / (NdotV * (1.0 - k) + k);
}

float geometrySmith(float3 N, float3 V, float3 L, float roughness) {
  float NdotV = max(dot(N, V), 0.0);
  float NdotL = max(dot(N, L), 0.0);
  return geometrySchlickGGX(NdotV, roughness) *
         geometrySchlickGGX(NdotL, roughness);
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target {
  float NdotV = max(input.uv.x, 0.0001);
  float roughness = input.uv.y;

  float3 V = float3(sqrt(1.0 - NdotV * NdotV), 0.0, NdotV);
  float3 N = float3(0.0, 0.0, 1.0);

  float scale = 0.0;
  float bias = 0.0;

  for (uint i = 0u; i < SAMPLE_COUNT; ++i) {
    float2 Xi = hammersley(i, SAMPLE_COUNT);
    float3 H = importanceSampleGGX(Xi, N, roughness);
    float3 L = normalize(2.0 * dot(V, H) * H - V);

    float NdotL = max(L.z, 0.0);
    float NdotH = max(H.z, 0.0);
    float VdotH = max(dot(V, H), 0.0);

    if (NdotL > 0.0) {
      float G = geometrySmith(N, V, L, roughness);
      float Gv = (G * VdotH) / (NdotH * NdotV);
      float Fc = pow(1.0 - VdotH, 5.0);
      scale += (1.0 - Fc) * Gv;
      bias += Fc * Gv;
    }
  }

  scale /= float(SAMPLE_COUNT);
  bias /= float(SAMPLE_COUNT);

  return float4(scale, bias, 0.0, 1.0);
}
