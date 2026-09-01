# Engine File Boundaries

RuneForge source files should own one primary responsibility. Gameplay systems must not migrate into renderer files merely because the renderer displays them. Prefer focused 150–350 line implementation files; review files around 400 lines for responsibility splits and strongly justify anything beyond ~500 lines. Generated/third-party data are exceptions.

Current ownership examples: mining under `game/mining`, drops under `game/drops`, inventory under `game/inventory`, growth under `world/growth`, micro-state under `world/micro`, meshing under `world/meshing`, GPU presentation under `render/vulkan`, and persistence under `save`.
