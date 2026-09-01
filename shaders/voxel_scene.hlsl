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
    float miningMode;
    float miningProgress;
    float targetBlockX;
    float targetBlockY;
    float targetBlockZ;
    float targetActive;
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

float hash21(float2 p) {
    float3 p3 = frac(float3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.x + p3.y) * p3.z);
}

float hash31(float3 p) {
    p = frac(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return frac((p.x + p.y) * p.z);
}

float noise2(float2 p) {
    float2 i = floor(p);
    float2 f = frac(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash21(i);
    float b = hash21(i + float2(1, 0));
    float c = hash21(i + float2(0, 1));
    float d = hash21(i + float2(1, 1));
    return lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y);
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

float fbm3(float3 p) {
    float value = noise3(p) * 0.56;
    value += noise3(p * 2.03 + 17.1) * 0.28;
    value += noise3(p * 4.07 + 41.7) * 0.11;
    value += noise3(p * 8.11 + 71.3) * 0.05;
    return value;
}

float fbm2(float2 p) {
    float value = noise2(p) * 0.58;
    value += noise2(p * 2.07 + 11.7) * 0.27;
    value += noise2(p * 4.13 + 29.2) * 0.15;
    return value;
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

float2 surfaceUv(float3 p, float3 n) {
    float3 a = abs(n);
    if (a.y >= a.x && a.y >= a.z) return p.xz;
    if (a.x >= a.z) return p.zy;
    return p.xy;
}

float gridLine(float2 uv, float scale, float width) {
    float2 f = frac(uv * scale);
    float2 d = min(f, 1.0 - f);
    return 1.0 - smoothstep(0.0, width, min(d.x, d.y));
}

float plateCrack(float2 uv, float scale) {
    float2 cell = floor(uv * scale);
    float2 f = frac(uv * scale) - 0.5;
    float jitter = hash21(cell) - 0.5;
    float diagonalA = abs(f.x + f.y * (0.48 + jitter * 0.64));
    float diagonalB = abs(f.x * (0.42 - jitter * 0.30) - f.y);
    float seamA = 1.0 - smoothstep(0.025, 0.080, diagonalA);
    float seamB = 1.0 - smoothstep(0.020, 0.065, diagonalB);
    float border = gridLine(uv, scale, 0.038);
    return saturate(max(border * 0.72, max(seamA * 0.62, seamB * 0.38)));
}

struct MaterialSample {
    float3 albedo;
    float roughness;
    float relief;
    float cavity;
    float emissive;
};

MaterialSample sampleMaterial(uint material, float3 p, float3 baseNormal) {
    MaterialSample s;
    s.albedo = float3(0.5, 0.5, 0.5);
    s.roughness = 0.9;
    s.relief = 0.0;
    s.cavity = 0.0;
    s.emissive = 0.0;

    float2 uv = surfaceUv(p, baseNormal);
    float macroNoise = fbm3(p * 0.44);
    float mediumNoise = noise3(p * 2.6);
    float fineNoise = noise3(p * 10.7);

    // Every nearby surface visually acknowledges the same 8x8 micro-cell scale used by physical
    // fracture state. The seams are subtle on pristine blocks, so promotion never looks like an LOD pop.
    float microGrid = gridLine(uv, 8.0, 0.060);
    float microCell = hash21(floor(uv * 8.0));

    if (material == 0) { // Grass top
        float lush = fbm3(float3(p.x * 0.78, 3.1, p.z * 0.78));
        float3 dark = float3(0.045, 0.18, 0.025);
        float3 light = float3(0.37, 0.71, 0.13);
        s.albedo = lerp(dark, light, saturate(lush * 0.78 + fineNoise * 0.22));
        s.albedo *= 1.0 - microGrid * 0.10;
        s.albedo *= 0.93 + microCell * 0.12;
        s.roughness = 0.91;
        s.relief = lush * 0.50 + fineNoise * 0.18 + microCell * 0.10;
        s.cavity = microGrid * 0.10;
    } else if (material == 1) { // Rooted grass side
        float turf = smoothstep(0.68, 0.98, frac(p.y + 0.012));
        float root = pow(saturate(0.5 + 0.5 * sin(uv.x * 24.0 + fbm3(p * 1.7) * 7.0)), 10.0);
        float3 soil = lerp(float3(0.13, 0.047, 0.013), float3(0.40, 0.22, 0.065), macroNoise);
        float3 green = lerp(float3(0.07, 0.25, 0.026), float3(0.31, 0.61, 0.10), mediumNoise);
        s.albedo = lerp(soil, green, turf * 0.90);
        s.albedo = lerp(s.albedo, float3(0.48, 0.31, 0.11), root * (1.0 - turf) * 0.55);
        s.albedo *= 1.0 - microGrid * 0.08;
        s.roughness = 0.96;
        s.relief = mediumNoise * 0.38 + root * 0.17;
        s.cavity = root * 0.13 + microGrid * 0.08;
    } else if (material == 2) { // Dirt
        float pebble = smoothstep(0.69, 0.92, fineNoise);
        float root = pow(saturate(0.5 + 0.5 * sin(uv.x * 18.0 + uv.y * 4.0 + macroNoise * 8.0)), 13.0);
        s.albedo = lerp(float3(0.13, 0.047, 0.014), float3(0.43, 0.245, 0.075), macroNoise);
        s.albedo = lerp(s.albedo, float3(0.34, 0.31, 0.24), pebble * 0.34);
        s.albedo = lerp(s.albedo, float3(0.47, 0.30, 0.11), root * 0.31);
        s.albedo *= 1.0 - microGrid * 0.09;
        s.roughness = 0.98;
        s.relief = mediumNoise * 0.32 + pebble * 0.24 + microCell * 0.06;
        s.cavity = microGrid * 0.10 + (1.0 - mediumNoise) * 0.06;
    } else if (material == 3) { // Fractured stone
        float fracture = plateCrack(uv + macroNoise * 0.025, 3.35);
        float smallFracture = plateCrack(uv + mediumNoise * 0.018, 8.0) * 0.32;
        float slab = hash21(floor(uv * 3.35));
        float strata = 0.5 + 0.5 * sin((p.y + macroNoise * 0.48) * 8.6);
        float3 cool = float3(0.17, 0.20, 0.23);
        float3 warm = float3(0.55, 0.52, 0.45);
        s.albedo = lerp(cool, warm, saturate(macroNoise * 0.51 + slab * 0.36 + strata * 0.13));
        s.albedo *= 1.0 - fracture * 0.60 - smallFracture * 0.20;
        s.albedo *= 1.0 - microGrid * 0.075;
        s.roughness = 0.84 + fracture * 0.09;
        s.relief = slab * 0.58 + mediumNoise * 0.19 + microCell * 0.10 - fracture * 0.48;
        s.cavity = saturate(fracture * 0.80 + smallFracture * 0.32 + microGrid * 0.07);
    } else if (material == 4) { // Bark
        float groove = 0.5 + 0.5 * sin((uv.x + macroNoise * 0.11) * 29.0);
        groove = pow(groove, 3.1);
        float fissure = plateCrack(float2(uv.x * 0.55, uv.y), 2.5) * 0.58;
        s.albedo = lerp(float3(0.12, 0.039, 0.010), float3(0.49, 0.25, 0.060), groove * 0.56 + macroNoise * 0.44);
        s.albedo *= 1.0 - fissure * 0.46 - microGrid * 0.06;
        s.roughness = 0.91;
        s.relief = groove * 0.52 - fissure * 0.35 + fineNoise * 0.08;
        s.cavity = fissure * 0.57 + microGrid * 0.06;
    } else if (material == 5) { // Cut wood
        float2 centered = frac(uv) - 0.5;
        float radius = length(centered + (macroNoise - 0.5) * 0.04);
        float rings = 0.5 + 0.5 * sin(radius * 84.0 + macroNoise * 8.0);
        float split = plateCrack(uv, 1.25) * 0.45;
        s.albedo = lerp(float3(0.25, 0.085, 0.019), float3(0.69, 0.40, 0.12), rings * 0.45 + macroNoise * 0.55);
        s.albedo *= 1.0 - split * 0.35 - microGrid * 0.05;
        s.roughness = 0.86;
        s.relief = rings * 0.25 - split * 0.22 + microCell * 0.05;
        s.cavity = split * 0.50 + microGrid * 0.05;
    } else if (material == 6) { // Leaves
        float clump = fbm3(p * 1.62);
        float holes = smoothstep(0.67, 0.91, fineNoise);
        s.albedo = lerp(float3(0.020, 0.13, 0.027), float3(0.20, 0.53, 0.095), clump);
        s.albedo *= 1.0 - holes * 0.32 - microGrid * 0.05;
        s.roughness = 0.83;
        s.relief = clump * 0.46 + fineNoise * 0.17;
        s.cavity = holes * 0.28 + microGrid * 0.04;
    } else if (material == 7) { // White flower
        s.albedo = lerp(float3(0.68, 0.70, 0.70), float3(1.00, 0.96, 0.86), fineNoise);
        s.roughness = 0.72;
        s.relief = fineNoise * 0.14;
        s.emissive = 0.025;
    } else if (material == 8) { // Yellow flower
        s.albedo = lerp(float3(0.78, 0.39, 0.025), float3(1.00, 0.88, 0.16), fineNoise);
        s.roughness = 0.70;
        s.relief = fineNoise * 0.14;
        s.emissive = 0.035;
    } else { // Blue flower
        s.albedo = lerp(float3(0.12, 0.29, 0.69), float3(0.37, 0.72, 1.00), fineNoise);
        s.roughness = 0.68;
        s.relief = fineNoise * 0.14;
        s.emissive = 0.035;
    }
    return s;
}

float materialHeight(uint material, float3 p, float3 n) {
    return sampleMaterial(material, p, n).relief;
}

float3 detailNormal(uint material, float3 p, float3 n) {
    float3 tangent = abs(n.y) < 0.92 ? normalize(cross(float3(0, 1, 0), n)) : float3(1, 0, 0);
    float3 bitangent = normalize(cross(n, tangent));
    float epsilon = 0.028;
    float h = materialHeight(material, p, n);
    float ht = materialHeight(material, p + tangent * epsilon, n);
    float hb = materialHeight(material, p + bitangent * epsilon, n);
    float strength = material == 3 ? 0.82 : (material <= 2 ? 0.55 : 0.62);
    return normalize(n - tangent * ((ht - h) / epsilon) * strength -
                     bitangent * ((hb - h) / epsilon) * strength);
}

float structuralDamageCrack(float3 worldPosition, float3 geometricNormal) {
    if (pushData.targetActive < 0.5 || pushData.miningProgress <= 0.001) return 0.0;
    // Micro mode is represented by true missing geometry. Surface cracks are reserved for
    // structural Block mining and the structural component of Mixed mining.
    if (pushData.miningMode > 0.5 && pushData.miningMode < 1.5) return 0.0;

    const float3 targetBlock = float3(pushData.targetBlockX, pushData.targetBlockY, pushData.targetBlockZ);
    const float3 interiorPoint = worldPosition - geometricNormal * 0.0025;
    const float3 fragmentBlock = floor(interiorPoint);
    const float3 targetDelta = abs(fragmentBlock - targetBlock);
    if (max(max(targetDelta.x, targetDelta.y), targetDelta.z) > 0.25) return 0.0;

    const float progress = saturate(pushData.miningProgress);
    const float seed = hash31(targetBlock * 0.173 + 7.91);
    const float2 uv = surfaceUv(worldPosition, geometricNormal);
    float crack = plateCrack(uv + seed * 0.071, 1.25) * smoothstep(0.06, 0.24, progress);
    crack = max(crack, plateCrack(uv + seed * 0.113 + 0.21, 2.65) * smoothstep(0.28, 0.52, progress));
    crack = max(crack, plateCrack(uv - seed * 0.083 + 0.47, 5.20) * smoothstep(0.58, 0.88, progress));
    return saturate(crack * (0.58 + progress * 0.42));
}

float3 acesTone(float3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float3 cameraRay(float2 uv) {
    float2 ndc = uv * 2.0 - 1.0;
    ndc.y = -ndc.y;
    const float f = 1.9209821;
    float3 ray = normalize(float3(ndc.x * pushData.aspect / f, -ndc.y / f, 1.0));
    ray = rotateX(ray, -pushData.pitch);
    ray = rotateY(ray, pushData.yaw);
    return normalize(ray);
}

VSOutput VSMain(VSInput input) {
    VSOutput output;
    const float3 eye = float3(pushData.eyeX, pushData.eyeY, pushData.eyeZ);
    const float3 delta = input.position - eye;
    const float3 view = rotateX(rotateY(delta, -pushData.yaw), pushData.pitch);

    const float nearPlane = 0.08;
    const float farPlane = 360.0;
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
    output.uv = position * 0.5 + 0.5;
    return output;
}

float4 PSSky(FullscreenOutput input) : SV_Target0 {
    float2 uv = input.uv;
    float3 ray = cameraRay(uv);
    float horizon = saturate(ray.y * 0.5 + 0.5);

    float3 horizonColor = float3(0.48, 0.43, 0.39);
    float3 midColor = float3(0.18, 0.32, 0.51);
    float3 zenithColor = float3(0.035, 0.085, 0.18);
    float3 sky = lerp(horizonColor, midColor, smoothstep(0.46, 0.64, horizon));
    sky = lerp(sky, zenithColor, smoothstep(0.62, 0.96, horizon));

    float3 sunDirection = normalize(float3(-0.52, 0.63, -0.44));
    float sunDot = dot(ray, sunDirection);
    float sun = smoothstep(0.996, 0.9994, sunDot);
    float halo = pow(saturate(sunDot), 36.0);
    sky += float3(1.00, 0.72, 0.34) * sun * 5.0;
    sky += float3(1.00, 0.55, 0.24) * halo * 0.55;

    if (ray.y > 0.015) {
        float2 cloudUv = ray.xz / max(ray.y, 0.04) * 0.22;
        cloudUv += float2(pushData.time * 0.0034, pushData.time * 0.0011);
        float clouds = fbm2(cloudUv * 2.5);
        clouds = smoothstep(0.56, 0.75, clouds);
        float cloudFade = smoothstep(0.015, 0.12, ray.y) * (1.0 - smoothstep(0.70, 0.96, ray.y));
        float3 cloudColor = lerp(float3(0.42, 0.45, 0.49), float3(0.94, 0.89, 0.80), saturate(sunDot * 0.5 + 0.5));
        sky = lerp(sky, cloudColor, clouds * cloudFade * 0.58);
    }

    float lowerHaze = exp(-abs(ray.y) * 22.0);
    sky += float3(0.34, 0.29, 0.26) * lowerHaze * 0.22;
    sky = acesTone(sky);
    sky = pow(max(sky, 0.0), 1.0 / 2.2);
    return float4(sky, 1.0);
}

float4 PSMain(VSOutput input) : SV_Target0 {
    float3 geometricNormal = normalize(input.normal);
    MaterialSample material = sampleMaterial(input.material, input.worldPosition, geometricNormal);
    const float damageCrack = structuralDamageCrack(input.worldPosition, geometricNormal);
    if (damageCrack > 0.0) {
        const float progress = saturate(pushData.miningProgress);
        material.albedo *= 1.0 - damageCrack * (0.38 + progress * 0.34);
        material.cavity = saturate(material.cavity + damageCrack * (0.48 + progress * 0.38));
        material.roughness = saturate(material.roughness + damageCrack * 0.08);
    }
    float3 n = detailNormal(input.material, input.worldPosition, geometricNormal);

    float3 sunDirection = normalize(float3(-0.52, 0.63, -0.44));
    float direct = max(dot(n, sunDirection), 0.0);
    float skyLight = saturate(n.y * 0.52 + 0.48);
    float3 halfVector = normalize(sunDirection + normalize(float3(pushData.eyeX, pushData.eyeY, pushData.eyeZ) - input.worldPosition));
    float specular = pow(saturate(dot(n, halfVector)), lerp(13.0, 75.0, 1.0 - material.roughness));
    specular *= (1.0 - material.roughness) * 0.28;

    float cavity = saturate(material.cavity);
    float ambientOcclusion = 1.0 - cavity * 0.58;
    float contact = 0.92 + 0.08 * smoothstep(0.0, 2.0, input.worldPosition.y);

    float3 warmSun = float3(1.08, 0.89, 0.65);
    float3 coolSky = float3(0.37, 0.53, 0.77);
    float3 lighting = warmSun * (0.20 + direct * 1.10) + coolSky * (0.12 + skyLight * 0.24);
    float3 lit = material.albedo * lighting * ambientOcclusion * contact;
    lit += warmSun * specular;
    lit += material.albedo * material.emissive;

    // Height and distance layers are deliberately strong enough to make deep valleys read as deep.
    float heightWarmth = saturate((input.worldPosition.y - 4.0) / 22.0);
    lit += float3(0.045, 0.026, 0.008) * heightWarmth;

    float fogAmount = saturate((input.depth - 40.0) / 110.0);
    float3 fogColor = float3(0.105, 0.165, 0.235);
    float verticalFog = saturate(1.0 - (input.worldPosition.y - 3.0) / 35.0);
    fogAmount = saturate(fogAmount + verticalFog * fogAmount * 0.18);
    lit = lerp(lit, fogColor, fogAmount * 0.72);

    lit = acesTone(lit);
    lit = pow(max(lit, 0.0), 1.0 / 2.2);
    return float4(lit, 1.0);
}

float boxMask(float2 p, float2 center, float2 halfSize, float softness) {
    float2 q = abs(p - center) - halfSize;
    float outside = length(max(q, 0.0));
    float inside = min(max(q.x, q.y), 0.0);
    return 1.0 - smoothstep(0.0, softness, outside + inside);
}

float ringMask(float2 p, float radius, float thickness) {
    return 1.0 - smoothstep(thickness, thickness + 0.0018, abs(length(p) - radius));
}

float4 PSHud(FullscreenOutput input) : SV_Target0 {
    float2 uv = input.uv;
    float2 px = (uv - 0.5) * float2(pushData.viewportWidth / max(pushData.viewportHeight, 1.0), 1.0);
    float aspectPixel = pushData.viewportWidth / max(pushData.viewportHeight, 1.0);

    float alpha = 0.0;
    float3 color = float3(0.0, 0.0, 0.0);

    // Fine central crosshair with a gold-white center and subtle dark backing.
    float crossH = boxMask(px, float2(0, 0), float2(0.018, 0.00145), 0.0012);
    float crossV = boxMask(px, float2(0, 0), float2(0.00145, 0.018), 0.0012);
    float centerCut = 1.0 - boxMask(px, float2(0, 0), float2(0.0045, 0.0045), 0.0010);
    float cross = max(crossH, crossV) * centerCut;
    color = lerp(color, float3(0.94, 0.88, 0.69), cross);
    alpha = max(alpha, cross * 0.92);

    float centerGem = ringMask(px, 0.0062, 0.0018);
    color = lerp(color, float3(0.30, 0.68, 1.00), centerGem);
    alpha = max(alpha, centerGem * 0.95);

    // Mining progress ring only appears once structural damage exists.
    if (pushData.miningProgress > 0.001) {
        float radius = 0.031;
        float angle = atan2(px.y, px.x);
        float normalizedAngle = frac(angle / 6.2831853 + 0.25);
        float ring = ringMask(px, radius, 0.0028);
        float progress = step(normalizedAngle, saturate(pushData.miningProgress));
        float active = ring * progress;
        color = lerp(color, float3(1.00, 0.68, 0.17), active);
        alpha = max(alpha, active * 0.92);
    }

    // Compact mining-mode badge at bottom-left. Gold = block, blue = micro, split cyan/gold = mixed.
    float2 badgeCenter = float2(-aspectPixel * 0.5 + 0.16, -0.435);
    float badge = boxMask(px, badgeCenter, float2(0.125, 0.030), 0.008);
    float3 modeColor = pushData.miningMode < 0.5 ? float3(0.93, 0.63, 0.18) :
                       (pushData.miningMode < 1.5 ? float3(0.23, 0.66, 1.00) : float3(0.36, 0.88, 0.80));
    color = lerp(color, modeColor * 0.24, badge);
    alpha = max(alpha, badge * 0.66);

    // Nine-slot survival hotbar silhouette, styled after the forged inventory frame.
    float hotbarY = -0.425;
    for (int i = 0; i < 9; ++i) {
        float slotX = (float(i) - 4.0) * 0.064;
        float slot = boxMask(px, float2(slotX, hotbarY), float2(0.028, 0.028), 0.004);
        float edge = slot - boxMask(px, float2(slotX, hotbarY), float2(0.023, 0.023), 0.003);
        color = lerp(color, float3(0.025, 0.032, 0.045), slot * 0.78);
        color = lerp(color, float3(0.51, 0.34, 0.13), edge);
        alpha = max(alpha, slot * 0.72);
    }

    return float4(color, alpha);
}
