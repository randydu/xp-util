#ifndef ON_EXIT_H_
#define ON_EXIT_H_

#include <utility>

namespace xp {
template <typename T>
class exit_exec
{
public:
    exit_exec(T&& f) : f_(std::forward<T>(f)) {}
    ~exit_exec() { f_(); }

private:
    T f_;
};

template <typename T>
constexpr auto make_on_exit(T&& f)
{
    return exit_exec{std::forward<T>(f)};
}
} // namespace xp

#define COMBINE1(X, Y) X##Y // helper macro
#define COMBINE(X, Y) COMBINE1(X, Y)

#define ON_EXIT(code) auto COMBINE(exit_exec, __LINE__) = xp::make_on_exit([&]() { code; })

#endif