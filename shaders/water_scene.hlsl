struct PushData {
    float time;
    float aspect;
    float eyeX;
    float eyeY;
    float eyeZ;
    float cameraForwardX;
    float cameraForwardY;
    float cameraForwardZ;
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

struct VSOutput {
    float4 position : SV_Position;
    float3 normal : TEXCOORD0;
    float depth : TEXCOORD1;
    float3 worldPosition : TEXCOORD2;
    nointerpolation uint material : TEXCOORD3;
};

float hash21(float2 p) {
    float3 p3 = frac(float3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.x + p3.y) * p3.z);
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

float fbm2(float2 p) {
    float v = noise2(p) * 0.58;
    v += noise2(p * 2.03 + 11.7) * 0.27;
    v += noise2(p * 4.13 + 29.2) * 0.10;
    v += noise2(p * 8.17 + 53.4) * 0.05;
    return v;
}

float3 acesTone(float3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float3 worldWaterNormal(float3 p, float3 geometricNormal) {
    // Broad rolling shapes carry the body while smaller cross-waves make the surface read as a
    // crystalline voxel liquid. Everything is world/time based; camera motion can never drag it.
    float2 uv = p.xz;
    float2 wind = float2(pushData.time * 0.16, pushData.time * 0.105);
    float h0 = sin((uv.x * 0.76 + uv.y * 0.34) + pushData.time * 0.76) * 0.025;
    float h1 = sin((uv.x * -0.47 + uv.y * 0.92) + pushData.time * 0.57) * 0.018;
    float h2 = sin((uv.x * 1.62 - uv.y * 1.18) + pushData.time * 1.06) * 0.008;
    float h3 = (fbm2((uv + wind) * 1.55) - 0.5) * 0.024;

    const float epsilon = 0.050;
    float2 ux = uv + float2(epsilon, 0.0);
    float2 uz = uv + float2(0.0, epsilon);
    float hx = sin((ux.x * 0.76 + ux.y * 0.34) + pushData.time * 0.76) * 0.025 +
               sin((ux.x * -0.47 + ux.y * 0.92) + pushData.time * 0.57) * 0.018 +
               sin((ux.x * 1.62 - ux.y * 1.18) + pushData.time * 1.06) * 0.008 +
               (fbm2((ux + wind) * 1.55) - 0.5) * 0.024;
    float hz = sin((uz.x * 0.76 + uz.y * 0.34) + pushData.time * 0.76) * 0.025 +
               sin((uz.x * -0.47 + uz.y * 0.92) + pushData.time * 0.57) * 0.018 +
               sin((uz.x * 1.62 - uz.y * 1.18) + pushData.time * 1.06) * 0.008 +
               (fbm2((uz + wind) * 1.55) - 0.5) * 0.024;
    float h = h0 + h1 + h2 + h3;

    if (abs(geometricNormal.y) > 0.75) {
        return normalize(float3(-(hx - h) / epsilon * 0.78, 1.0, -(hz - h) / epsilon * 0.78));
    }

    float ripple = (fbm2(float2(p.y * 0.82, dot(p.xz, float2(0.67, 0.41))) + wind * 0.42) - 0.5) * 0.19;
    return normalize(geometricNormal + float3(ripple, ripple * 0.22, -ripple));
}

float4 PSWater(VSOutput input) : SV_Target0 {
    float3 geometricNormal = normalize(input.normal);
    float3 n = worldWaterNormal(input.worldPosition, geometricNormal);
    float3 eye = float3(pushData.eyeX, pushData.eyeY, pushData.eyeZ);
    float3 viewDirection = normalize(eye - input.worldPosition);

    const float3 sunDirection = normalize(float3(-0.52, 0.63, -0.44));
    float ndv = saturate(abs(dot(n, viewDirection)));
    float fresnel = 0.045 + 0.955 * pow(1.0 - ndv, 5.0);

    // Deeper cobalt body with saturated cyan shallows, matching the user's crystalline reference
    // instead of the washed gray/teal 0.5.2 lake. This is stylized optical depth, not fake fluid truth.
    float sideDepth = saturate((7.0 - input.worldPosition.y) / 5.5);
    float distanceDepth = saturate(input.depth / 145.0);
    float depthFactor = saturate(sideDepth * 0.72 + distanceDepth * 0.18);
    float3 shallowColor = float3(0.018, 0.355, 0.680);
    float3 middleColor = float3(0.008, 0.185, 0.465);
    float3 deepColor = float3(0.004, 0.045, 0.190);
    float3 bodyColor = lerp(shallowColor, middleColor, smoothstep(0.05, 0.58, depthFactor));
    bodyColor = lerp(bodyColor, deepColor, smoothstep(0.48, 1.0, depthFactor));

    // World-locked quantized glints echo the small block facets in the reference cube without making
    // the actual water surface a static checkerboard.
    float2 facetId = floor(input.worldPosition.xz * 10.0 + float2(pushData.time * 0.55, -pushData.time * 0.36));
    float facet = hash21(facetId + 31.0);
    float facetGlint = step(0.91, facet) * (0.35 + 0.65 * fresnel);
    float causticA = abs(sin(input.worldPosition.x * 2.1 + pushData.time * 0.82) *
                         sin(input.worldPosition.z * 1.7 - pushData.time * 0.67));
    float causticB = fbm2(input.worldPosition.xz * 2.7 + float2(pushData.time * 0.22, -pushData.time * 0.17));
    float caustic = smoothstep(0.60, 0.94, causticA * 0.62 + causticB * 0.58);
    bodyColor += float3(0.015, 0.30, 0.50) * caustic * (1.0 - depthFactor) * 0.42;

    float3 reflectedSky = lerp(float3(0.10, 0.34, 0.66), float3(0.62, 0.82, 0.98), saturate(n.y * 0.5 + 0.5));
    float3 halfVector = normalize(sunDirection + viewDirection);
    float sunGlint = pow(saturate(dot(n, halfVector)), 128.0) * 2.15;

    float broadWave = fbm2(input.worldPosition.xz * 0.27 + float2(pushData.time * 0.034, pushData.time * 0.019));
    bodyColor *= 0.90 + broadWave * 0.20;

    float3 color = lerp(bodyColor, reflectedSky, saturate(fresnel * 0.80));
    color += float3(0.55, 0.92, 1.00) * facetGlint * 0.36;
    color += float3(1.00, 0.91, 0.72) * sunGlint;

    // Keep the distance integration subtle so water retains its blue identity instead of fading to gray.
    float fog = smoothstep(112.0, 255.0, max(input.depth, 0.0));
    color = lerp(color, float3(0.23, 0.38, 0.55), fog * 0.34);
    color = acesTone(color * 0.96);
    color = pow(max(color, 0.0), 1.0 / 2.2);

    const bool topSurface = geometricNormal.y > 0.75;
    float alpha = topSurface ? lerp(0.48, 0.76, fresnel) : lerp(0.62, 0.84, fresnel);
    alpha = saturate(alpha + depthFactor * 0.10);
    return float4(color, alpha);
}
