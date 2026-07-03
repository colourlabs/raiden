struct FrameUniforms {
  float4x4 projection;
  float4x4 view;
  float4x4 model;
  float4 extra;
  float4 cameraPos;
  float4 lightDir;
  float4 lightColor;
  float4 ambientSky;
  float4 ambientGround;
};

[[vk::binding(0, 0)]]
ConstantBuffer<FrameUniforms> frameUBO;

[[vk::binding(0, 1)]]
SamplerState skySamp;

struct VertexInput {
  [[vk::location(0)]] float3 pos : POSITION;
};

struct VertexOutput {
  float4 position : SV_Position;
  [[vk::location(0)]] float3 uv : TEXCOORD;
};

[[vk::push_constant]]
cbuffer PushConstants {
  float4x4 model;
};

[shader("vertex")]
VertexOutput vertexMain(VertexInput input) {
  VertexOutput output;
  float4x4 viewNoTrans = frameUBO.view;
  viewNoTrans[3] = float4(0, 0, 0, 1);
  float3 worldPos = input.pos + frameUBO.cameraPos.xyz;
  output.position = mul(frameUBO.projection, mul(viewNoTrans, float4(worldPos, 1.0)));
  output.position.z = output.position.w;
  output.uv = input.pos;
  return output;
}

[[vk::binding(0, 2)]]
TextureCube<float4> skyboxMap;

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target {
  return skyboxMap.Sample(skySamp, normalize(input.uv));
}
