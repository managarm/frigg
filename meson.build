project('frigg', 'cpp')

incl = include_directories('include')

if not get_option('frigg_no_install')
	install_headers(
		'include/frg/allocation.hpp',
		'include/frg/container_of.hpp',
		'include/frg/eternal.hpp',
		'include/frg/formatting.hpp',
		'include/frg/functional.hpp',
		'include/frg/hash.hpp',
		'include/frg/hash_map.hpp',
		'include/frg/intrusive.hpp',
		'include/frg/list.hpp',
		'include/frg/logging.hpp',
		'include/frg/macros.hpp',
		'include/frg/manual_box.hpp',
		'include/frg/mutex.hpp',
		'include/frg/optional.hpp',
		'include/frg/pairing_heap.hpp',
		'include/frg/qs.hpp',
		'include/frg/random.hpp',
		'include/frg/rbtree.hpp',
		'include/frg/rcu_radixtree.hpp',
		'include/frg/slab.hpp',
		'include/frg/string.hpp',
		'include/frg/tuple.hpp',
		'include/frg/utility.hpp',
		'include/frg/vector.hpp',
		'include/frg/unique.hpp',
		subdir: 'frg')
endif

frigg_dep = declare_dependency(include_directories: incl)

