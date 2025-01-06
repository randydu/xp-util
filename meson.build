project('xputil', 'cpp', default_options: ['cpp_std=c++20', 'werror=true'], version: '2.4.0')

cpp = meson.get_compiler('cpp')
message('cpp id: ' + cpp.get_id())

cc_id = cpp.get_id()

if cc_id == 'gcc'
    add_project_arguments(['-Werror=unknown-pragmas'], language: 'cpp')
elif cc_id == 'clang'
    add_project_arguments(['-Werror=unknown-pragmas'], language: 'cpp')
elif cc_id == 'msvc'
#  enable support of __VA_OPT__ 
    add_project_arguments(['/Zc:preprocessor'], language: 'cpp')
endif

gsl_dep = dependency('gsl')

subdir('src')

if get_option('XPUTIL_BUILD_TESTS')
    subdir('test')
endif