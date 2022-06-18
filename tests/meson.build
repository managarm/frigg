gtest = dependency('gtest_main')

test_executable = executable('frigg_tests',
	'tests.cpp',
	dependencies: [
		gtest, frigg_dep
	],
        native: true,
	install: false
)

test('frigg tests', test_executable)
