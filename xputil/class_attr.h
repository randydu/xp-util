#ifndef CLASS_ATTR_H_
#define CLASS_ATTR_H_

// prohibit heap-based objects.
#define NO_HEAP                                      \
    static void *operator new(std::size_t) = delete; \
    static void *operator new[](std::size_t) = delete;

#endif