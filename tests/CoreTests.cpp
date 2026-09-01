#include "core/HubModel.h"
#include "core/Version.h"

#include <cassert>
#include <iostream>

int main() {
    const auto a = rf::Version::parse("0.1.0");
    const auto b = rf::Version::parse("v0.2.1");
    assert(a && b);
    assert(*b > *a);
    assert(a->toString() == "0.1.0");
    assert(!rf::Version::parse("0.1"));

    rf::HubModel hub;
    assert(hub.modes().size() == 6);
    assert(hub.selectedMode().id == "frontier");
    hub.selectMode(3);
    assert(hub.selectedMode().id == "labyrinth");
    hub.selectMode(999);
    assert(hub.selectedMode().id == "labyrinth");

    std::cout << "RuneForge core tests passed\n";
    return 0;
}
