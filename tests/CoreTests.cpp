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
    runDropTests();
    runPolishFoundationTests();

    std::cout << "RuneForge 0.5.0 commercial-foundation tests passed\n";
    return 0;
}
