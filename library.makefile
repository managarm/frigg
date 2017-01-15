
.PHONY: install
install:
	mkdir -p $(DESTDIR)$(prefix)include/frg
	install --mode=0644 $S/include/frg/macros.hpp $(DESTDIR)$(prefix)include/frg/
	install --mode=0644 $S/include/frg/slab.hpp $(DESTDIR)$(prefix)include/frg/

