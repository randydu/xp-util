#include <catch2/catch.hpp>

#include <xputil/intf_defs.h>

#define CALC_IID(x) xp::calc_iid(x)

// Make sure the interface-id algorithm is backward compatible.
TEST_CASE("intf-id-test", "[intf_id]")
{
    using namespace xp;

    CHECK(CALC_IID("") == 5381);
    CHECK(CALC_IID(" ") == 177605);
    CHECK(IID_IINTERFACE == 0xa34b6dbd1d954bff);
    CHECK(IID_IINTERFACEEX == 0xc6b1973a682b017c);
    CHECK(IID_IBUS == 0xafd07334098fcc11);
}