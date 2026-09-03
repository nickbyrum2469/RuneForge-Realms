// RuneForge 0.6.1 terrain material correction.
//
// Keep the existing shader's camera/sky/HUD and unrelated material behavior intact, but wrap its
// terrain material functions so grass + rooted soil can be corrected without duplicating renderer
// ownership or touching stone/bark/water/character systems in this focused pass.
#define sampleMaterial sampleMaterialLegacy
#define materialHeight materialHeightLegacy
#define detailNormal detailNormalLegacy
#define PSMain PSMainLegacy
#include "voxel_scene_base.hlsl"
#undef PSMain
#undef detailNormal
#undef materialHeight
#undef sampleMaterial

MaterialSample sampleSurface061(uint material, float3 p, float3 n) {
    if (material > 2u && material != 19u) return sampleMaterialLegacy(material, p, n);

    MaterialSample s;
    s.albedo = float3(0.5, 0.5, 0.5);
    s.roughness = 0.98;
    s.relief = 0.0;
    s.cavity = 0.0;
    s.emissive = 0.0;
    s.alpha = 1.0;

    float2 uv = surfaceUv(p, n);

    if (material == 0u) {
        // Grass top: irregular cellular color/cavity field instead of a global axis-aligned texel
        // grid. Physical surface addresses remain stable; the color field can no longer form long
        // straight lines across neighboring blocks.
        float region = fbm2(uv * 0.55 + float2(7.3, 11.1));
        float2 microUv = uv * 16.0;
        float2 cellular = cellular2(microUv);
        float2 microId = floor(microUv);
        float cellRnd = cellular.y;
        float cellRnd2 = hash21(microId * 1.91 + 47.0);
        float cellRnd3 = hash21(microId * 3.17 + 91.0);
        float boundary = 1.0 - smoothstep(0.025, 0.105, cellular.x);

        float3 deep = float3(0.015, 0.060, 0.004);
        float3 middle = float3(0.050, 0.185, 0.012);
        float3 lush = float3(0.145, 0.355, 0.026);
        float3 tip = float3(0.300, 0.520, 0.055);
        s.albedo = lerp(deep, middle, saturate(0.28 + region * 0.46 + cellRnd * 0.14));
        s.albedo = lerp(s.albedo, lush, step(0.43, cellRnd) * 0.54);
        s.albedo = lerp(s.albedo, tip, step(0.88, cellRnd2) * 0.34);
        s.albedo *= 1.0 - boundary * (0.025 + step(0.76, cellRnd3) * 0.035);
        s.roughness = 0.98;
        s.relief = (cellRnd - 0.5) * 0.082 + step(0.82, cellRnd2) * 0.050 - boundary * 0.016;
        s.cavity = boundary * (0.025 + step(0.80, cellRnd3) * 0.030);
        return s;
    }

    if (material == 19u) {
        // Dedicated thin root fiber. Geometry supplies the branching silhouette; this only gives
        // that fiber a readable warm/dark material range.
        float fiber = hash31(floor(p * 42.0) + float3(17.0, 31.0, 53.0));
        float fiberFine = noise3(p * 28.0 + 9.0);
        float3 rootDark = float3(0.095, 0.030, 0.006);
        float3 rootMid = float3(0.260, 0.105, 0.024);
        float3 rootWarm = float3(0.460, 0.235, 0.075);
        s.albedo = lerp(rootDark, rootMid, 0.28 + fiber * 0.58);
        s.albedo = lerp(s.albedo, rootWarm, step(0.82, fiberFine) * 0.38);
        s.roughness = 0.99;
        s.relief = (fiberFine - 0.5) * 0.035;
        s.cavity = 0.025 + (1.0 - fiber) * 0.035;
        return s;
    }

    // Dirt/grass-side share one small-clod language. Rows are independently staggered and columns
    // receive a smaller vertical warp, retaining voxel construction without a perfect brick grid.
    float2 clodUv = uv * 16.0;
    float row = floor(clodUv.y);
    clodUv.x += (hash11(row * 13.7 + 5.3) - 0.5) * 0.72;
    float column = floor(clodUv.x);
    clodUv.y += (hash11(column * 9.17 + 21.0) - 0.5) * 0.24;
    float2 clodId = floor(clodUv);
    float2 clodLocal = frac(clodUv);
    float clodRnd = hash21(clodId + 23.0);
    float clodRnd2 = hash21(clodId * 2.31 + 83.0);
    float cluster = fbm2(uv * 2.15 + float2(13.0, 29.0));
    float edge = min(min(clodLocal.x, 1.0 - clodLocal.x), min(clodLocal.y, 1.0 - clodLocal.y));
    float rawJoint = 1.0 - smoothstep(0.025, 0.080, edge);
    float jointGate = 0.30 + step(0.52, hash21(clodId * 4.17 + 131.0)) * 0.70;
    float joint = rawJoint * jointGate;
    float mineral = step(0.955, clodRnd2);

    float3 soilDeep = float3(0.022, 0.007, 0.002);
    float3 soilUmber = float3(0.070, 0.022, 0.006);
    float3 soilBrown = float3(0.145, 0.055, 0.014);
    float3 soilWarm = float3(0.255, 0.115, 0.036);
    float3 soilLight = float3(0.390, 0.220, 0.090);
    float tone = saturate(clodRnd * 0.72 + cluster * 0.34);
    float3 dirt = lerp(soilDeep, soilUmber, smoothstep(0.04, 0.34, tone));
    dirt = lerp(dirt, soilBrown, smoothstep(0.26, 0.64, tone));
    dirt = lerp(dirt, soilWarm, smoothstep(0.58, 0.86, tone) * 0.72);
    dirt = lerp(dirt, soilLight, step(0.91, clodRnd2) * 0.48);
    dirt *= 1.0 - joint * 0.18;
    dirt = lerp(dirt, float3(0.36, 0.35, 0.32), mineral * 0.78);

    if (material == 2u) {
        s.albedo = dirt;
        s.roughness = 0.99 - mineral * 0.08;
        s.relief = (clodRnd - 0.5) * 0.145 - joint * 0.085 + mineral * 0.155;
        s.cavity = joint * 0.19;
        return s;
    }

    // Grass-side: same soil plus an irregular short turf lip. Roots are no longer painted into
    // this material; near-camera roots are explicit fibers, removing the 0.6.0 double-root effect.
    float localY = frac(p.y + 0.001);
    float columnRnd = hash21(float2(floor(uv.x * 16.0), floor(p.x + p.z) + 29.0));
    float broadLip = noise2(float2(uv.x * 2.2 + 17.0, p.x * 0.13 + p.z * 0.17 + 31.0));
    float turfFloor = 0.735 + columnRnd * 0.135 + (broadLip - 0.5) * 0.075;
    float turf = smoothstep(turfFloor - 0.020, turfFloor + 0.020, localY);
    float turfRnd = hash21(clodId * 1.73 + 211.0);
    float turfRnd2 = hash21(clodId * 3.11 + 313.0);
    float3 turfDeep = float3(0.012, 0.055, 0.004);
    float3 turfMid = float3(0.043, 0.165, 0.011);
    float3 turfLush = float3(0.130, 0.330, 0.024);
    float3 turfTip = float3(0.275, 0.490, 0.052);
    float3 turfColor = lerp(turfDeep, turfMid, 0.30 + turfRnd * 0.58);
    turfColor = lerp(turfColor, turfLush, step(0.48, turfRnd) * 0.52);
    turfColor = lerp(turfColor, turfTip, step(0.90, turfRnd2) * 0.34);
    turfColor *= 1.0 - joint * 0.09;

    s.albedo = lerp(dirt, turfColor, turf);
    s.roughness = 0.99;
    s.relief = (clodRnd - 0.5) * 0.125 - joint * 0.070 + turf * 0.052;
    s.cavity = joint * 0.16 + (1.0 - turf) * rawJoint * 0.025;
    return s;
}

float surfaceHeight061(uint material, float3 p, float3 n) {
    return sampleSurface061(material, p, n).relief;
}

float3 detailNormal061(uint material, float3 p, float3 n) {
    if (material > 2u && material != 19u) return detailNormalLegacy(material, p, n);
    float3 tangent = abs(n.y) < 0.92 ? normalize(cross(float3(0,1,0), n)) : float3(1,0,0);
    float3 bitangent = normalize(cross(n, tangent));
    float epsilon = 0.031;
    float h = surfaceHeight061(material, p, n);
    float ht = surfaceHeight061(material, p + tangent * epsilon, n);
    float hb = surfaceHeight061(material, p + bitangent * epsilon, n);
    float strength = material == 0u ? 0.34 : (material == 1u ? 0.43 : (material == 2u ? 0.52 : 0.24));
    return normalize(n - tangent * ((ht - h) / epsilon) * strength -
                     bitangent * ((hb - h) / epsilon) * strength);
}

float4 PSMain(VSOutput input) : SV_Target0 {
    uint baseMaterial = input.material & 0xffu;
    uint damageStage = (input.material >> 8u) & 0xffu;
    float3 geometricNormal = normalize(input.normal);
    MaterialSample material = sampleSurface061(baseMaterial, input.worldPosition, geometricNormal);

    if (baseMaterial == 6u) clip(material.alpha - 0.46);

    float damage = organicDamage(input.worldPosition, geometricNormal, baseMaterial, damageStage);
    if (damage > 0.0) {
        float stage = saturate((float)damageStage / 5.0);
        material.albedo *= 1.0 - damage * (0.18 + stage * 0.38);
        material.cavity = saturate(material.cavity + damage * (0.36 + stage * 0.42));
        material.roughness = saturate(material.roughness + damage * 0.08);
        material.relief -= damage * 0.12;
    }

    float3 n = detailNormal061(baseMaterial, input.worldPosition, geometricNormal);
    const float3 sunDirection = normalize(float3(-0.52, 0.63, -0.44));
    float direct = max(dot(n, sunDirection), 0.0);
    float skyLight = saturate(n.y * 0.52 + 0.48);
    float3 viewDirection = normalize(float3(pushData.eyeX, pushData.eyeY, pushData.eyeZ) - input.worldPosition);
    float3 halfVector = normalize(sunDirection + viewDirection);
    float specular = pow(saturate(dot(n, halfVector)), lerp(10.0, 68.0, 1.0 - material.roughness));
    specular *= (1.0 - material.roughness) * 0.20;

    bool soilFamily = baseMaterial <= 2u || baseMaterial == 19u;
    float cavityStrength = soilFamily ? 0.76 : 0.62;
    float ao = 1.0 - saturate(material.cavity) * cavityStrength;
    float3 warmSun = float3(1.00, 0.88, 0.70);
    float3 coolSky = float3(0.34, 0.47, 0.66);
    float3 lighting = warmSun * (0.16 + direct * 0.90) + coolSky * (0.10 + skyLight * 0.22);
    float3 lit = material.albedo * lighting * ao;
    lit += warmSun * specular;
    lit += material.albedo * material.emissive;

    // Reference matching correction is local to grass/dirt/root. This is not a global lighting or
    // exposure pass and deliberately leaves every unrelated material unchanged.
    if (soilFamily) {
        float luma = dot(lit, float3(0.2126, 0.7152, 0.0722));
        float saturation = baseMaterial == 0u ? 1.28 : (baseMaterial == 1u ? 1.20 : 1.16);
        float exposure = baseMaterial == 0u ? 0.86 : (baseMaterial == 1u ? 0.83 : (baseMaterial == 2u ? 0.82 : 0.88));
        lit = max(lerp(luma.xxx, lit, saturation), 0.0) * exposure;
    }

    if (abs(pushData.targetActive) > 0.5) {
        float3 interior = input.worldPosition - geometricNormal * 0.0025;
        float3 blockCoord = floor(interior);
        float3 targetCoord = float3(pushData.targetBlockX, pushData.targetBlockY, pushData.targetBlockZ);
        float isTarget = 1.0 - step(0.01, length(blockCoord - targetCoord));
        if (isTarget > 0.5) {
            float2 faceUv = frac(surfaceUv(input.worldPosition, geometricNormal));
            float edgeDistance = min(min(faceUv.x, 1.0 - faceUv.x), min(faceUv.y, 1.0 - faceUv.y));
            float border = 1.0 - smoothstep(0.018, 0.050, edgeDistance);
            float3 reachColor = pushData.targetActive > 0.0 ? float3(0.20, 0.95, 0.72)
                                                           : float3(1.00, 0.30, 0.12);
            lit = lerp(lit, reachColor * 1.28, border * 0.78);
        }
    }

    float fog = smoothstep(68.0, 205.0, max(input.depth, 0.0));
    float3 fogColor = float3(0.39, 0.48, 0.56);
    lit = lerp(lit, fogColor, fog * 0.62);
    lit = acesTone(lit * 0.86);
    lit = pow(max(lit, 0.0), 1.0 / 2.2);
    return float4(lit, 1.0);
}
