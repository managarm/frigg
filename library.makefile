
.PHONY: install
install:
	mkdir -p $(DESTDIR)$(prefix)include/frg
	install --mode=0644 $S/include/frg/container_of.hpp $(DESTDIR)$(prefix)include/frg/
	install --mode=0644 $S/include/frg/intrusive.hpp $(DESTDIR)$(prefix)include/frg/
	install --mode=0644 $S/include/frg/macros.hpp $(DESTDIR)$(prefix)include/frg/
	install --mode=0644 $S/include/frg/mutex.hpp $(DESTDIR)$(prefix)include/frg/
	install --mode=0644 $S/include/frg/pairing_heap.hpp $(DESTDIR)$(prefix)include/frg/
	install --mode=0644 $S/include/frg/qs.hpp $(DESTDIR)$(prefix)include/frg/
	install --mode=0644 $S/include/frg/random.hpp $(DESTDIR)$(prefix)include/frg/
	install --mode=0644 $S/include/frg/rbtree.hpp $(DESTDIR)$(prefix)include/frg/
	install --mode=0644 $S/include/frg/rcu_radixtree.hpp $(DESTDIR)$(prefix)include/frg/
	install --mode=0644 $S/include/frg/slab.hpp $(DESTDIR)$(prefix)include/frg/
	install --mode=0644 $S/include/frg/utility.hpp $(DESTDIR)$(prefix)include/frg/

