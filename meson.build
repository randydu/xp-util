project('xputil', 'cpp', default_options: ['cpp_std=c++20', 'werror=true'], version: '2.0.4')

# stop after 1 errors

cpp = meson.get_compiler('cpp')
message('cpp id: ' + cpp.get_id())

if cpp.get_id() == 'gcc'
    add_project_arguments(['-fmax-errors=1', '-Werror=unknown-pragmas'], language: 'cpp')
endif

if cpp.get_id() == 'msvc'
#  enable support of __VA_OPT__ 
    add_project_arguments(['/Zc:preprocessor'], language: 'cpp')
endif

gsl_dep = dependency('gsl')

subdir('xputil')
subdir('test')