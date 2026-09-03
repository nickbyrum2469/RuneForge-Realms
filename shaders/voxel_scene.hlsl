// RuneForge 0.6.3 grass + rooted-soil reference-shape correction.
//
// The physical SurfaceRelief mesh now owns visible micro construction. This shader deliberately
// stops drawing a second fake 16x16 material grid or aggressive pseudo-height over that geometry.
// Unrelated materials still delegate to the canonical base shader unchanged.
#define sampleMaterial sampleMaterialLegacy
#define materialHeight materialHeightLegacy
#define detailNormal detailNormalLegacy
#define PSMain PSMainLegacy
#include "voxel_scene_base.hlsl"
#undef PSMain
#undef detailNormal
#undef materialHeight
#undef sampleMaterial

MaterialSample sampleSurface062(uint material, float3 p, float3 n) {
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
        // Grass top: continuous multi-scale organic variation only. No floor()/cell boundary mask
        // exists here; the real 16x16 turf geometry provides the voxel construction and the R2
        // anchor set provides the visible turf pieces. This prevents a second shader grid from aligning
        // with either one.
        float broad = fbm2(uv * 0.52 + float2(7.3, 11.1));
        float medium = fbm2(uv * 3.7 + float2(31.0, 19.0));
        float fine = noise2(uv * 13.5 + float2(71.0, 43.0));
        float glint = smoothstep(0.83, 0.97, noise2(uv * 27.0 + float2(113.0, 89.0)));

        float3 deep = float3(0.016, 0.050, 0.0035);
        float3 olive = float3(0.052, 0.125, 0.0065);
        float3 meadow = float3(0.105, 0.215, 0.0120);
        float3 warmTip = float3(0.205, 0.345, 0.0270);
        float tone = saturate(broad * 0.48 + medium * 0.36 + fine * 0.16);
        s.albedo = lerp(deep, olive, smoothstep(0.14, 0.46, tone));
        s.albedo = lerp(s.albedo, meadow, smoothstep(0.40, 0.72, tone) * 0.72);
        s.albedo = lerp(s.albedo, warmTip, glint * 0.28);
        s.roughness = 0.99;
        s.relief = (medium - 0.5) * 0.018 + (fine - 0.5) * 0.008;
        s.cavity = saturate((0.46 - medium) * 0.075 + (0.38 - fine) * 0.030);
        return s;
    }

    if (material == 19u) {
        // Root fibers are real narrow prisms. Keep the material quiet so the fiber stays crisp and
        // does not acquire noisy fake thickness at grazing angles.
        float fiber = fbm3(p * 7.5 + 9.0);
        float fine = noise3(p * 18.0 + 21.0);
        float3 rootDark = float3(0.070, 0.024, 0.005);
        float3 rootMid = float3(0.185, 0.075, 0.016);
        float3 rootWarm = float3(0.320, 0.155, 0.045);
        s.albedo = lerp(rootDark, rootMid, smoothstep(0.20, 0.70, fiber));
        s.albedo = lerp(s.albedo, rootWarm, smoothstep(0.82, 0.96, fine) * 0.32);
        s.roughness = 0.995;
        s.relief = (fine - 0.5) * 0.010;
        s.cavity = 0.025 + (1.0 - fiber) * 0.035;
        return s;
    }

    // Soil: the mesher now constructs actual depth with a connected micro-prism shell. Color is
    // therefore continuous clod-scale earth variation rather than a second flat brick/checker map.
    float broadSoil = fbm2(uv * 0.70 + float2(13.0, 29.0));
    float clod = fbm2(uv * 3.25 + float2(53.0, 17.0));
    float grain = noise2(uv * 11.5 + float2(97.0, 61.0));
    float darkPocket = smoothstep(0.58, 0.88, 1.0 - clod) *
                       smoothstep(0.52, 0.82, 1.0 - grain);

    float3 soilDeep = float3(0.020, 0.0065, 0.0018);
    float3 soilUmber = float3(0.060, 0.019, 0.0045);
    float3 soilBrown = float3(0.125, 0.047, 0.0105);
    float3 soilWarm = float3(0.230, 0.105, 0.0300);
    float tone = saturate(broadSoil * 0.30 + clod * 0.52 + grain * 0.18);
    float3 dirt = lerp(soilDeep, soilUmber, smoothstep(0.08, 0.37, tone));
    dirt = lerp(dirt, soilBrown, smoothstep(0.31, 0.66, tone));
    dirt = lerp(dirt, soilWarm, smoothstep(0.70, 0.94, tone) * 0.48);
    dirt *= 1.0 - darkPocket * 0.24;

    if (material == 2u) {
        s.albedo = dirt;
        s.roughness = 0.995;
        s.relief = (clod - 0.5) * 0.030 + (grain - 0.5) * 0.010;
        s.cavity = darkPocket * 0.18 + saturate(0.40 - clod) * 0.045;
        return s;
    }

    // Grass side: same dimensional soil shell plus a short irregular turf band. The geometry owns
    // the thicker hanging lip; this shader only changes the material family over that upper band.
    float localY = frac(p.y + 0.001);
    float lipNoise = fbm2(float2(uv.x * 2.6 + 17.0, p.x * 0.11 + p.z * 0.15 + 31.0));
    float fineLip = noise2(float2(uv.x * 8.0 + 41.0, p.x * 0.21 + p.z * 0.19 + 67.0));
    float turfFloor = 0.70 + (lipNoise - 0.5) * 0.13 + (fineLip - 0.5) * 0.045;
    float turf = smoothstep(turfFloor - 0.018, turfFloor + 0.018, localY);

    float turfTone = saturate(lipNoise * 0.55 + fineLip * 0.45);
    float3 turfDeep = float3(0.014, 0.047, 0.0032);
    float3 turfOlive = float3(0.050, 0.120, 0.0065);
    float3 turfMeadow = float3(0.105, 0.205, 0.0120);
    float3 turfColor = lerp(turfDeep, turfOlive, smoothstep(0.12, 0.58, turfTone));
    turfColor = lerp(turfColor, turfMeadow, smoothstep(0.52, 0.86, turfTone) * 0.62);

    s.albedo = lerp(dirt, turfColor, turf);
    s.roughness = 0.995;
    s.relief = (clod - 0.5) * 0.025 + turf * (fineLip - 0.5) * 0.010;
    s.cavity = darkPocket * 0.16 + (1.0 - turf) * saturate(0.39 - clod) * 0.040;
    return s;
}

float surfaceHeight062(uint material, float3 p, float3 n) {
    return sampleSurface062(material, p, n).relief;
}

float3 detailNormal062(uint material, float3 p, float3 n) {
    if (material > 2u && material != 19u) return detailNormalLegacy(material, p, n);
    float3 tangent = abs(n.y) < 0.92 ? normalize(cross(float3(0,1,0), n)) : float3(1,0,0);
    float3 bitangent = normalize(cross(n, tangent));
    float epsilon = 0.038;
    float h = surfaceHeight062(material, p, n);
    float ht = surfaceHeight062(material, p + tangent * epsilon, n);
    float hb = surfaceHeight062(material, p + bitangent * epsilon, n);
    // Geometry now provides the important normals. These values intentionally stay low so the
    // shader cannot blur or shimmer the real micro-prism silhouette at grazing angles.
    float strength = material == 0u ? 0.16 : (material == 1u ? 0.18 : (material == 2u ? 0.17 : 0.11));
    return normalize(n - tangent * ((ht - h) / epsilon) * strength -
                     bitangent * ((hb - h) / epsilon) * strength);
}

float4 PSMain(VSOutput input) : SV_Target0 {
    uint baseMaterial = input.material & 0xffu;
    uint damageStage = (input.material >> 8u) & 0xffu;
    float3 geometricNormal = normalize(input.normal);
    MaterialSample material = sampleSurface062(baseMaterial, input.worldPosition, geometricNormal);

    if (baseMaterial == 6u) clip(material.alpha - 0.46);

    float damage = organicDamage(input.worldPosition, geometricNormal, baseMaterial, damageStage);
    if (damage > 0.0) {
        float stage = saturate((float)damageStage / 5.0);
        material.albedo *= 1.0 - damage * (0.18 + stage * 0.38);
        material.cavity = saturate(material.cavity + damage * (0.36 + stage * 0.42));
        material.roughness = saturate(material.roughness + damage * 0.08);
        material.relief -= damage * 0.12;
    }

    float3 normal = detailNormal062(baseMaterial, input.worldPosition, geometricNormal);
    const float3 sunDirection = normalize(float3(-0.52, 0.63, -0.44));
    float direct = max(dot(normal, sunDirection), 0.0);
    float skyLight = saturate(normal.y * 0.52 + 0.48);
    float3 viewDirection = normalize(float3(pushData.eyeX, pushData.eyeY, pushData.eyeZ) - input.worldPosition);
    float3 halfVector = normalize(sunDirection + viewDirection);
    float specular = pow(saturate(dot(normal, halfVector)), lerp(10.0, 68.0, 1.0 - material.roughness));
    specular *= (1.0 - material.roughness) * 0.20;

    bool soilFamily = baseMaterial <= 2u || baseMaterial == 19u;
    float cavityStrength = soilFamily ? 0.82 : 0.62;
    float ao = 1.0 - saturate(material.cavity) * cavityStrength;
    float3 warmSun = float3(1.00, 0.88, 0.70);
    float3 coolSky = float3(0.34, 0.47, 0.66);
    float3 lighting = warmSun * (0.16 + direct * 0.90) + coolSky * (0.10 + skyLight * 0.22);
    float3 lit = material.albedo * lighting * ao;
    lit += warmSun * specular;
    lit += material.albedo * material.emissive;

    // Hardware/reference measurement: 0.6.1 grass/dirt were dramatically brighter than the target.
    // Keep this correction local to the four surface materials rather than hiding geometry problems
    // behind a global exposure change.
    if (soilFamily) {
        float luma = dot(lit, float3(0.2126, 0.7152, 0.0722));
        float saturation = baseMaterial == 0u ? 1.02 :
                           (baseMaterial == 1u ? 1.05 : (baseMaterial == 2u ? 1.16 : 1.12));
        float exposure = baseMaterial == 0u ? 0.66 :
                         (baseMaterial == 1u ? 0.67 : (baseMaterial == 2u ? 0.61 : 0.68));
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
