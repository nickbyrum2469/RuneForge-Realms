struct PushData {
    float time;
    float aspect;
    float yaw;
    float pitch;
    float distance;
    float pad0;
    float pad1;
    float pad2;
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
    if (material == 0) return lerp(float3(0.16, 0.42, 0.11), float3(0.28, 0.62, 0.17), variation);
    if (material == 1) return lerp(float3(0.26, 0.13, 0.050), float3(0.39, 0.22, 0.085), variation);
    if (material == 2) return lerp(float3(0.30, 0.32, 0.35), float3(0.48, 0.50, 0.53), variation);
    if (material == 3) return lerp(float3(0.30, 0.14, 0.045), float3(0.48, 0.25, 0.075), variation);
    return lerp(float3(0.075, 0.30, 0.09), float3(0.14, 0.48, 0.14), variation);
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

    const float3 target = float3(8.0, 5.0, 8.0);
    float3 centered = input.position - target;
    float3 view = rotateX(rotateY(centered, pushData.yaw), pushData.pitch);
    view.z += pushData.distance;

    float3 viewNormal = normalize(rotateX(rotateY(input.normal, pushData.yaw), pushData.pitch));

    const float nearPlane = 0.10;
    const float farPlane = 160.0;
    const float f = 1.7320508; // cot(60deg / 2)
    float projectedZ = (farPlane / (farPlane - nearPlane)) * view.z -
                       (farPlane * nearPlane / (farPlane - nearPlane));

    output.position = float4(view.x * f / max(pushData.aspect, 0.01), -view.y * f, projectedZ, view.z);
    output.color = materialColor(input.material, input.position);
    output.normal = viewNormal;
    output.depth = view.z;
    output.worldPosition = input.position;
    return output;
}

float4 PSMain(VSOutput input) : SV_Target0 {
    float3 n = normalize(input.normal);
    float3 sunDirection = normalize(float3(-0.45, 0.82, -0.35));
    float ndotl = max(dot(n, sunDirection), 0.0);
    float hemi = saturate(n.y * 0.5 + 0.5);
    float lighting = 0.18 + ndotl * 0.74 + hemi * 0.17;

    float3 lit = input.color * lighting;

    // Slightly cool atmosphere catches silhouettes without flattening the voxel faces.
    float rim = pow(1.0 - saturate(abs(n.z)), 3.0) * 0.08;
    lit += float3(0.16, 0.28, 0.48) * rim;

    // Contact-ish darkening toward the lowest terrain layers gives the chunk more readable depth.
    float groundShade = saturate((input.worldPosition.y + 1.0) / 6.0);
    lit *= lerp(0.72, 1.0, groundShade);

    float fogAmount = saturate((input.depth - 23.0) / 32.0);
    float3 fogColor = float3(0.030, 0.055, 0.105);
    lit = lerp(lit, fogColor, fogAmount * 0.48);

    lit = lit / (1.0 + lit);
    lit = pow(lit, 1.0 / 2.2);
    return float4(lit, 1.0);
}
