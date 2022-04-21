project('xputil', 'cpp', default_options: ['cpp_std=c++17', 'werror=true'], version: '1.0.0')

# stop after 1 errors
if meson.get_compiler('cpp').get_id() == 'gcc'
    add_project_arguments(['-fmax-errors=1', '-Werror=unknown-pragmas'], language: 'cpp') 
endif

subdir('xputil')
subdir('test')