#include <peony.calcifer/include/calcifer.hpp>

#include "Geode/loader/ModEvent.hpp"

namespace c = calcifer;

$on_mod(Loaded) {
    (void)c::test("loads"_spr, [](c::TestCase& tc) {
        tc.level(c::Level::main(1))
            .timeout(c::seconds(1))
            .expect(calcifer::Outcome::Timeout);
    });
};
