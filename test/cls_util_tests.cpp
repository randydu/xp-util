#include <type_traits>
#include <xputil/class_util.h>

#include "catch2.h"

const auto tag = "[cls]";

class A
{
public:
    constexpr static int id = 1;
    static std::string name;
    static int count;
    static void dummy() {}
    static int test(int, std::string) { return 0; }
    static int test3(int, std::string, float) { return 0; }
    static int hello(int) { return 0; }
    static void world(std::string) {}

    int add(int) { return 0; }
    int inc() { return 0; }
    void dec(int&, void*) {}
};


TEST_CASE("is_callable", tag)
{
    // static_assert(xp::is_callable(A::hello));
    static_assert(HAS_STATIC_VAR(A, const int, id));

    static_assert(HAS_STATIC_VAR(A, std::string, name));
    static_assert(HAS_STATIC_VAR(A, int, count));

    static_assert(HAS_STATIC_FUNCTION(A, void, dummy));
    static_assert(HAS_STATIC_FUNCTION(A, int, test, int, std::string));
    static_assert(HAS_STATIC_FUNCTION(A, int, hello, int));
    // std::invoke_result_t<decltype(A::hello)> i;
    static_assert(HAS_STATIC_FUNCTION(A, void, world, std::string));

    // static_assert(xp::has_static_function<A, std::string>(A::world));
    static_assert(HAS_FUNCTION(A, int, add, int));
    static_assert(HAS_FUNCTION(A, int, inc));
    static_assert(HAS_FUNCTION(A, void, dec, int&, void*));
    MUST_HAS_FUNCTION(A, int, add, int);
    // static_assert(HAS_MEMBER_FUNCTION(A, add, void, int), "xxx");
    // using ft = std::function<int(int)>;
    // using namespace std::placeholders;
    // static_assert( std::is_same_v< decltype(std::bind(&A::add, std::declval<A>(), int())()), void>);
    // A::test(VARG2(int, std::string));
    // auto x = xp::xbind<decltype(A::hello), int>(A::hello);
    // static_assert( std::is_same_v<int, decltype(x())> );
    // auto y = xp::xbind<decltype(A::test), int, std::string>(A::test);
    // auto z = xp::xbind<decltype(A::test3), int, std::string, float>(A::test3);
}
