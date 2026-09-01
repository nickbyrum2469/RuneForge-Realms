#include "TestSuites.h"

#include "render/scene/ChunkCulling.h"

#include <cassert>

void runCullingTests() {
    rf::render::scene::ChunkCullInput input;
    input.eye = {0.0f, 6.0f, 0.0f};
    input.forward = {0.0f, 0.0f, 1.0f};
    input.maxDistanceBlocks = 100.0f;

    assert(rf::render::scene::ChunkCulling::visible({0, 2}, input));
    assert(!rf::render::scene::ChunkCulling::visible({0, -3}, input));

    // Nearby chunks remain visible even if technically behind the view vector so turning
    // the camera cannot expose a hole while conservative culling catches up.
    assert(rf::render::scene::ChunkCulling::visible({0, -1}, input));

    input.maxDistanceBlocks = 24.0f;
    assert(!rf::render::scene::ChunkCulling::visible({4, 4}, input));
}
