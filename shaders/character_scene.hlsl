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
    const float3 voxelCell = floor(input.worldPosition * 17.0);
    const float voxelVariation = hash31(voxelCell + 31.0);

    float3 albedo = float3(0.48, 0.28, 0.14);
    float roughness = 0.80;
    float metallic = 0.0;

    if (material == 11u) { // warm tan/peach skin from the supplied barbarian reference
        // Keep the body visibly voxel-built: broad color families change per small world-space cell
        // instead of a smooth porcelain gradient. Highlights are intentionally restrained so skin
        // does not blow out to the near-white sheet seen in the 0.5.1 first-person screenshot.
        const float3 shadowSkin = float3(0.30, 0.115, 0.042);
        const float3 baseSkin = float3(0.56, 0.275, 0.115);
        const float3 warmSkin = float3(0.72, 0.425, 0.205);
        const float family = step(0.25, voxelVariation) * 0.46 + step(0.72, voxelVariation) * 0.24;
        albedo = lerp(shadowSkin, baseSkin, 0.52 + family);
        albedo = lerp(albedo, warmSkin, step(0.86, voxelVariation) * 0.34);
        albedo *= lerp(0.93, 1.04, step(0.48, fine));
        roughness = 0.88;
    } else if (material == 12u) { // blue tunic / cloth gear
        const float weave = abs(frac((input.worldPosition.x + input.worldPosition.y) * 30.0) - 0.5);
        albedo = lerp(float3(0.025,0.105,0.20), float3(0.075,0.31,0.55), medium * 0.62 + 0.20);
        albedo *= lerp(0.90, 1.06, smoothstep(0.22,0.48,weave));
        roughness = 0.94;
    } else if (material == 13u) { // worn dark leather / rope belt
        const float scuff = smoothstep(0.70,0.93,fine);
        albedo = lerp(float3(0.070,0.028,0.010), float3(0.31,0.135,0.035), medium * 0.72 + 0.18);
        albedo = lerp(albedo, float3(0.43,0.245,0.095), scuff * 0.20);
        roughness = 0.88;
    } else if (material == 14u) { // chipped dark steel gear
        const float edgeFleck = smoothstep(0.76,0.96,fine);
        albedo = lerp(float3(0.085,0.095,0.11), float3(0.29,0.32,0.34), medium * 0.48 + 0.18);
        albedo = lerp(albedo, float3(0.54,0.49,0.39), edgeFleck * 0.22);
        roughness = 0.39;
        metallic = 0.72;
    } else if (material == 15u) { // chunky layered dark-brown hair
        const float strand = hash31(floor(input.worldPosition * 24.0) + 7.0);
        const float3 hairDeep = float3(0.026,0.010,0.004);
        const float3 hairBrown = float3(0.155,0.052,0.014);
        const float3 hairWarm = float3(0.245,0.100,0.027);
        albedo = lerp(hairDeep, hairBrown, 0.34 + medium * 0.44);
        albedo = lerp(albedo, hairWarm, step(0.82, strand) * 0.26);
        roughness = 0.92;
    } else if (material == 16u) { // readable warm eye white
        albedo = float3(0.82,0.80,0.72);
        roughness = 0.58;
    } else if (material == 17u) { // saturated blue iris, a key reference feature
        const float irisCell = hash31(floor(input.worldPosition * 30.0) + 3.0);
        albedo = lerp(float3(0.008,0.115,0.33), float3(0.025,0.48,0.92), 0.58 + irisCell * 0.34);
        roughness = 0.32;
    } else if (material == 18u) { // minimal olive-brown rough survival loincloth
        const float fiber = hash31(floor(input.worldPosition * 22.0) + 19.0);
        const float3 clothDark = float3(0.075,0.060,0.014);
        const float3 clothOlive = float3(0.235,0.205,0.050);
        albedo = lerp(clothDark, clothOlive, 0.30 + medium * 0.42 + fiber * 0.12);
        roughness = 0.98;
    }

    const float3 n = normalize(input.normal);
    const float3 sunDirection = normalize(float3(-0.52, 0.63, -0.44));
    const float direct = saturate(dot(n, sunDirection));
    const float sky = saturate(n.y * 0.5 + 0.5);
    const float3 warmSun = float3(1.0,0.82,0.60);
    const float3 coolAmbient = float3(0.20,0.30,0.43);
    float3 color = albedo * (warmSun * (0.15 + direct * 0.82) + coolAmbient * (0.11 + sky * 0.18));

    if (metallic > 0.0) {
        const float rim = pow(1.0 - saturate(abs(n.z)), 3.0);
        color += float3(0.52,0.62,0.76) * rim * metallic * (1.0 - roughness) * 0.34;
    }

    color = acesTone(color * 0.84);
    color = pow(max(color, 0.0), 1.0 / 2.2);
    return float4(color, 1.0);
}
