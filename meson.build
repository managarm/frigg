project('frigg', 'cpp')

incl = include_directories('include')

install_headers('include/frg/container_of.hpp',
		'include/frg/intrusive.hpp',
		'include/frg/macros.hpp',
		'include/frg/mutex.hpp',
		'include/frg/pairing_heap.hpp',
		'include/frg/qs.hpp',
		'include/frg/random.hpp',
		'include/frg/rbtree.hpp',
		'include/frg/rcu_radixtree.hpp',
		'include/frg/slab.hpp',
		'include/frg/utility.hpp',
    	subdir: 'frg')

frigg_dep = declare_dependency(include_directories: incl)

