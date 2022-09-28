srcs = [
    'impl_intfs.cpp',
]

lib_xputil = static_library('xputil', srcs, dependencies: [gsl_dep], include_directories: ['include'])

xputil_dep = declare_dependency(include_directories: 'include', link_with: lib_xputil, dependencies: [gsl_dep])