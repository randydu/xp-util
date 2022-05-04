srcs = [
    'impl_intfs.cpp',
]

lib_xputil_lite = static_library('xputil_lite', srcs, dependencies: [gsl_dep])

xputil_lite_dep = declare_dependency(include_directories: '..', link_with: lib_xputil_lite, dependencies: [gsl_dep])