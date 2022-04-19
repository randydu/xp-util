#include "catch_macros.h"
#include <type_traits>
#include <xputil/class_util.h>

const auto tag = "cls";

class A
{
public:
    static std::string name;
    static int count;
    static void dummy() {}
    static int test(int, std::string) {return 0;}
    static int test3(int, std::string, float) {return 0;}
    static int hello(int) {return 0;}
    static void world(std::string) {}

    int add(int) {return 0;}
};


TEST_CASE("is_callable", tag)
{
    //static_assert(xp::is_callable(A::hello));
    
    static_assert(HAS_STATIC_MEMBER(std::string, A::name));
    static_assert(HAS_STATIC_MEMBER(int, A::count));

    static_assert(HAS_STATIC_FUNCTION_NO_PARAM(void, A::dummy));
    static_assert(HAS_STATIC_FUNCTION(int, A::test, int, std::string));
    static_assert(HAS_STATIC_FUNCTION(int, A::hello, int));
   //std::invoke_result_t<decltype(A::hello)> i;
    static_assert(HAS_STATIC_FUNCTION(void, A::world, std::string));

    //static_assert(xp::has_static_function<A, std::string>(A::world));
    static_assert(HAS_MEMBER_FUNCTION(int, A, add, int));
    static_assert(HAS_MEMBER_FUNCTION(void, A, add, int), "must have void A::add(int)");
    //static_assert(HAS_MEMBER_FUNCTION(A, add, void, int), "xxx");
   // using ft = std::function<int(int)>;
   // using namespace std::placeholders;
    //static_assert( std::is_same_v< decltype(std::bind(&A::add, std::declval<A>(), int())()), void>);
    //A::test(VARG2(int, std::string));
    //auto x = xp::xbind<decltype(A::hello), int>(A::hello);
    //static_assert( std::is_same_v<int, decltype(x())> );
    //auto y = xp::xbind<decltype(A::test), int, std::string>(A::test);
    //auto z = xp::xbind<decltype(A::test3), int, std::string, float>(A::test3);
}
