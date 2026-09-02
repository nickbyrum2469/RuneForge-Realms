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
    // Two broad world-space wave families plus finer wind-driven ripples. The phase uses world
    // position and simulation time only; turning the camera can never drag the water pattern.
    float2 wind = float2(pushData.time * 0.18, pushData.time * 0.11);
    float2 uv = p.xz;
    float h0 = sin((uv.x * 0.72 + uv.y * 0.31) + pushData.time * 0.72) * 0.020;
    float h1 = sin((uv.x * -0.41 + uv.y * 0.88) + pushData.time * 0.53) * 0.014;
    float h2 = (fbm2((uv + wind) * 1.35) - 0.5) * 0.028;

    const float epsilon = 0.055;
    float hx = sin(((uv.x + epsilon) * 0.72 + uv.y * 0.31) + pushData.time * 0.72) * 0.020 +
               sin(((uv.x + epsilon) * -0.41 + uv.y * 0.88) + pushData.time * 0.53) * 0.014 +
               (fbm2((uv + float2(epsilon, 0.0) + wind) * 1.35) - 0.5) * 0.028;
    float hz = sin((uv.x * 0.72 + (uv.y + epsilon) * 0.31) + pushData.time * 0.72) * 0.020 +
               sin((uv.x * -0.41 + (uv.y + epsilon) * 0.88) + pushData.time * 0.53) * 0.014 +
               (fbm2((uv + float2(0.0, epsilon) + wind) * 1.35) - 0.5) * 0.028;
    float h = h0 + h1 + h2;

    if (abs(geometricNormal.y) > 0.75) {
        return normalize(float3(-(hx - h) / epsilon * 0.72, 1.0, -(hz - h) / epsilon * 0.72));
    }

    // Vertical water faces keep their geometric orientation but receive subtle ripple variation.
    float ripple = (fbm2(float2(p.y * 0.72, dot(p.xz, float2(0.63, 0.37))) + wind * 0.35) - 0.5) * 0.16;
    return normalize(geometricNormal + float3(ripple, ripple * 0.25, -ripple));
}

float4 PSWater(VSOutput input) : SV_Target0 {
    float3 geometricNormal = normalize(input.normal);
    float3 n = worldWaterNormal(input.worldPosition, geometricNormal);
    float3 eye = float3(pushData.eyeX, pushData.eyeY, pushData.eyeZ);
    float3 viewDirection = normalize(eye - input.worldPosition);

    const float3 sunDirection = normalize(float3(-0.52, 0.63, -0.44));
    float ndv = saturate(abs(dot(n, viewDirection)));
    float fresnel = 0.035 + 0.965 * pow(1.0 - ndv, 5.0);

    // This pass deliberately remains stylized: clear cyan shallows, teal depth absorption and a
    // cool sky reflection instead of photorealistic mirror water.
    float sideDepth = saturate((6.0 - input.worldPosition.y) / 4.5);
    float distanceDepth = saturate(input.depth / 120.0);
    float depthFactor = saturate(sideDepth * 0.72 + distanceDepth * 0.20);
    float3 shallowColor = float3(0.055, 0.385, 0.500);
    float3 deepColor = float3(0.018, 0.125, 0.245);
    float3 bodyColor = lerp(shallowColor, deepColor, depthFactor);

    float3 reflectedSky = lerp(float3(0.30, 0.49, 0.69), float3(0.70, 0.78, 0.83), saturate(n.y * 0.5 + 0.5));
    float3 halfVector = normalize(sunDirection + viewDirection);
    float sunGlint = pow(saturate(dot(n, halfVector)), 150.0) * 1.85;

    float broadWave = fbm2(input.worldPosition.xz * 0.24 + float2(pushData.time * 0.035, pushData.time * 0.018));
    bodyColor *= 0.91 + broadWave * 0.16;

    float3 color = lerp(bodyColor, reflectedSky, saturate(fresnel * 0.72));
    color += float3(1.00, 0.88, 0.65) * sunGlint;

    // Slight horizon haze keeps large lakes integrated with the same atmosphere as terrain.
    float fog = smoothstep(92.0, 235.0, max(input.depth, 0.0));
    color = lerp(color, float3(0.39, 0.48, 0.56), fog * 0.48);
    color = acesTone(color * 0.88);
    color = pow(max(color, 0.0), 1.0 / 2.2);

    const bool topSurface = geometricNormal.y > 0.75;
    float alpha = topSurface ? lerp(0.42, 0.68, fresnel) : lerp(0.54, 0.76, fresnel);
    alpha = saturate(alpha + depthFactor * 0.08);
    return float4(color, alpha);
}
