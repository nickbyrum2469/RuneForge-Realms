#include "TestSuites.h"

#include <iostream>

int main() {
    runRegistryTests();
    runJobTests();
    runWorldTests();
    runPersistenceTests();
    runCullingTests();
    runMicroMiningTests();
    runInventoryGrowthTests();

    std::cout << "RuneForge 0.4.0 visual-survival tests passed\n";
    return 0;
}
