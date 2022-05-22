# frigg is header-only, so we do not need a compiler.
project('frigg')

# We alreadys need C++ on the build system to build the analyzer.
add_languages('cpp', native: true)

incl = include_directories('include')

if not get_option('frigg_no_install')
	install_headers(
		'include/frg/algorithm.hpp',
		'include/frg/allocation.hpp',
		'include/frg/array.hpp',
		'include/frg/container_of.hpp',
		'include/frg/detection.hpp',
		'include/frg/dyn_array.hpp',
		'include/frg/eternal.hpp',
		'include/frg/expected.hpp',
		'include/frg/formatting.hpp',
		'include/frg/functional.hpp',
		'include/frg/hash.hpp',
		'include/frg/hash_map.hpp',
		'include/frg/interval_tree.hpp',
		'include/frg/intrusive.hpp',
		'include/frg/list.hpp',
		'include/frg/logging.hpp',
		'include/frg/macros.hpp',
		'include/frg/manual_box.hpp',
		'include/frg/mutex.hpp',
		'include/frg/optional.hpp',
		'include/frg/pairing_heap.hpp',
		'include/frg/printf.hpp',
		'include/frg/qs.hpp',
		'include/frg/random.hpp',
		'include/frg/rbtree.hpp',
		'include/frg/rcu_radixtree.hpp',
		'include/frg/slab.hpp',
		'include/frg/small_vector.hpp',
		'include/frg/span.hpp',
		'include/frg/spinlock.hpp',
		'include/frg/stack.hpp',
		'include/frg/std_compat.hpp',
		'include/frg/string.hpp',
		'include/frg/tuple.hpp',
		'include/frg/unique.hpp',
		'include/frg/utility.hpp',
		'include/frg/variant.hpp',
		'include/frg/vector.hpp',
		subdir: 'frg')

	pkg = import('pkgconfig')
	pkg.generate(
		name: 'frigg',
		description: 'Lightweight C++ utilities and algorithms for system programming',
	)
endif

frigg_dep = declare_dependency(include_directories: incl)

analyzer = executable(
		'slab_trace_analyzer',
		'slab_trace_analyzer.cpp',
		cpp_args: ['-std=c++17', '-O3'],
		build_by_default: false,
		native: true)
