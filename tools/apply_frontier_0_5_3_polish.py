from pathlib import Path
import re


def replace_once(path: str, old: str, new: str) -> None:
    p = Path(path)
    text = p.read_text()
    if old not in text:
        raise SystemExit(f"missing marker in {path}: {old[:120]!r}")
    p.write_text(text.replace(old, new, 1))


def replace_regex(path: str, pattern: str, replacement: str) -> None:
    p = Path(path)
    text = p.read_text()
    updated, count = re.subn(pattern, replacement, text, count=1, flags=re.S)
    if count != 1:
        raise SystemExit(f"expected one match in {path}, got {count}: {pattern[:120]!r}")
    p.write_text(updated)


# -----------------------------------------------------------------------------
# P0 UI COMPOSITION
# -----------------------------------------------------------------------------
# Real RX 7900 XTX evidence showed the state entered Pause/Inventory correctly while the child D2D
# HWND remained invisible over the Vulkan parent. Do not put D2D and Vulkan into one composition
# tree. The native modal is now an owned borderless top-level popup covering the Vulkan client rect.
new_create_overlay = r'''bool NativeWindow::createUiOverlay() {
    RECT client{};
    GetClientRect(hwnd_, &client);
    const int width = std::max<LONG>(client.right - client.left, 1);
    const int height = std::max<LONG>(client.bottom - client.top, 1);

    // IMPORTANT: this must be an owned top-level popup, not WS_CHILD. On the real AMD/Vulkan
    // machine a child Direct2D HwndRenderTarget could own input/state yet never visibly composite
    // above the Vulkan swapchain. An owned popup gives DWM two independent top-level surfaces while
    // keeping the modal attached to/minimized with the game window.
    uiOverlayHwnd_ = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        overlayWindowClass,
        L"RuneForge Native UI Overlay",
        WS_POPUP | WS_CLIPCHILDREN,
        0, 0, width, height,
        hwnd_, nullptr, instance_, this);
    if (!uiOverlayHwnd_) return false;
    resizeUiOverlay(static_cast<unsigned>(width), static_cast<unsigned>(height));
    ShowWindow(uiOverlayHwnd_, SW_HIDE);
    return true;
}

'''
replace_regex(
    "src/platform/windows/NativeWindow.cpp",
    r"bool NativeWindow::createUiOverlay\(\) \{.*?\n\}\n\n(?=int NativeWindow::run)",
    new_create_overlay,
)

replace_once(
    "src/platform/windows/NativeWindow.cpp",
    "        case WM_SIZE: {\n",
    r'''        case WM_MOVE:
            if (uiOverlayHwnd_ && uiState_.nativeOverlayVisible()) {
                RECT client{};
                GetClientRect(hwnd_, &client);
                resizeUiOverlay(static_cast<unsigned>(std::max<LONG>(client.right - client.left, 1)),
                                static_cast<unsigned>(std::max<LONG>(client.bottom - client.top, 1)));
            }
            return 0;

        case WM_SIZE: {
''',
)

new_overlay_functions = r'''void NativeWindow::resizeUiOverlay(unsigned width, unsigned height) {
    if (!uiOverlayHwnd_ || !hwnd_) return;

    RECT client{};
    GetClientRect(hwnd_, &client);
    POINT topLeft{client.left, client.top};
    POINT bottomRight{client.right, client.bottom};
    ClientToScreen(hwnd_, &topLeft);
    ClientToScreen(hwnd_, &bottomRight);

    const unsigned actualWidth = static_cast<unsigned>(std::max<LONG>(bottomRight.x - topLeft.x, 1));
    const unsigned actualHeight = static_cast<unsigned>(std::max<LONG>(bottomRight.y - topLeft.y, 1));
    (void)width;
    (void)height;

    SetWindowPos(uiOverlayHwnd_, HWND_TOP,
                 topLeft.x, topLeft.y,
                 static_cast<int>(actualWidth), static_cast<int>(actualHeight),
                 SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    if (inventoryPainter_) inventoryPainter_->resize(actualWidth, actualHeight);
    if (pausePainter_) pausePainter_->resize(actualWidth, actualHeight);
    if (settingsPainter_) settingsPainter_->resize(actualWidth, actualHeight);
}

void NativeWindow::showUiOverlay() {
    if (!uiOverlayHwnd_ || !hwnd_) return;
    RECT client{};
    GetClientRect(hwnd_, &client);
    resizeUiOverlay(static_cast<unsigned>(std::max<LONG>(client.right - client.left, 1)),
                    static_cast<unsigned>(std::max<LONG>(client.bottom - client.top, 1)));

    ShowWindow(uiOverlayHwnd_, SW_SHOWNORMAL);
    SetWindowPos(uiOverlayHwnd_, HWND_TOP, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_SHOWWINDOW);
    SetActiveWindow(uiOverlayHwnd_);
    SetFocus(uiOverlayHwnd_);

    // Force the first D2D frame now. Waiting for a later incidental paint was precisely how a modal
    // could be logically active while the last Vulkan image remained all the user saw.
    InvalidateRect(uiOverlayHwnd_, nullptr, TRUE);
    RedrawWindow(uiOverlayHwnd_, nullptr, nullptr,
                 RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW | RDW_ALLCHILDREN);
}

void NativeWindow::hideUiOverlay() {
    if (uiOverlayHwnd_) ShowWindow(uiOverlayHwnd_, SW_HIDE);
    if (hwnd_) {
        SetActiveWindow(hwnd_);
        SetFocus(hwnd_);
    }
}

'''
replace_regex(
    "src/platform/windows/NativeWindow.cpp",
    r"void NativeWindow::resizeUiOverlay\(unsigned width, unsigned height\) \{.*?\n\}\n\nvoid NativeWindow::showUiOverlay\(\) \{.*?\n\}\n\nvoid NativeWindow::hideUiOverlay\(\) \{.*?\n\}\n\n(?=void NativeWindow::syncInteractionState)",
    new_overlay_functions,
)

# -----------------------------------------------------------------------------
# CROSSHAIR-LOCKED FIRST PERSON STRIKE
# -----------------------------------------------------------------------------
replace_once(
    "src/render/scene/FirstPersonBodyBuilder.cpp",
    "    const float strikeDepth = 0.48f + targetRatio * 0.20f;\n",
    "    const float strikeDepth = 0.46f + targetRatio * 0.44f;\n",
)
replace_once(
    "src/render/scene/FirstPersonBodyBuilder.cpp",
    "    const Vec3 strike = eye + forward * strikeDepth + right * 0.012f - up * 0.020f;\n",
    "    // At impact the visible knuckles sit exactly on the center camera ray. Distance changes only\n"
    "    // how far forward the hand travels; it never introduces a lower/side pseudo-impact.\n"
    "    const Vec3 strike = eye + forward * strikeDepth;\n",
)

# -----------------------------------------------------------------------------
# REFERENCE-DRIVEN TERRAIN MATERIALS
# -----------------------------------------------------------------------------
materials = r'''    if (material == 0) { // dense grass top: dark turf bed + discrete bright micro clumps
        float2 macroId = floor(uv * 5.0);
        float2 cellId = floor(uv * 22.0);
        float2 cellLocal = frac(uv * 22.0);
        float macroRnd = hash21(macroId + 3.7);
        float cellRnd = hash21(cellId + 19.1);
        float cellRnd2 = hash21(cellId * 1.91 + 47.0);
        float seam = 1.0 - smoothstep(0.035, 0.105,
            min(min(cellLocal.x, 1.0 - cellLocal.x), min(cellLocal.y, 1.0 - cellLocal.y)));
        float3 deep = float3(0.030, 0.125, 0.020);
        float3 middle = float3(0.105, 0.315, 0.045);
        float3 lush = float3(0.245, 0.515, 0.075);
        float3 tip = float3(0.405, 0.655, 0.115);
        s.albedo = lerp(deep, middle, 0.28 + macroRnd * 0.55);
        s.albedo = lerp(s.albedo, lush, step(0.42, cellRnd) * 0.46);
        s.albedo = lerp(s.albedo, tip, step(0.88, cellRnd2) * 0.30);
        s.albedo *= 1.0 - seam * 0.12;
        s.roughness = 0.97;
        s.relief = (cellRnd - 0.5) * 0.075 + step(0.80, cellRnd2) * 0.055 - seam * 0.025;
        s.cavity = seam * 0.07;
    } else if (material == 1) { // grass side: irregular hanging turf over layered soil and short roots
        float localY = frac(p.y + 0.001);
        float2 soilId = floor(uv * float2(13.0, 12.0));
        float2 soilLocal = frac(uv * float2(13.0, 12.0));
        float soilRnd = hash21(soilId + 11.0);
        float soilRnd2 = hash21(soilId * 2.13 + 71.0);
        float seam = 1.0 - smoothstep(0.025, 0.085,
            min(min(soilLocal.x, 1.0 - soilLocal.x), min(soilLocal.y, 1.0 - soilLocal.y)));
        float columnRnd = hash21(float2(floor(uv.x * 13.0), floor(p.x + p.z) + 29.0));
        float turfFloor = 0.70 + columnRnd * 0.18;
        float turf = smoothstep(turfFloor - 0.025, turfFloor + 0.025, localY);
        float rootColumn = step(0.88, hash21(float2(floor(uv.x * 15.0), floor(p.x + p.z) + 53.0)));
        float rootStart = 0.32 + hash21(float2(floor(uv.x * 15.0), 91.0)) * 0.28;
        float root = rootColumn * step(rootStart, localY) * (1.0 - turf);
        float3 soilDark = float3(0.070, 0.028, 0.010);
        float3 soilMid = float3(0.235, 0.095, 0.025);
        float3 soilWarm = float3(0.375, 0.185, 0.060);
        float3 dirt = lerp(soilDark, soilMid, 0.30 + soilRnd * 0.55);
        dirt = lerp(dirt, soilWarm, step(0.78, soilRnd2) * 0.30);
        dirt *= 1.0 - seam * 0.20;
        float3 turfDark = float3(0.035, 0.155, 0.020);
        float3 turfLight = float3(0.245, 0.500, 0.075);
        float3 turfColor = lerp(turfDark, turfLight, 0.30 + soilRnd2 * 0.58);
        s.albedo = lerp(dirt, turfColor, turf);
        s.albedo = lerp(s.albedo, float3(0.42, 0.245, 0.085), root * 0.52);
        s.roughness = 0.98;
        s.relief = (soilRnd - 0.5) * 0.10 + turf * 0.055 + root * 0.045 - seam * 0.035;
        s.cavity = seam * 0.13 + root * 0.08;
    } else if (material == 2) { // dirt: assembled soil clods, dark joints, embedded mineral chips and roots
        float2 clodUv = uv * 12.0;
        float row = floor(clodUv.y);
        clodUv.x += hash11(row * 13.7) * 0.62;
        float2 clodId = floor(clodUv);
        float2 clodLocal = frac(clodUv);
        float clodRnd = hash21(clodId + 23.0);
        float clodRnd2 = hash21(clodId * 2.31 + 83.0);
        float edge = min(min(clodLocal.x, 1.0 - clodLocal.x), min(clodLocal.y, 1.0 - clodLocal.y));
        float joint = 1.0 - smoothstep(0.035, 0.105, edge);
        float mineral = step(0.94, clodRnd2);
        float root = step(0.91, hash21(float2(clodId.x, floor(clodId.y * 0.37)) + 117.0)) *
                     step(0.25, clodLocal.y) * step(clodLocal.y, 0.80);
        float3 deep = float3(0.060, 0.024, 0.009);
        float3 brown = float3(0.215, 0.082, 0.021);
        float3 warm = float3(0.390, 0.175, 0.055);
        s.albedo = lerp(deep, brown, 0.30 + clodRnd * 0.62);
        s.albedo = lerp(s.albedo, warm, step(0.73, clodRnd2) * 0.28);
        s.albedo *= 1.0 - joint * 0.27;
        s.albedo = lerp(s.albedo, float3(0.38, 0.36, 0.31), mineral * 0.72);
        s.albedo = lerp(s.albedo, float3(0.48, 0.29, 0.11), root * 0.48);
        s.roughness = 0.98 - mineral * 0.08;
        s.relief = (clodRnd - 0.5) * 0.14 - joint * 0.10 + mineral * 0.17 + root * 0.055;
        s.cavity = joint * 0.21 + root * 0.07;
    } else if (material == 3) { // stone: stacked irregular slabs with dark mortar-like creases and chips
        float2 slabUv = uv * float2(5.4, 6.0);
        float slabRow = floor(slabUv.y);
        slabUv.x += hash11(slabRow * 7.91) * 0.74;
        float2 slabId = floor(slabUv);
        float2 slabLocal = frac(slabUv);
        float slabRnd = hash21(slabId + 17.0);
        float slabRnd2 = hash21(slabId * 1.91 + 101.0);
        float edge = min(min(slabLocal.x, 1.0 - slabLocal.x), min(slabLocal.y, 1.0 - slabLocal.y));
        float joint = 1.0 - smoothstep(0.035, 0.105, edge);
        float chip = step(0.90, hash21(floor(uv * 19.0) + 211.0));
        float mineral = step(0.965, hash21(floor(uv * 23.0) * 2.77 + 311.0));
        float3 stoneDeep = float3(0.105, 0.115, 0.120);
        float3 stoneMid = float3(0.285, 0.295, 0.295);
        float3 stoneLight = float3(0.475, 0.465, 0.425);
        s.albedo = lerp(stoneDeep, stoneMid, 0.32 + slabRnd * 0.50);
        s.albedo = lerp(s.albedo, stoneLight, step(0.72, slabRnd2) * 0.27);
        s.albedo *= 1.0 - joint * 0.38;
        s.albedo *= lerp(1.0, 0.78, chip);
        s.albedo = lerp(s.albedo, float3(0.66, 0.64, 0.58), mineral * 0.50);
        s.roughness = 0.92 - mineral * 0.09;
        s.relief = (slabRnd - 0.5) * 0.18 - joint * 0.18 + chip * 0.07 + mineral * 0.10;
        s.cavity = joint * 0.34 + chip * 0.08;
    } else if (material == 4) { // bark: irregular vertical plates, not horizontal brick rows
        float column = floor(uv.x * 7.0);
        float columnRnd = hash11(column * 19.31 + 7.0);
        float yCoord = uv.y * (3.0 + columnRnd * 2.0) + columnRnd * 2.7;
        float2 plateUv = float2(uv.x * 7.0, yCoord);
        float2 plateId = floor(plateUv);
        float2 plateLocal = frac(plateUv);
        float plateRnd = hash21(plateId + 13.0);
        float plateRnd2 = hash21(plateId * 2.31 + 53.0);
        float verticalEdge = min(plateLocal.x, 1.0 - plateLocal.x);
        float verticalFissure = 1.0 - smoothstep(0.045, 0.115, verticalEdge);
        float brokenEnd = (1.0 - smoothstep(0.020, 0.080, min(plateLocal.y, 1.0 - plateLocal.y))) *
                          step(0.55, plateRnd2);
        float chip = step(0.91, hash21(floor(uv * 18.0) + 91.0));
        float3 barkBlack = float3(0.035, 0.012, 0.004);
        float3 barkBrown = float3(0.185, 0.060, 0.013);
        float3 barkWarm = float3(0.335, 0.145, 0.035);
        s.albedo = lerp(barkBlack, barkBrown, 0.30 + plateRnd * 0.55);
        s.albedo = lerp(s.albedo, barkWarm, step(0.76, plateRnd2) * 0.25);
        s.albedo *= 1.0 - verticalFissure * 0.46 - brokenEnd * 0.20;
        s.albedo *= lerp(0.90, 1.05, chip);
        s.roughness = 0.97;
        s.relief = (plateRnd - 0.5) * 0.15 - verticalFissure * 0.22 - brokenEnd * 0.08 + chip * 0.05;
        s.cavity = verticalFissure * 0.42 + brokenEnd * 0.14;
    } else if (material == 5) { // cut wood: squared voxel growth rings + radial drying cracks
        float2 q = frac(uv) - 0.5;
        float squareRadius = max(abs(q.x), abs(q.y));
        float ringIndex = floor((squareRadius + hash21(floor(uv) + 7.0) * 0.012) * 20.0);
        float ringTone = hash11(ringIndex * 11.7 + floor(uv.x + uv.y) * 3.1);
        float2 texelId = floor(uv * 18.0);
        float texel = hash21(texelId + 59.0);
        float heart = 1.0 - smoothstep(0.035, 0.120, squareRadius);
        float crackA = 1.0 - smoothstep(0.010, 0.025, abs(q.y - q.x * 0.42));
        float crackB = 1.0 - smoothstep(0.009, 0.022, abs(q.x + q.y * 0.30));
        float radialMask = smoothstep(0.12, 0.46, squareRadius);
        float crack = max(crackA * step(0.52, q.x), crackB * step(0.48, -q.y)) * radialMask;
        float3 heartWood = float3(0.115, 0.038, 0.008);
        float3 wood = float3(0.405, 0.205, 0.052);
        float3 fresh = float3(0.665, 0.415, 0.125);
        s.albedo = lerp(heartWood, wood, 0.28 + ringTone * 0.58);
        s.albedo = lerp(s.albedo, fresh, step(0.76, texel) * 0.23);
        s.albedo *= 1.0 - heart * 0.15 - crack * 0.42;
        s.roughness = 0.91;
        s.relief = (ringTone - 0.5) * 0.13 + step(0.83, texel) * 0.05 - crack * 0.12;
        s.cavity = crack * 0.28 + heart * 0.07;
    } else if (material == 6) { // leaves: deep cubical clusters with bright sun tips and pixel holes
        float2 cellId = floor(uv * 14.0);
        float cell = hash21(cellId + 19.0);
        float cell2 = hash21(cellId * 2.17 + 73.0);
        float clump = hash21(floor(p * 5.0).xz + floor(p.y * 5.0) * 7.0);
        float3 leafDeep = float3(0.015, 0.080, 0.014);
        float3 leafMid = float3(0.055, 0.235, 0.035);
        float3 leafSun = float3(0.230, 0.470, 0.075);
        float3 leafTip = float3(0.390, 0.610, 0.105);
        s.albedo = lerp(leafDeep, leafMid, 0.28 + clump * 0.55);
        s.albedo = lerp(s.albedo, leafSun, step(0.57, cell) * 0.35);
        s.albedo = lerp(s.albedo, leafTip, step(0.90, cell2) * 0.26);
        s.roughness = 0.92;
        s.relief = (cell - 0.5) * 0.12 + step(0.84, cell2) * 0.060;
        s.cavity = step(cell, 0.10) * 0.08;
        s.alpha = step(0.085, cell2);
    } else if (material == 7) {'''
replace_regex(
    "shaders/voxel_scene.hlsl",
    r"    if \(material == 0\) \{.*?    \} else if \(material == 7\) \{",
    materials,
)

print("0.5.3 UI/material/crosshair polish patch applied")
