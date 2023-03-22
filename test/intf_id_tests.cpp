#include <catch2/catch.hpp>

#include <xputil/intf_defs.h>

#define CALC_IID(x) xp::calc_iid(x)

// Make sure the interface-id algorithm is backward compatible.
TEST_CASE("intf-id-test", "[intf_id]")
{
    using namespace xp;

    CHECK(CALC_IID("") == 0x553e93901e462a6e);
    CHECK(CALC_IID(" ") == 0xf8e5e1651bd01df3);
    CHECK(IID_IINTERFACE == 0xdc1bb37e5ceab0cb);
    CHECK(IID_IINTERFACEEX == 0x3137d9e333ec18e0);
    CHECK(IID_IBUS == 0xddde57c5e192042a);
}