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

struct VSOutput {
    float4 position : SV_Position;
    float3 color : COLOR0;
    float3 normal : TEXCOORD0;
    float depth : TEXCOORD1;
};

static const float3 kCubePositions[36] = {
    // Front (+Z)
    float3(-0.5, -0.5,  0.5), float3( 0.5, -0.5,  0.5), float3( 0.5,  0.5,  0.5),
    float3(-0.5, -0.5,  0.5), float3( 0.5,  0.5,  0.5), float3(-0.5,  0.5,  0.5),
    // Back (-Z)
    float3( 0.5, -0.5, -0.5), float3(-0.5, -0.5, -0.5), float3(-0.5,  0.5, -0.5),
    float3( 0.5, -0.5, -0.5), float3(-0.5,  0.5, -0.5), float3( 0.5,  0.5, -0.5),
    // Right (+X)
    float3( 0.5, -0.5,  0.5), float3( 0.5, -0.5, -0.5), float3( 0.5,  0.5, -0.5),
    float3( 0.5, -0.5,  0.5), float3( 0.5,  0.5, -0.5), float3( 0.5,  0.5,  0.5),
    // Left (-X)
    float3(-0.5, -0.5, -0.5), float3(-0.5, -0.5,  0.5), float3(-0.5,  0.5,  0.5),
    float3(-0.5, -0.5, -0.5), float3(-0.5,  0.5,  0.5), float3(-0.5,  0.5, -0.5),
    // Top (+Y)
    float3(-0.5,  0.5,  0.5), float3( 0.5,  0.5,  0.5), float3( 0.5,  0.5, -0.5),
    float3(-0.5,  0.5,  0.5), float3( 0.5,  0.5, -0.5), float3(-0.5,  0.5, -0.5),
    // Bottom (-Y)
    float3(-0.5, -0.5, -0.5), float3( 0.5, -0.5, -0.5), float3( 0.5, -0.5,  0.5),
    float3(-0.5, -0.5, -0.5), float3( 0.5, -0.5,  0.5), float3(-0.5, -0.5,  0.5)
};

static const float3 kCubeNormals[36] = {
    float3(0,0,1), float3(0,0,1), float3(0,0,1), float3(0,0,1), float3(0,0,1), float3(0,0,1),
    float3(0,0,-1), float3(0,0,-1), float3(0,0,-1), float3(0,0,-1), float3(0,0,-1), float3(0,0,-1),
    float3(1,0,0), float3(1,0,0), float3(1,0,0), float3(1,0,0), float3(1,0,0), float3(1,0,0),
    float3(-1,0,0), float3(-1,0,0), float3(-1,0,0), float3(-1,0,0), float3(-1,0,0), float3(-1,0,0),
    float3(0,1,0), float3(0,1,0), float3(0,1,0), float3(0,1,0), float3(0,1,0), float3(0,1,0),
    float3(0,-1,0), float3(0,-1,0), float3(0,-1,0), float3(0,-1,0), float3(0,-1,0), float3(0,-1,0)
};

float hash01(uint value) {
    value ^= value >> 16;
    value *= 0x7feb352d;
    value ^= value >> 15;
    value *= 0x846ca68b;
    value ^= value >> 16;
    return (value & 0x00ffffff) / 16777215.0;
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

void instanceData(uint instanceId, out float3 translation, out float scale, out float3 baseColor) {
    if (instanceId < 81) {
        uint gx = instanceId % 9;
        uint gz = instanceId / 9;
        translation = float3((float(gx) - 4.0) * 1.02, -0.60, (float(gz) - 4.0) * 1.02);
        scale = 1.0;
        float variation = hash01(instanceId) * 0.09;
        baseColor = float3(0.20 + variation, 0.47 + variation * 0.5, 0.17 + variation * 0.25);
        return;
    }

    if (instanceId < 85) {
        uint layer = instanceId - 81;
        translation = float3(0.0, 0.15 + float(layer) * 0.68, 0.0);
        scale = 0.62;
        float variation = hash01(instanceId) * 0.08;
        baseColor = float3(0.34 + variation, 0.19 + variation * 0.4, 0.08);
        return;
    }

    uint leafId = instanceId - 85;
    uint lx = leafId % 5;
    uint lz = leafId / 5;
    translation = float3((float(lx) - 2.0) * 0.62, 3.00 + 0.18 * sin(float(lx + lz)), (float(lz) - 2.0) * 0.62);
    scale = 0.72 + hash01(instanceId) * 0.16;
    float variation = hash01(instanceId * 13 + 9) * 0.12;
    baseColor = float3(0.10 + variation * 0.3, 0.42 + variation, 0.16 + variation * 0.35);
}

VSOutput VSMain(uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID) {
    VSOutput output;

    float3 translation;
    float scale;
    float3 baseColor;
    instanceData(instanceId, translation, scale, baseColor);

    float3 local = kCubePositions[vertexId] * scale;
    float3 world = local + translation;
    float3 normal = kCubeNormals[vertexId];

    // Ground side faces expose dirt while upward faces stay grassy.
    if (instanceId < 81 && normal.y < 0.5) {
        baseColor = lerp(float3(0.28, 0.16, 0.075), baseColor, 0.24);
    }

    // Orbit the small voxel diorama around a fixed camera target.
    float3 centered = world - float3(0.0, 1.0, 0.0);
    float3 view = rotateX(rotateY(centered, pushData.yaw), pushData.pitch);
    view.z += pushData.distance;

    float3 viewNormal = normalize(rotateX(rotateY(normal, pushData.yaw), pushData.pitch));

    const float nearPlane = 0.10;
    const float farPlane = 100.0;
    const float f = 1.7320508; // cot(60deg / 2)
    float projectedZ = (farPlane / (farPlane - nearPlane)) * view.z -
                       (farPlane * nearPlane / (farPlane - nearPlane));

    output.position = float4(view.x * f / max(pushData.aspect, 0.01), -view.y * f, projectedZ, view.z);
    output.color = baseColor;
    output.normal = viewNormal;
    output.depth = view.z;
    return output;
}

float4 PSMain(VSOutput input) : SV_Target0 {
    float3 n = normalize(input.normal);
    float3 sunDirection = normalize(float3(-0.45, 0.82, -0.35));
    float ndotl = max(dot(n, sunDirection), 0.0);
    float hemi = saturate(n.y * 0.5 + 0.5);
    float lighting = 0.20 + ndotl * 0.72 + hemi * 0.16;

    float3 lit = input.color * lighting;
    float rim = pow(1.0 - saturate(abs(n.z)), 3.0) * 0.10;
    lit += float3(0.18, 0.32, 0.55) * rim;

    float fogAmount = saturate((input.depth - 10.0) / 17.0);
    float3 fogColor = float3(0.035, 0.060, 0.115);
    lit = lerp(lit, fogColor, fogAmount * 0.55);

    // Simple filmic-ish shoulder for this renderer proof.
    lit = lit / (1.0 + lit);
    lit = pow(lit, 1.0 / 2.2);
    return float4(lit, 1.0);
}
