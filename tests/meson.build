gtest = dependency('gtest_main')

test_executable = executable('frigg_tests',
	'tests.cpp',
	dependencies: [
		gtest, frigg_dep
	],
	override_options: ['cpp_std=c++20'],
	native: true,
	install: false
)

test('frigg tests', test_executable)
