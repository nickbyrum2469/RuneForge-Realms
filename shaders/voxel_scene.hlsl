struct PushData {
    float time;
    float aspect;
    float eyeX;
    float eyeY;
    float eyeZ;
    float yaw;
    float pitch;
    float viewportWidth;
    float viewportHeight;
    float selectedMaterial;
    float pad0;
    float pad1;
};

[[vk::push_constant]] ConstantBuffer<PushData> pushData;

struct VSInput {
    [[vk::location(0)]] float3 position : POSITION0;
    [[vk::location(1)]] float3 normal : NORMAL0;
    [[vk::location(2)]] uint material : TEXCOORD0;
};

struct VSOutput {
    float4 position : SV_Position;
    float3 color : COLOR0;
    float3 normal : TEXCOORD0;
    float depth : TEXCOORD1;
    float3 worldPosition : TEXCOORD2;
};

float hash31(float3 p) {
    p = frac(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return frac((p.x + p.y) * p.z);
}

float3 materialColor(uint material, float3 position) {
    float variation = hash31(floor(position) + float3(material * 7.0, material * 3.0, material * 11.0));
    if (material == 0) return lerp(float3(0.12, 0.36, 0.075), float3(0.30, 0.64, 0.16), variation);
    if (material == 1) return lerp(float3(0.24, 0.115, 0.040), float3(0.43, 0.245, 0.085), variation);
    if (material == 2) return lerp(float3(0.28, 0.30, 0.33), float3(0.53, 0.55, 0.57), variation);
    if (material == 3) return lerp(float3(0.25, 0.105, 0.030), float3(0.48, 0.245, 0.065), variation);
    return lerp(float3(0.055, 0.25, 0.065), float3(0.16, 0.48, 0.12), variation);
}

float3 rotateY(float3 p, float angle) {
    float c = cos(angle);
    float s = sin(angle);
    return float3(c * p.x + s * p.z, p.y, -s * p.x + c * p.z);
}

float3 rotateX(float3 p, float angle) {
    float c = cos(angle);
    float s = sin(angle);
    return float3(p.x, c * p.y - s * p.z, s * p.y + c * p.z);
}

VSOutput VSMain(VSInput input) {
    VSOutput output;
    const float3 eye = float3(pushData.eyeX, pushData.eyeY, pushData.eyeZ);
    const float3 delta = input.position - eye;
    const float3 view = rotateX(rotateY(delta, -pushData.yaw), pushData.pitch);

    const float nearPlane = 0.08;
    const float farPlane = 220.0;
    const float f = 1.9209821; // 55-degree vertical field of view.
    const float projectedZ = (farPlane / (farPlane - nearPlane)) * view.z -
                             (farPlane * nearPlane / (farPlane - nearPlane));

    output.position = float4(view.x * f / max(pushData.aspect, 0.01), -view.y * f, projectedZ, view.z);
    output.color = materialColor(input.material, input.position);
    output.normal = input.normal;
    output.depth = view.z;
    output.worldPosition = input.position;
    return output;
}

float4 PSMain(VSOutput input) : SV_Target0 {
    float3 n = normalize(input.normal);
    float3 sunDirection = normalize(float3(-0.52, 0.78, -0.34));
    float direct = max(dot(n, sunDirection), 0.0);
    float sky = saturate(n.y * 0.5 + 0.5);
    float lighting = 0.24 + direct * 0.72 + sky * 0.14;

    float3 lit = input.color * lighting;
    float heightWarmth = saturate((input.worldPosition.y - 1.0) / 12.0);
    lit += float3(0.035, 0.026, 0.010) * heightWarmth;

    // Deep blue distance haze creates readable terrain layering even before volumetric fog arrives.
    float fogAmount = saturate((input.depth - 34.0) / 75.0);
    float3 fogColor = float3(0.075, 0.125, 0.190);
    lit = lerp(lit, fogColor, fogAmount * 0.72);

    // Mild filmic shoulder + gamma keeps the development materials from looking flat and clipped.
    lit = lit / (1.0 + lit);
    lit = pow(max(lit, 0.0), 1.0 / 2.2);
    return float4(lit, 1.0);
}
