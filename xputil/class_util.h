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


template <typename Fn, typename... Args>
constexpr bool is_callable()
{
    if constexpr (std::is_invocable_v<Fn, Args...>) {
        return true;
    } else {
        return false;
    }
}


// static member checking
#define HAS_STATIC_VAR(t, cls, member) (std::is_same_v<t, decltype(cls::member)>)

#define HAS_STATIC_FUNCTION_NO_PARAM(ret_t, cls, member) (std::is_same_v<std::invoke_result_t<decltype(cls::member)>, ret_t>)
#define HAS_STATIC_FUNCTION(ret_t, cls, member, ...) (std::is_same_v<std::invoke_result_t<decltype(cls::member), __VA_ARGS__>, ret_t>)

#define HAS_FUNCTION(ret_t, cls, member, ...) (std::is_same_v<std::invoke_result_t<decltype(&cls::member), decltype(std::declval<cls>()), __VA_ARGS__>, ret_t>)

#define MUST_HAS_STATIC_VAR(t, cls, member)                                                           \
    {                                                                                                 \
        static_assert(HAS_STATIC_VAR(t, cls, member), "invalid static variable: " #cls "::" #member); \
    }
#define MUST_HAS_STATIC_FUNCTION(ret_t, cls, member, ...)                                                                   \
    {                                                                                                                       \
        static_assert(HAS_STATIC_FUNCTION(ret_t, cls, member, __VA_ARGS__), "invalid static function: " #cls "::" #member); \
    }
#define MUST_HAS_FUNCTION(ret_t, cls, member, ...)                                                                   \
    {                                                                                                                \
        static_assert(HAS_FUNCTION(ret_t, cls, member, __VA_ARGS__), "invalid member function: " #cls "::" #member); \
    }

//#define HAS_MEMBER_FUNCTION(cls, f, ret_t, a) (std::is_same_v<decltype(std::bind(&cls::f, std::declval<cls>(), std::declval<a>())()), ret_t>)
//#define HAS_MEMBER_FUNCTION_DEV(f, ret_t, a) (std::is_same_v<std::invoke_result_t<(xp::xbind(&cls::f, std::declval<cls>(), std::declval<a>())()), ret_t>)
} // namespace xp


#endif