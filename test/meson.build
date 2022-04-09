srcs = [
    'intf_tests.cpp',
]

catch2_dep = dependency('catch2')
catch2_ex_dep = dependency('catch2-ex')

executable('xputil-test', srcs, 
    dependencies: [catch2_dep, catch2_ex_dep, xputil_lite_dep],
)