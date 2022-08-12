srcs = [
    'intf_tests.cpp', 'cls_util_tests.cpp',
]

catch2_dep = dependency('catch2')

xputil_test = executable('xputil-test', srcs, dependencies: [catch2_dep, xputil_dep])

test('xputil-test', xputil_test)