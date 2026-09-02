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
    float fovScale;
    uint hotbar0;
    uint hotbar1;
    uint hotbar2;
    uint hotbar3;
    uint hotbar4;
    uint hotbar5;
    uint hotbar6;
    uint hotbar7;
    uint hotbar8;
    uint selectedHotbar;
    float foliageQuality;
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

float2 hash22(float2 p) {
    float n = hash21(p);
    return frac(float2(n, hash11(n + 0.371)) * float2(17.17, 31.73));
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
    float n000 = hash31(i + float3(0,0,0));
    float n100 = hash31(i + float3(1,0,0));
    float n010 = hash31(i + float3(0,1,0));
    float n110 = hash31(i + float3(1,1,0));
    float n001 = hash31(i + float3(0,0,1));
    float n101 = hash31(i + float3(1,0,1));
    float n011 = hash31(i + float3(0,1,1));
    float n111 = hash31(i + float3(1,1,1));
    float x00 = lerp(n000, n100, f.x);
    float x10 = lerp(n010, n110, f.x);
    float x01 = lerp(n001, n101, f.x);
    float x11 = lerp(n011, n111, f.x);
    return lerp(lerp(x00, x10, f.y), lerp(x01, x11, f.y), f.z);
}

float fbm2(float2 p) {
    float v = noise2(p) * 0.57;
    v += noise2(p * 2.03 + 13.2) * 0.27;
    v += noise2(p * 4.11 + 37.6) * 0.11;
    v += noise2(p * 8.21 + 71.9) * 0.05;
    return v;
}

float fbm3(float3 p) {
    float v = noise3(p) * 0.56;
    v += noise3(p * 2.03 + 17.1) * 0.28;
    v += noise3(p * 4.07 + 41.7) * 0.11;
    v += noise3(p * 8.11 + 71.3) * 0.05;
    return v;
}

float2 surfaceUv(float3 p, float3 n) {
    float3 a = abs(n);
    if (a.y >= a.x && a.y >= a.z) return p.xz;
    if (a.x >= a.z) return p.zy;
    return p.xy;
}

// Returns nearest-cell distance and a stable cell random. This produces irregular rock/soil forms
// without the obvious diagonal X pattern of the old shared plate-crack function.
float2 cellular2(float2 p) {
    float2 cell = floor(p);
    float2 local = frac(p);
    float nearest = 10.0;
    float second = 10.0;
    float cellRandom = 0.0;
    [unroll] for (int y = -1; y <= 1; ++y) {
        [unroll] for (int x = -1; x <= 1; ++x) {
            float2 offset = float2(x, y);
            float2 id = cell + offset;
            float2 sitePoint = offset + 0.18 + hash22(id) * 0.64;
            float d = length(sitePoint - local);
            if (d < nearest) {
                second = nearest;
                nearest = d;
                cellRandom = hash21(id + 9.17);
            } else if (d < second) second = d;
        }
    }
    return float2(max(second - nearest, 0.0), cellRandom);
}

struct MaterialSample {
    float3 albedo;
    float roughness;
    float relief;
    float cavity;
    float emissive;
    float alpha;
};

MaterialSample sampleMaterial(uint material, float3 p, float3 n) {
    MaterialSample s;
    s.albedo = float3(0.5, 0.5, 0.5);
    s.roughness = 0.9;
    s.relief = 0.0;
    s.cavity = 0.0;
    s.emissive = 0.0;
    s.alpha = 1.0;

    float2 uv = surfaceUv(p, n);
    float macro = fbm3(p * 0.42);
    float medium = fbm3(p * 2.25);
    float fine = noise3(p * 12.5);

    if (material == 0) {
        float lush = fbm2(p.xz * 1.35);
        float fleck = noise2(p.xz * 14.0);
        float3 deep = float3(0.055, 0.205, 0.040);
        float3 mid = float3(0.205, 0.455, 0.095);
        float3 fresh = float3(0.405, 0.670, 0.155);
        s.albedo = lerp(deep, mid, saturate(lush * 1.08));
        s.albedo = lerp(s.albedo, fresh, smoothstep(0.63, 0.92, fleck) * 0.30);
        s.roughness = 0.91;
        s.relief = lush * 0.26 + fleck * 0.07;
        s.cavity = (1.0 - lush) * 0.08;
    } else if (material == 1) {
        float localY = frac(p.y + 0.001);
        float turf = smoothstep(0.86, 0.995, localY);
        float rootWarp = fbm2(float2(uv.x * 1.7, uv.y * 0.55));
        float root = 1.0 - smoothstep(0.028, 0.075, abs(frac(uv.x * 4.2 + rootWarp * 0.65) - 0.5));
        root *= smoothstep(0.12, 0.82, frac(uv.y * 0.83 + hash21(floor(uv.xx * 3.0))));
        float3 soil = lerp(float3(0.155, 0.072, 0.028), float3(0.355, 0.205, 0.085), macro);
        float3 green = lerp(float3(0.095, 0.285, 0.045), float3(0.335, 0.590, 0.115), medium);
        s.albedo = lerp(soil, green, turf);
        s.albedo = lerp(s.albedo, float3(0.42, 0.30, 0.16), root * (1.0 - turf) * 0.28);
        s.roughness = 0.96;
        s.relief = medium * 0.19 + root * 0.10;
        s.cavity = root * 0.10 + (1.0 - medium) * 0.05;
    } else if (material == 2) {
        float2 cells = cellular2(uv * 7.2 + macro * 0.31);
        float clumpEdge = 1.0 - smoothstep(0.035, 0.115, cells.x);
        float pebbleSeed = hash21(floor(uv * 11.0));
        float pebble = smoothstep(0.90, 0.985, pebbleSeed) * smoothstep(0.58, 0.83, fine);
        float moisture = fbm2(uv * 0.72 + 19.0);
        float3 drySoil = float3(0.385, 0.218, 0.090);
        float3 richSoil = float3(0.145, 0.065, 0.028);
        s.albedo = lerp(richSoil, drySoil, saturate(macro * 0.72 + 0.15));
        s.albedo *= lerp(0.82, 1.06, moisture);
        s.albedo = lerp(s.albedo, float3(0.42, 0.39, 0.32), pebble * 0.66);
        s.albedo *= 1.0 - clumpEdge * 0.11;
        s.roughness = lerp(0.94, 0.985, 1.0 - moisture);
        s.relief = medium * 0.27 + pebble * 0.24 - clumpEdge * 0.11;
        s.cavity = clumpEdge * 0.18 + (1.0 - medium) * 0.05;
    } else if (material == 3) {
        float2 cells = cellular2(uv * 3.15 + fbm2(uv * 0.65) * 0.36);
        float crevice = 1.0 - smoothstep(0.025, 0.105, cells.x);
        float2 smallCells = cellular2(uv * 8.6 + medium * 0.22);
        float microCrevice = (1.0 - smoothstep(0.015, 0.055, smallCells.x)) * 0.32;
        float slab = cells.y;
        float3 charcoal = float3(0.205, 0.225, 0.235);
        float3 neutral = float3(0.445, 0.455, 0.445);
        float3 warm = float3(0.585, 0.555, 0.495);
        s.albedo = lerp(charcoal, neutral, saturate(macro * 0.68 + slab * 0.22));
        s.albedo = lerp(s.albedo, warm, smoothstep(0.72, 0.96, slab) * 0.28);
        s.albedo *= 1.0 - crevice * 0.58 - microCrevice * 0.22;
        s.roughness = 0.86 + crevice * 0.08;
        s.relief = slab * 0.28 + medium * 0.12 - crevice * 0.35 - microCrevice * 0.10;
        s.cavity = saturate(crevice * 0.86 + microCrevice * 0.42);
    } else if (material == 4) {
        float warp = fbm2(float2(uv.x * 0.55, uv.y * 0.18));
        float ridgePhase = uv.x * 10.5 + warp * 2.7 + sin(uv.y * 1.4) * 0.24;
        float ridge = 0.5 + 0.5 * sin(ridgePhase * 6.2831853);
        ridge = pow(ridge, 1.65);
        float splitNoise = fbm2(float2(uv.x * 2.1 + warp, uv.y * 0.46));
        float split = smoothstep(0.72, 0.88, splitNoise) * smoothstep(0.22, 0.82, 1.0 - ridge);
        float knot = smoothstep(0.86, 0.96, fbm2(float2(uv.x * 0.85, uv.y * 0.72) + 31.0));
        float3 darkBark = float3(0.145, 0.070, 0.027);
        float3 warmBark = float3(0.405, 0.220, 0.075);
        s.albedo = lerp(darkBark, warmBark, ridge * 0.62 + macro * 0.38);
        s.albedo *= 1.0 - split * 0.48;
        s.albedo = lerp(s.albedo, float3(0.20, 0.105, 0.045), knot * 0.35);
        s.roughness = 0.93;
        s.relief = ridge * 0.35 - split * 0.31 + knot * 0.10;
        s.cavity = split * 0.62 + (1.0 - ridge) * 0.07;
    } else if (material == 5) {
        float2 q = frac(uv) - 0.5;
        float radialWarp = fbm2(uv * 2.0) * 0.035;
        float r = length(q) + radialWarp;
        float rings = 0.5 + 0.5 * sin(r * 76.0 + fbm2(uv * 3.0) * 4.5);
        float radialSplit = 1.0 - smoothstep(0.018, 0.060, abs(atan2(q.y, q.x) - (hash21(floor(uv)) - 0.5) * 2.8));
        s.albedo = lerp(float3(0.300, 0.155, 0.060), float3(0.665, 0.410, 0.155), rings * 0.42 + macro * 0.58);
        s.albedo *= 1.0 - radialSplit * 0.24;
        s.roughness = 0.87;
        s.relief = rings * 0.16 - radialSplit * 0.12;
        s.cavity = radialSplit * 0.24;
    } else if (material == 6) {
        float2 local = frac(uv * 2.35);
        float leafCell = hash21(floor(uv * 7.0));
        float veins = abs(local.x - 0.5) * 0.7 + abs(local.y - 0.5);
        float maskNoise = fbm2(uv * 5.6 + leafCell * 4.0);
        float leafMask = smoothstep(0.36, 0.58, maskNoise - veins * 0.13);
        float clump = fbm3(p * 1.7);
        s.albedo = lerp(float3(0.035, 0.175, 0.045), float3(0.235, 0.510, 0.105), saturate(clump * 0.82 + medium * 0.24));
        s.roughness = 0.82;
        s.relief = clump * 0.22 + medium * 0.08;
        s.cavity = (1.0 - clump) * 0.10;
        s.alpha = leafMask;
    } else if (material == 7) {
        s.albedo = lerp(float3(0.80,0.82,0.82), float3(1.0,0.97,0.90), fine);
        s.roughness = 0.72; s.relief = fine * 0.10; s.emissive = 0.018;
    } else if (material == 8) {
        s.albedo = lerp(float3(0.82,0.46,0.035), float3(1.0,0.88,0.16), fine);
        s.roughness = 0.70; s.relief = fine * 0.10; s.emissive = 0.025;
    } else {
        s.albedo = lerp(float3(0.16,0.33,0.72), float3(0.42,0.72,1.0), fine);
        s.roughness = 0.68; s.relief = fine * 0.10; s.emissive = 0.025;
    }
    return s;
}

float materialHeight(uint material, float3 p, float3 n) {
    return sampleMaterial(material, p, n).relief;
}

float3 detailNormal(uint material, float3 p, float3 n) {
    float3 tangent = abs(n.y) < 0.92 ? normalize(cross(float3(0,1,0), n)) : float3(1,0,0);
    float3 bitangent = normalize(cross(n, tangent));
    float epsilon = 0.031;
    float h = materialHeight(material, p, n);
    float ht = materialHeight(material, p + tangent * epsilon, n);
    float hb = materialHeight(material, p + bitangent * epsilon, n);
    float strength = material == 3 ? 0.66 : (material == 4 ? 0.54 : (material <= 2 ? 0.42 : 0.46));
    return normalize(n - tangent * ((ht - h) / epsilon) * strength -
                     bitangent * ((hb - h) / epsilon) * strength);
}

float segmentDistance(float2 p, float2 a, float2 b) {
    float2 pa = p - a;
    float2 ba = b - a;
    float h = saturate(dot(pa, ba) / max(dot(ba, ba), 0.00001));
    return length(pa - ba * h);
}

float organicDamage(float3 worldPosition, float3 normal, uint material, uint stage) {
    if (stage == 0) return 0.0;
    float2 uv = frac(surfaceUv(worldPosition, normal));
    float3 interior = worldPosition - normal * 0.0025;
    float3 block = floor(interior);
    float seed = hash31(block * 0.173 + float3(material * 1.37, 7.91, 3.11));
    float2 origin = float2(0.36, 0.44) + (hash22(block.xz + seed) - 0.5) * 0.22;
    float crack = 0.0;
    const int branchCount = min(2 + (int)stage, 7);
    [unroll] for (int i = 0; i < 7; ++i) {
        if (i >= branchCount) break;
        float fi = (float)i;
        float branchSeed = hash11(seed * 17.0 + fi * 9.13);
        float angle = branchSeed * 6.2831853 + fi * 1.71;
        if (material == 4 || material == 5) angle = lerp(angle, 1.5707963, 0.58);
        float lengthScale = 0.16 + (float)stage * 0.060 + hash11(branchSeed + 2.2) * 0.13;
        float2 dir = float2(cos(angle), sin(angle));
        float2 mid = origin + dir * lengthScale * 0.52;
        mid += float2(-dir.y, dir.x) * (hash11(branchSeed + 4.8) - 0.5) * 0.12;
        float2 end = origin + dir * lengthScale;
        float width = lerp(0.010, 0.018, (float)stage / 5.0);
        float d = min(segmentDistance(uv, origin, mid), segmentDistance(uv, mid, end));
        crack = max(crack, 1.0 - smoothstep(width, width + 0.010, d));

        if (stage >= 3 && i < 3) {
            float2 sideDir = normalize(float2(-dir.y, dir.x) * (branchSeed > 0.5 ? 1.0 : -1.0) + dir * 0.35);
            float2 sideEnd = mid + sideDir * lengthScale * 0.42;
            float sd = segmentDistance(uv, mid, sideEnd);
            crack = max(crack, (1.0 - smoothstep(width * 0.72, width * 0.72 + 0.009, sd)) * 0.88);
        }
    }

    if (material <= 2) {
        float crumb = smoothstep(0.63 - stage * 0.045, 0.86 - stage * 0.030,
                                 fbm2(uv * 8.0 + seed * 13.0));
        float centerFalloff = 1.0 - smoothstep(0.10, 0.52, length(uv - origin));
        crack = max(crack * 0.42, crumb * centerFalloff * 0.72);
    }
    return saturate(crack);
}

float3 acesTone(float3 x) {
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float3 cameraForward() {
    float cp = cos(pushData.pitch);
    return normalize(float3(sin(pushData.yaw) * cp, sin(pushData.pitch), cos(pushData.yaw) * cp));
}

float3 cameraRay(float2 uv) {
    float2 ndc = uv * 2.0 - 1.0;
    ndc.y = -ndc.y;
    float3 forward = cameraForward();
    float3 right = normalize(float3(cos(pushData.yaw), 0.0, -sin(pushData.yaw)));
    float3 up = normalize(cross(forward, right));
    float f = max(pushData.fovScale, 0.45);
    return normalize(forward + right * (ndc.x * pushData.aspect / f) + up * (ndc.y / f));
}

VSOutput VSMain(VSInput input) {
    VSOutput output;
    float3 eye = float3(pushData.eyeX, pushData.eyeY, pushData.eyeZ);
    float3 delta = input.position - eye;
    float3 forward = cameraForward();
    float3 right = normalize(float3(cos(pushData.yaw), 0.0, -sin(pushData.yaw)));
    float3 up = normalize(cross(forward, right));
    float3 view = float3(dot(delta, right), dot(delta, up), dot(delta, forward));

    const float nearPlane = 0.08;
    const float farPlane = 360.0;
    float f = max(pushData.fovScale, 0.45);
    float projectedZ = (farPlane / (farPlane - nearPlane)) * view.z -
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
    float2 position = vertexId == 0 ? float2(-1.0,-1.0) :
                      (vertexId == 1 ? float2(-1.0,3.0) : float2(3.0,-1.0));
    output.position = float4(position, 0.99999, 1.0);
    output.uv = position * 0.5 + 0.5;
    return output;
}

float4 PSSky(FullscreenOutput input) : SV_Target0 {
    float3 ray = cameraRay(input.uv);
    float horizon = saturate(ray.y * 0.5 + 0.5);
    float3 horizonColor = float3(0.50, 0.43, 0.37);
    float3 midColor = float3(0.20, 0.34, 0.52);
    float3 zenithColor = float3(0.050, 0.105, 0.195);
    float3 sky = lerp(horizonColor, midColor, smoothstep(0.46, 0.65, horizon));
    sky = lerp(sky, zenithColor, smoothstep(0.62, 0.96, horizon));

    const float3 sunDirection = normalize(float3(-0.52, 0.63, -0.44));
    float sunDot = dot(ray, sunDirection);
    float sun = smoothstep(0.9970, 0.99935, sunDot);
    float halo = pow(saturate(sunDot), 42.0);
    sky += float3(1.0,0.72,0.34) * sun * 3.2;
    sky += float3(1.0,0.54,0.25) * halo * 0.34;

    if (ray.y > 0.018) {
        const float cloudHeight = 118.0;
        float t = (cloudHeight - pushData.eyeY) / max(ray.y, 0.018);
        float2 cloudWorld = float2(pushData.eyeX, pushData.eyeZ) + ray.xz * t;
        float2 wind = float2(pushData.time * 0.42, pushData.time * 0.13);
        float clouds = fbm2((cloudWorld + wind) * 0.0105);
        clouds = smoothstep(0.58, 0.76, clouds);
        float cloudFade = smoothstep(0.02, 0.12, ray.y) * (1.0 - smoothstep(0.76, 0.98, ray.y));
        float3 cloudColor = lerp(float3(0.49,0.51,0.54), float3(0.94,0.90,0.83), saturate(sunDot * 0.5 + 0.5));
        sky = lerp(sky, cloudColor, clouds * cloudFade * 0.48);
    }

    sky += float3(0.34,0.29,0.26) * exp(-abs(ray.y) * 22.0) * 0.16;
    sky = acesTone(sky * 0.82);
    sky = pow(max(sky, 0.0), 1.0 / 2.2);
    return float4(sky, 1.0);
}

float4 PSMain(VSOutput input) : SV_Target0 {
    uint baseMaterial = input.material & 0xffu;
    uint damageStage = (input.material >> 8u) & 0xffu;
    float3 geometricNormal = normalize(input.normal);
    MaterialSample material = sampleMaterial(baseMaterial, input.worldPosition, geometricNormal);

    if (baseMaterial == 6) clip(material.alpha - 0.46);

    float damage = organicDamage(input.worldPosition, geometricNormal, baseMaterial, damageStage);
    if (damage > 0.0) {
        float stage = saturate((float)damageStage / 5.0);
        material.albedo *= 1.0 - damage * (0.18 + stage * 0.38);
        material.cavity = saturate(material.cavity + damage * (0.36 + stage * 0.42));
        material.roughness = saturate(material.roughness + damage * 0.08);
        material.relief -= damage * 0.12;
    }

    float3 n = detailNormal(baseMaterial, input.worldPosition, geometricNormal);
    const float3 sunDirection = normalize(float3(-0.52, 0.63, -0.44));
    float direct = max(dot(n, sunDirection), 0.0);
    float skyLight = saturate(n.y * 0.52 + 0.48);
    float3 viewDirection = normalize(float3(pushData.eyeX, pushData.eyeY, pushData.eyeZ) - input.worldPosition);
    float3 halfVector = normalize(sunDirection + viewDirection);
    float specular = pow(saturate(dot(n, halfVector)), lerp(10.0, 68.0, 1.0 - material.roughness));
    specular *= (1.0 - material.roughness) * 0.20;

    float ao = 1.0 - saturate(material.cavity) * 0.62;
    float3 warmSun = float3(1.00, 0.88, 0.70);
    float3 coolSky = float3(0.34, 0.47, 0.66);
    float3 lighting = warmSun * (0.16 + direct * 0.90) + coolSky * (0.10 + skyLight * 0.22);
    float3 lit = material.albedo * lighting * ao;
    lit += warmSun * specular;
    lit += material.albedo * material.emissive;

    float fog = smoothstep(68.0, 205.0, max(input.depth, 0.0));
    float3 fogColor = float3(0.39, 0.48, 0.56);
    lit = lerp(lit, fogColor, fog * 0.62);
    lit = acesTone(lit * 0.86);
    lit = pow(max(lit, 0.0), 1.0 / 2.2);
    return float4(lit, 1.0);
}

float boxMask(float2 p, float2 center, float2 halfSize, float feather) {
    float2 d = abs(p - center) - halfSize;
    float outside = length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
    return 1.0 - smoothstep(0.0, feather, outside);
}

float ringMask(float2 p, float radius, float thickness) {
    return 1.0 - smoothstep(thickness, thickness + 0.0018, abs(length(p) - radius));
}

uint hotbarValue(int index) {
    if (index == 0) return pushData.hotbar0;
    if (index == 1) return pushData.hotbar1;
    if (index == 2) return pushData.hotbar2;
    if (index == 3) return pushData.hotbar3;
    if (index == 4) return pushData.hotbar4;
    if (index == 5) return pushData.hotbar5;
    if (index == 6) return pushData.hotbar6;
    if (index == 7) return pushData.hotbar7;
    return pushData.hotbar8;
}

float3 itemColor(uint itemId) {
    if (itemId == 1u) return float3(0.30, 0.58, 0.12);
    if (itemId == 2u) return float3(0.38, 0.22, 0.09);
    if (itemId == 3u) return float3(0.45, 0.47, 0.47);
    if (itemId == 4u) return float3(0.48, 0.25, 0.07);
    if (itemId == 5u) return float3(0.15, 0.42, 0.12);
    return float3(0.08, 0.10, 0.13);
}

float4 PSHud(FullscreenOutput input) : SV_Target0 {
    float2 px = (input.uv - 0.5) * float2(pushData.viewportWidth / max(pushData.viewportHeight, 1.0), 1.0);
    float alpha = 0.0;
    float3 color = float3(0.0,0.0,0.0);

    float crossH = boxMask(px, float2(0,0), float2(0.014,0.0012), 0.0010);
    float crossV = boxMask(px, float2(0,0), float2(0.0012,0.014), 0.0010);
    float centerCut = 1.0 - boxMask(px, float2(0,0), float2(0.0040,0.0040), 0.0008);
    float cross = max(crossH, crossV) * centerCut;
    color = lerp(color, float3(0.96,0.92,0.82), cross);
    alpha = max(alpha, cross * 0.94);
    float centerGem = ringMask(px, 0.0054, 0.0015);
    color = lerp(color, float3(0.35,0.70,1.0), centerGem);
    alpha = max(alpha, centerGem * 0.93);

    if (pushData.miningProgress > 0.001) {
        float radius = 0.028;
        float angle = atan2(px.y, px.x);
        float normalizedAngle = frac(angle / 6.2831853 + 0.25);
        float ring = ringMask(px, radius, 0.0023) * step(normalizedAngle, saturate(pushData.miningProgress));
        float3 modeColor = pushData.miningMode < 0.5 ? float3(0.94,0.68,0.24) :
                           (pushData.miningMode < 1.5 ? float3(0.30,0.68,1.0) : float3(0.36,0.88,0.76));
        color = lerp(color, modeColor, ring * 0.90);
        alpha = max(alpha, ring * 0.92);
    }

    const float hotbarY = 0.425;
    [unroll] for (int i = 0; i < 9; ++i) {
        float slotX = (float(i) - 4.0) * 0.064;
        float slot = boxMask(px, float2(slotX, hotbarY), float2(0.028,0.028), 0.0035);
        float inner = boxMask(px, float2(slotX, hotbarY), float2(0.022,0.022), 0.0025);
        bool selected = (uint)i == pushData.selectedHotbar;
        float3 borderColor = selected ? float3(0.98,0.78,0.32) : float3(0.47,0.34,0.18);
        color = lerp(color, float3(0.022,0.028,0.039), slot * 0.88);
        color = lerp(color, borderColor, (slot - inner) * (selected ? 1.0 : 0.82));
        alpha = max(alpha, slot * 0.86);

        uint packed = hotbarValue(i);
        uint itemId = packed & 0xffu;
        uint count = (packed >> 8u) & 0xffu;
        if (itemId != 0u && count > 0u) {
            float item = boxMask(px, float2(slotX, hotbarY - 0.002), float2(0.0135,0.0135), 0.0025);
            float highlight = boxMask(px, float2(slotX - 0.004, hotbarY - 0.006), float2(0.008,0.004), 0.0018);
            float3 ic = itemColor(itemId);
            color = lerp(color, ic, item * 0.94);
            color = lerp(color, min(ic * 1.45, 1.0), highlight * item * 0.38);
            alpha = max(alpha, item * 0.96);

            float countWidth = 0.020 * saturate((float)count / 64.0);
            float meter = boxMask(px, float2(slotX - 0.020 + countWidth, hotbarY + 0.020),
                                 float2(max(countWidth, 0.001), 0.0020), 0.0010);
            color = lerp(color, float3(0.86,0.91,0.96), meter);
            alpha = max(alpha, meter * 0.94);
        }
    }

    return float4(color, alpha);
}
