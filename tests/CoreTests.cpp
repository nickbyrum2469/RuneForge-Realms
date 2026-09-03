#include "TestSuites.h"

#include <iostream>

int main() {
    runRegistryTests();
    runJobTests();
    runWorldTests();
    runPersistenceTests();
    runCullingTests();
    runMeshSchedulingTests();
    runMicroMiningTests();
    runInventoryGrowthTests();
    runSurfaceReliefTests();
    runDropTests();
    runPolishFoundationTests();
    runCharacterRigTests();

    std::cout << "RuneForge core regression tests passed\n";
    return 0;
}
