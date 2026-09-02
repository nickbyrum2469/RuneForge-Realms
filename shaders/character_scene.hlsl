struct VSOutput {
    float4 position : SV_Position;
    float3 normal : TEXCOORD0;
    float depth : TEXCOORD1;
    float3 worldPosition : TEXCOORD2;
    nointerpolation uint material : TEXCOORD3;
};

float hash31(float3 p) {
    p = frac(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return frac((p.x + p.y) * p.z);
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

float3 acesTone(float3 x) {
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float4 PSCharacter(VSOutput input) : SV_Target0 {
    const uint material = input.material & 0xffu;
    const float fine = noise3(input.worldPosition * 21.0);
    const float medium = noise3(input.worldPosition * 6.5 + 13.0);

    float3 albedo = float3(0.48, 0.28, 0.14);
    float roughness = 0.80;
    float metallic = 0.0;

    if (material == 11u) { // warm stylized skin
        albedo = lerp(float3(0.56,0.29,0.16), float3(0.86,0.57,0.34), fine * 0.26 + 0.56);
        roughness = 0.67;
    } else if (material == 12u) { // blue tunic / cloth
        const float weave = abs(frac((input.worldPosition.x + input.worldPosition.y) * 30.0) - 0.5);
        albedo = lerp(float3(0.025,0.105,0.20), float3(0.075,0.31,0.55), medium * 0.62 + 0.20);
        albedo *= lerp(0.90, 1.06, smoothstep(0.22,0.48,weave));
        roughness = 0.94;
    } else if (material == 13u) { // worn leather
        const float scuff = smoothstep(0.70,0.93,fine);
        albedo = lerp(float3(0.105,0.048,0.018), float3(0.39,0.19,0.055), medium * 0.72 + 0.18);
        albedo = lerp(albedo, float3(0.50,0.30,0.13), scuff * 0.20);
        roughness = 0.82;
    } else if (material == 14u) { // chipped dark steel
        const float edgeFleck = smoothstep(0.76,0.96,fine);
        albedo = lerp(float3(0.085,0.095,0.11), float3(0.29,0.32,0.34), medium * 0.48 + 0.18);
        albedo = lerp(albedo, float3(0.54,0.49,0.39), edgeFleck * 0.22);
        roughness = 0.39;
        metallic = 0.72;
    }

    const float3 n = normalize(input.normal);
    const float3 sunDirection = normalize(float3(-0.52, 0.63, -0.44));
    const float direct = saturate(dot(n, sunDirection));
    const float sky = saturate(n.y * 0.5 + 0.5);
    const float3 warmSun = float3(1.0,0.86,0.66);
    const float3 coolAmbient = float3(0.25,0.36,0.52);
    float3 color = albedo * (warmSun * (0.20 + direct * 0.94) + coolAmbient * (0.14 + sky * 0.22));

    if (metallic > 0.0) {
        const float rim = pow(1.0 - saturate(abs(n.z)), 3.0);
        color += float3(0.52,0.62,0.76) * rim * metallic * (1.0 - roughness) * 0.34;
    }

    color = acesTone(color * 0.92);
    color = pow(max(color, 0.0), 1.0 / 2.2);
    return float4(color, 1.0);
}
