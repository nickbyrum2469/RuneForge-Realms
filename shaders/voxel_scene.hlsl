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
    float3 normal : TEXCOORD0;
    float depth : TEXCOORD1;
    float3 worldPosition : TEXCOORD2;
    nointerpolation uint material : TEXCOORD3;
};

struct FullscreenOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float hash11(float p) {
    p = frac(p * 0.1031);
    p *= p + 33.33;
    p *= p + p;
    return frac(p);
}

float hash31(float3 p) {
    p = frac(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return frac((p.x + p.y) * p.z);
}

float hash21(float2 p) {
    float3 p3 = frac(float3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.x + p3.y) * p3.z);
}

float noise3(float3 p) {
    float3 i = floor(p);
    float3 f = frac(p);
    f = f * f * (3.0 - 2.0 * f);
    float n000 = hash31(i + float3(0, 0, 0));
    float n100 = hash31(i + float3(1, 0, 0));
    float n010 = hash31(i + float3(0, 1, 0));
    float n110 = hash31(i + float3(1, 1, 0));
    float n001 = hash31(i + float3(0, 0, 1));
    float n101 = hash31(i + float3(1, 0, 1));
    float n011 = hash31(i + float3(0, 1, 1));
    float n111 = hash31(i + float3(1, 1, 1));
    float x00 = lerp(n000, n100, f.x);
    float x10 = lerp(n010, n110, f.x);
    float x01 = lerp(n001, n101, f.x);
    float x11 = lerp(n011, n111, f.x);
    return lerp(lerp(x00, x10, f.y), lerp(x01, x11, f.y), f.z);
}

float fbm(float3 p) {
    float value = noise3(p) * 0.62;
    value += noise3(p * 2.03 + 17.1) * 0.27;
    value += noise3(p * 4.07 + 41.7) * 0.11;
    return value;
}

float2 surfaceUv(float3 p, float3 n) {
    float3 a = abs(n);
    if (a.y >= a.x && a.y >= a.z) return p.xz;
    if (a.x >= a.z) return p.zy;
    return p.xy;
}

float gridLine(float2 uv, float scale, float width) {
    float2 f = frac(uv * scale);
    float2 d = min(f, 1.0 - f);
    float nearest = min(d.x, d.y);
    return 1.0 - smoothstep(0.0, width, nearest);
}

float cellCrack(float2 uv, float scale) {
    float2 cell = floor(uv * scale);
    float2 f = frac(uv * scale) - 0.5;
    float jitter = hash21(cell) - 0.5;
    float diagonal = abs(f.x + f.y * (0.52 + jitter * 0.55));
    float seam = 1.0 - smoothstep(0.035, 0.095, diagonal);
    float border = gridLine(uv, scale, 0.045);
    return saturate(max(border * 0.72, seam * 0.62));
}

struct MaterialSample {
    float3 albedo;
    float roughness;
    float relief;
    float cavity;
};

MaterialSample sampleMaterial(uint material, float3 p, float3 baseNormal) {
    MaterialSample s;
    float2 uv = surfaceUv(p, baseNormal);
    float macroNoise = fbm(p * 0.42);
    float mediumNoise = noise3(p * 2.4);
    float fineNoise = noise3(p * 9.5);
    float blockEdge = gridLine(uv, 1.0, 0.045);

    if (material == 0) { // grass top: dense turf with small clump cells
        float patch = fbm(float3(p.x * 0.72, 2.1, p.z * 0.72));
        float micro = gridLine(uv, 11.0, 0.16) * 0.18;
        float3 dark = float3(0.055, 0.22, 0.035);
        float3 light = float3(0.34, 0.68, 0.12);
        s.albedo = lerp(dark, light, saturate(patch * 0.82 + fineNoise * 0.24));
        s.albedo *= 1.0 - micro * 0.20;
        s.roughness = 0.91;
        s.relief = patch * 0.55 + fineNoise * 0.20;
        s.cavity = saturate(micro * 0.28 + blockEdge * 0.16);
    } else if (material == 1) { // grass side: rich soil + turf fringe + roots
        float turf = smoothstep(0.70, 0.98, frac(p.y + 0.015));
        float root = pow(saturate(0.5 + 0.5 * sin(uv.x * 23.0 + fbm(p * 1.8) * 7.0)), 11.0);
        float3 soil = lerp(float3(0.15, 0.060, 0.020), float3(0.40, 0.22, 0.065), macroNoise);
        float3 green = lerp(float3(0.08, 0.29, 0.035), float3(0.31, 0.60, 0.10), mediumNoise);
        s.albedo = lerp(soil, green, turf * 0.88);
        s.albedo = lerp(s.albedo, float3(0.38, 0.25, 0.095), root * (1.0 - turf));
        s.roughness = 0.95;
        s.relief = mediumNoise * 0.42 + root * 0.18;
        s.cavity = saturate(root * 0.15 + blockEdge * 0.19);
    } else if (material == 2) { // dirt: pebbled soil and sparse roots
        float pebble = smoothstep(0.70, 0.92, fineNoise);
        float root = pow(saturate(0.5 + 0.5 * sin(uv.x * 17.0 + uv.y * 4.0 + macroNoise * 8.0)), 13.0);
        s.albedo = lerp(float3(0.14, 0.055, 0.018), float3(0.43, 0.245, 0.075), macroNoise);
        s.albedo = lerp(s.albedo, float3(0.31, 0.29, 0.22), pebble * 0.35);
        s.albedo = lerp(s.albedo, float3(0.46, 0.30, 0.12), root * 0.35);
        s.roughness = 0.98;
        s.relief = mediumNoise * 0.33 + pebble * 0.25;
        s.cavity = saturate(blockEdge * 0.15 + (1.0 - mediumNoise) * 0.08);
    } else if (material == 3) { // fractured stone: layered plates and deep seams
        float fracture = cellCrack(uv + macroNoise * 0.035, 3.15);
        float slab = hash21(floor(uv * 3.15));
        float strata = 0.5 + 0.5 * sin((p.y + macroNoise * 0.55) * 8.2);
        float3 cool = float3(0.19, 0.22, 0.25);
        float3 warm = float3(0.52, 0.50, 0.44);
        s.albedo = lerp(cool, warm, saturate(macroNoise * 0.55 + slab * 0.35 + strata * 0.10));
        s.albedo *= 1.0 - fracture * 0.58;
        s.roughness = 0.84 + fracture * 0.08;
        s.relief = slab * 0.58 + mediumNoise * 0.20 - fracture * 0.45;
        s.cavity = saturate(fracture * 0.78 + blockEdge * 0.12);
    } else if (material == 4) { // bark: chunky vertical plates
        float groove = 0.5 + 0.5 * sin((uv.x + macroNoise * 0.11) * 28.0);
        groove = pow(groove, 3.2);
        float fissure = cellCrack(float2(uv.x * 0.55, uv.y), 2.4) * 0.55;
        s.albedo = lerp(float3(0.13, 0.045, 0.012), float3(0.47, 0.235, 0.055), groove * 0.58 + macroNoise * 0.42);
        s.albedo *= 1.0 - fissure * 0.46;
        s.roughness = 0.90;
        s.relief = groove * 0.52 - fissure * 0.34 + fineNoise * 0.08;
        s.cavity = saturate(fissure * 0.55 + blockEdge * 0.10);
    } else if (material == 5) { // cut wood: end grain rings
        float2 centered = frac(uv) - 0.5;
        float radius = length(centered + (macroNoise - 0.5) * 0.04);
        float rings = 0.5 + 0.5 * sin(radius * 83.0 + macroNoise * 8.0);
        float split = cellCrack(uv, 1.2) * 0.45;
        s.albedo = lerp(float3(0.26, 0.10, 0.025), float3(0.67, 0.39, 0.11), rings * 0.45 + macroNoise * 0.55);
        s.albedo *= 1.0 - split * 0.35;
        s.roughness = 0.86;
        s.relief = rings * 0.25 - split * 0.22;
        s.cavity = saturate(split * 0.50 + blockEdge * 0.08);
    } else { // leaves: chunky clumps rather than flat green
        float clump = fbm(p * 1.55);
        float holes = smoothstep(0.66, 0.91, fineNoise);
        float veins = 0.5 + 0.5 * sin((uv.x + uv.y) * 31.0 + macroNoise * 8.0);
        s.albedo = lerp(float3(0.025, 0.16, 0.035), float3(0.19, 0.51, 0.09), clump);
        s.albedo *= 1.0 - holes * 0.34;
        s.albedo += float3(0.05, 0.08, 0.01) * veins * 0.12;
        s.roughness = 0.83;
        s.relief = clump * 0.48 + fineNoise * 0.18;
        s.cavity = saturate(holes * 0.30 + blockEdge * 0.08);
    }
    return s;
}

float materialHeight(uint material, float3 p, float3 n) {
    MaterialSample s = sampleMaterial(material, p, n);
    return s.relief;
}

float3 detailNormal(uint material, float3 p, float3 n) {
    float3 tangent = abs(n.y) < 0.92 ? normalize(cross(float3(0, 1, 0), n)) : float3(1, 0, 0);
    float3 bitangent = normalize(cross(n, tangent));
    float epsilon = 0.035;
    float h = materialHeight(material, p, n);
    float ht = materialHeight(material, p + tangent * epsilon, n);
    float hb = materialHeight(material, p + bitangent * epsilon, n);
    float strength = material == 3 ? 0.72 : (material == 0 ? 0.46 : 0.55);
    return normalize(n - tangent * ((ht - h) / epsilon) * strength -
                     bitangent * ((hb - h) / epsilon) * strength);
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

float3 cameraRay(float2 uv) {
    float2 ndc = uv * 2.0 - 1.0;
    ndc.y = -ndc.y;
    const float f = 1.9209821;
    float3 viewRay = normalize(float3(ndc.x * pushData.aspect / f, -ndc.y / f, 1.0));
    viewRay = rotateX(viewRay, -pushData.pitch);
    viewRay = rotateY(viewRay, pushData.yaw);
    return normalize(viewRay);
}

float3 acesTone(float3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

VSOutput VSMain(VSInput input) {
    VSOutput output;
    const float3 eye = float3(pushData.eyeX, pushData.eyeY, pushData.eyeZ);
    const float3 delta = input.position - eye;
    const float3 view = rotateX(rotateY(delta, -pushData.yaw), pushData.pitch);

    const float nearPlane = 0.08;
    const float farPlane = 320.0;
    const float f = 1.9209821;
    const float projectedZ = (farPlane / (farPlane - nearPlane)) * view.z -
                             (farPlane * nearPlane / (farPlane - nearPlane));

    output.position = float4(view.x * f / max(pushData.aspect, 0.01), -view.y * f, projectedZ, view.z);
    output.normal = input.normal;
    output.depth = view.z;
    output.worldPosition = input.position;
    output.material = input.material;
    return output;
}

FullscreenOutput VSFullscreen(uint vertexId : SV_VertexID) {
    FullscreenOutput output;
    float2 position = vertexId == 0 ? float2(-1.0, -1.0) :
                      (vertexId == 1 ? float2(-1.0, 3.0) : float2(3.0, -1.0));
    output.position = float4(position, 0.99999, 1.0);
    output.uv = float2(position.x * 0.5 + 0.5, 1.0 - (position.y * 0.5 + 0.5));
    return output;
}

float4 PSMain(VSOutput input) : SV_Target0 {
    float3 baseNormal = normalize(input.normal);
    MaterialSample material = sampleMaterial(input.material, input.worldPosition, baseNormal);
    float3 n = detailNormal(input.material, input.worldPosition, baseNormal);
    float3 eye = float3(pushData.eyeX, pushData.eyeY, pushData.eyeZ);
    float3 viewDir = normalize(eye - input.worldPosition);
    float3 sunDirection = normalize(float3(-0.47, 0.80, -0.37));
    float3 halfVector = normalize(sunDirection + viewDir);

    float ndl = saturate(dot(n, sunDirection));
    float ndv = saturate(dot(n, viewDir));
    float ndh = saturate(dot(n, halfVector));
    float skyHemisphere = saturate(n.y * 0.5 + 0.5);
    float ambient = lerp(0.18, 0.36, skyHemisphere);
    float wrap = saturate((dot(n, sunDirection) + 0.24) / 1.24);

    float specPower = lerp(9.0, 68.0, saturate(1.0 - material.roughness));
    float specular = pow(ndh, specPower) * lerp(0.025, 0.13, 1.0 - material.roughness);
    float fresnel = pow(1.0 - ndv, 5.0) * 0.055;
    float cavityLight = 1.0 - material.cavity * 0.55;

    float3 sunColor = float3(1.18, 0.97, 0.69);
    float3 skyColor = float3(0.31, 0.46, 0.68);
    float3 groundBounce = float3(0.16, 0.11, 0.055);
    float3 lit = material.albedo * (ambient * skyColor + wrap * sunColor * 0.92 + groundBounce * 0.18);
    lit *= cavityLight;
    lit += sunColor * (specular + fresnel * ndl);

    // Top-facing micro geometry catches warm light like the reference voxel renders.
    lit += float3(0.05, 0.033, 0.012) * saturate(n.y) * saturate(material.relief) * 0.35;

    float distanceFog = saturate((input.depth - 42.0) / 130.0);
    float heightFog = saturate((7.0 - input.worldPosition.y) / 18.0) * saturate((input.depth - 24.0) / 90.0);
    float fogAmount = saturate(distanceFog * 0.78 + heightFog * 0.24);
    float3 fogColor = lerp(float3(0.075, 0.12, 0.18), float3(0.23, 0.31, 0.43), saturate(n.y * 0.5 + 0.5));
    lit = lerp(lit, fogColor, fogAmount);

    lit = acesTone(lit * 1.28);
    lit = pow(max(lit, 0.0), 1.0 / 2.2);
    return float4(lit, 1.0);
}

float4 PSSky(FullscreenOutput input) : SV_Target0 {
    float2 uv = input.uv;
    float3 ray = cameraRay(uv);
    float horizon = saturate(ray.y * 0.5 + 0.5);
    float3 lowSky = float3(0.055, 0.095, 0.16);
    float3 midSky = float3(0.18, 0.30, 0.47);
    float3 highSky = float3(0.055, 0.11, 0.24);
    float3 sky = lerp(lowSky, midSky, smoothstep(0.30, 0.58, horizon));
    sky = lerp(sky, highSky, smoothstep(0.60, 0.96, horizon));

    float3 sunDirection = normalize(float3(-0.47, 0.80, -0.37));
    float sunDot = saturate(dot(ray, sunDirection));
    float sun = pow(sunDot, 720.0);
    float halo = pow(sunDot, 26.0);
    sky += float3(1.6, 0.95, 0.40) * sun * 1.9;
    sky += float3(0.65, 0.34, 0.12) * halo * 0.32;

    float cloudPlane = max(ray.y + 0.18, 0.05);
    float2 cloudUv = ray.xz / cloudPlane * 0.75 + float2(pushData.time * 0.006, 0.0);
    float cloudNoise = fbm(float3(cloudUv.x, 3.4, cloudUv.y));
    float clouds = smoothstep(0.60, 0.78, cloudNoise) * smoothstep(-0.08, 0.20, ray.y);
    sky = lerp(sky, float3(0.67, 0.72, 0.78), clouds * 0.34);

    float horizonGlow = exp(-abs(ray.y + 0.03) * 9.0);
    sky += float3(0.18, 0.10, 0.045) * horizonGlow;
    sky = acesTone(sky * 1.16);
    sky = pow(max(sky, 0.0), 1.0 / 2.2);
    return float4(sky, 1.0);
}

float3 blockTint(int block) {
    if (block == 1) return float3(0.27, 0.64, 0.10);
    if (block == 2) return float3(0.39, 0.20, 0.055);
    if (block == 3) return float3(0.47, 0.48, 0.48);
    if (block == 4) return float3(0.47, 0.23, 0.055);
    if (block == 5) return float3(0.10, 0.40, 0.08);
    return float3(0.20, 0.28, 0.38);
}

float roundedBoxMask(float2 p, float2 center, float2 halfSize, float radius) {
    float2 q = abs(p - center) - halfSize + radius;
    float distance = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
    return 1.0 - smoothstep(-1.2, 1.2, distance);
}

float4 PSHud(FullscreenOutput input) : SV_Target0 {
    float2 pixel = input.uv * float2(pushData.viewportWidth, pushData.viewportHeight);
    float2 center = float2(pushData.viewportWidth, pushData.viewportHeight) * 0.5;
    float alpha = 0.0;
    float3 color = 0.0;

    // Fine ivory crosshair with a dark under-stroke.
    float horizontal = (abs(pixel.y - center.y) < 1.35 && abs(pixel.x - center.x) < 10.0) ? 1.0 : 0.0;
    float vertical = (abs(pixel.x - center.x) < 1.35 && abs(pixel.y - center.y) < 10.0) ? 1.0 : 0.0;
    float cross = max(horizontal, vertical);
    if (cross > 0.0) {
        color = float3(0.95, 0.88, 0.68);
        alpha = 0.88;
    }

    // Nine-slot dark forged hotbar. Five starter block slots are tinted from the real block palette.
    float slot = min(pushData.viewportWidth / 16.0, 58.0);
    float gap = 5.0;
    float total = slot * 9.0 + gap * 8.0;
    float startX = center.x - total * 0.5 + slot * 0.5;
    float slotY = pushData.viewportHeight - slot * 0.72;
    int selected = clamp((int)round(pushData.selectedMaterial), 1, 5) - 1;

    [unroll]
    for (int i = 0; i < 9; ++i) {
        float2 slotCenter = float2(startX + i * (slot + gap), slotY);
        float outer = roundedBoxMask(pixel, slotCenter, float2(slot * 0.50, slot * 0.50), 7.0);
        float inner = roundedBoxMask(pixel, slotCenter, float2(slot * 0.41, slot * 0.41), 5.0);
        float border = saturate(outer - inner);
        if (outer > 0.0) {
            float3 frame = i == selected ? float3(0.92, 0.65, 0.22) : float3(0.19, 0.23, 0.29);
            float3 panel = float3(0.025, 0.035, 0.05);
            float3 localColor = lerp(panel, frame, border);
            if (i < 5 && inner > 0.0) {
                float item = roundedBoxMask(pixel, slotCenter, float2(slot * 0.23, slot * 0.23), 3.0);
                localColor = lerp(localColor, blockTint(i + 1), item * 0.92);
            }
            color = lerp(color, localColor, outer);
            alpha = max(alpha, outer * 0.91);
        }
    }

    return float4(color, alpha);
}
