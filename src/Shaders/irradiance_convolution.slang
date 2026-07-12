static const float PI = 3.14159265;

[[vk::binding(0, 0)]]
TextureCube<float4> environmentMap;
[[vk::binding(1, 0)]]
SamplerState envMapSampler;

[[vk::push_constant]]
cbuffer PushConstants {
  float4x4 inverseFaceVP;
  float4 iblParams; // x unused, y unused
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

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target {
  float3 N = normalize(input.localPos);

  float3 up = abs(N.y) < 0.999 ? float3(0, 1, 0) : float3(1, 0, 0);
  float3 right = normalize(cross(up, N));
  up = cross(N, right);

  float3 irradiance = float3(0, 0, 0);
  float sampleDelta = 0.025;
  float nrSamples = 0.0;

  for (float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta) {
    for (float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta) {
      float3 tangentSample = float3(sin(theta) * cos(phi),
                                    sin(theta) * sin(phi), cos(theta));
      float3 sampleVec =
          tangentSample.x * right + tangentSample.y * up + tangentSample.z * N;
      irradiance +=
          environmentMap.Sample(envMapSampler, sampleVec).rgb *
          cos(theta) * sin(theta);
      nrSamples++;
    }
  }

  irradiance = PI * irradiance * (1.0 / nrSamples);
  return float4(irradiance, 1.0);
}
