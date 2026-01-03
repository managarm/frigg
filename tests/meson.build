gtest = dependency('gtest_main')

test_executable = executable('frigg_tests',
	'support.cpp',
	'tests.cpp',
	dependencies: [
		gtest, frigg_dep
	],
	override_options: ['cpp_std=c++23'],
	native: true,
	install: false
)

test('frigg tests', test_executable)
