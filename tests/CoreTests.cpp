#include "TestSuites.h"

#include <iostream>

int main() {
    runRegistryTests();
    runJobTests();
    runWorldTests();
    runPersistenceTests();
    runCullingTests();

    std::cout << "RuneForge 0.3.2 engine-scale tests passed\n";
    return 0;
}
