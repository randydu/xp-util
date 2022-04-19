#ifndef XP_CLASS_UTIL_H
#define XP_CLASS_UTIL_H

#include <type_traits>
// Helper for class template

namespace xp {
// primary template handles types that have no nested ::type member:
template <class, class = void>
struct has_type : std::false_type {
};

// specialization recognizes types that do have a nested ::type member:
template <class T>
struct has_type<T, std::void_t<T>> : std::true_type {
};


/*
template<typename V >
constexpr bool has_static_member(V&& v){
    return has_type<V>::value;
}*/

template <typename V>
constexpr bool has_static_member(V&& v)
{
    return true;
}

#define HAS_STATIC_MEMBER0(member) (xp::has_static_member(member))
#define HAS_STATIC_MEMBER(t, member) (std::is_same_v<t, decltype(member)>)

template <typename Fn, typename... Args>
constexpr bool has_static_function(Fn&& f)
{
    return std::is_invocable_v<Fn, Args...>;
}

template <typename Fn, typename... Args>
constexpr bool is_callable()
{
    if constexpr (std::is_invocable_v<Fn, Args...>) {
        return true;
    } else {
        return false;
    }
}

#define HAS_STATIC_FUNCTION_NO_PARAM(ret_t, member) (std::is_same_v<std::invoke_result_t<decltype(member)>, ret_t>)
#define HAS_STATIC_FUNCTION(ret_t, member, ...) (std::is_same_v<std::invoke_result_t<decltype(member), __VA_ARGS__>, ret_t>)

#define HAS_MEMBER_FUNCTION(ret_t, cls, member, ...) (std::is_same_v<std::invoke_result_t<decltype(&cls::member), decltype(std::declval<cls>()), __VA_ARGS__>, ret_t>)
//#define HAS_STATIC_FUNCTION1(ret_t, member, a) (std::is_same_v< decltype(member(std::declval<a>())), ret_t >)
//#define HAS_STATIC_FUNCTION2(ret_t, member, a) (std::is_same_v< decltype(member(std::declval<a>())), ret_t >)
//#define HAS_STATIC_FUNCTION1(member, ret_t, a) (xp::has_type< decltype(member(std::declval<a>())) >::value)

#define VARG(t) std::declval<t>()
#define VARG2(t1, ...) std::declval<t1>, VARG(__VA_ARGS__)   

template<typename F, typename V>
auto myf = [](F f, V v){
    return f(v);
};

template<typename F, typename T, typename ...S>
constexpr auto xbind(F f){
    constexpr int params = sizeof...(S);
    if constexpr (params == 0){
        return std::bind(f, std::declval<T>());
    }else if constexpr (params == 1){
        auto f_next = std::bind(f, std::declval<T>(), std::placeholders::_1);
        return xbind<decltype(f_next), S...>(f_next);
    }else{
        auto f_next = std::bind(f, std::declval<T>(), std::placeholders::_1, std::placeholders::_2);
        return xbind<decltype(f_next), S...>(f_next);
    }
}



//#define HAS_MEMBER_FUNCTION(cls, f, ret_t, a) (std::is_same_v<decltype(std::bind(&cls::f, std::declval<cls>(), std::declval<a>())()), ret_t>)
//#define HAS_MEMBER_FUNCTION_DEV(f, ret_t, a) (std::is_same_v<std::invoke_result_t<(xp::xbind(&cls::f, std::declval<cls>(), std::declval<a>())()), ret_t>)
} // namespace xp


#endif