srcs = [
    'impl_intfs.cpp',
]

lib_xputil = static_library('xputil', srcs, dependencies: [gsl_dep])

xputil_dep = declare_dependency(include_directories: '..', link_with: lib_xputil, dependencies: [gsl_dep])