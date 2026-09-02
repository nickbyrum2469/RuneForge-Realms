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
    runDropTests();
    runPolishFoundationTests();
    runCharacterRigTests();

    std::cout << "RuneForge 0.5.0 commercial-foundation tests passed\n";
    return 0;
}
