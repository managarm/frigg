gtest = dependency('gtest_main', required: false, disabler: true)

test_executable = executable('frigg_tests',
	'tests.cpp',
	dependencies: [
		gtest, frigg_dep
	],
	override_options: ['cpp_std=c++23'],
	native: true,
	install: false
)

test('frigg tests', test_executable)
