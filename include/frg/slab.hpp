#ifndef FRG_SLAB_HPP
#define FRG_SLAB_HPP

#include <string.h>
#include <frg/macros.hpp>
#include <frg/mutex.hpp>
#include <frg/rbtree.hpp>

namespace frg FRG_VISIBILITY {

namespace {
	// TODO: We need a frigg logging mechanism to enable this.
	//constexpr bool logAllocations = true;
}

inline int nextPower(uint64_t n) {
	uint64_t u = n;

	#define S(k) if (u >= (uint64_t(1) << k)) { p += k; u >>= k; }
	int p = 0;
	S(32); S(16); S(8); S(4); S(2); S(1);
	#undef S

	if(n > (uint64_t(1) << p))
		p++;
	return p;
}

template<typename Policy, typename Mutex>
class slab_allocator {
public:
	slab_allocator(Policy &plcy);

	void *allocate(size_t length);
	void *realloc(void *pointer, size_t new_length);
	void free(void *pointer);
	void deallocate(void *pointer, size_t size);

	size_t numUsedPages() {
		return _usedPages;
	}

private:	
	enum {
		kPageSize = 0x1000,
		kVirtualAreaPadding = kPageSize,
		kMinPower = 5,
		kMaxPower = 16,
		kNumPowers = kMaxPower - kMinPower + 1
	};

	struct freelist {
		freelist()
		: link{nullptr} { }

		freelist(const freelist &) = delete;

		freelist &operator= (const freelist &) = delete;

		freelist *link;
	};
	
	enum class frame_type {
		null,
		slab,
		large
	};

	// By frame we mean either a slab_frame or a large memory object.
	// Frames are stored in a tree to allow fast lookup by in-frame addresses.
	struct frame {
		frame(frame_type type_, uintptr_t address_, size_t length_)
		: type{type_}, address{address_}, length{length_} { }

		frame(const frame &) = delete;

		frame &operator= (const frame &) = delete;
		
		bool contains(void *p) {
			auto adr = reinterpret_cast<uintptr_t>(p);
			return adr >= address && adr < address + length;
		}

		const frame_type type;
		const uintptr_t address;
		const size_t length;
		rbtree_hook frame_hook;
	};

	struct slab_frame : frame {
		slab_frame(uintptr_t address_, size_t length_, int power_)
		: frame{frame_type::slab, address_, length_},
				power{power_}, num_reserved{0}, available{nullptr} { }

		slab_frame(const slab_frame &) = delete;

		slab_frame &operator= (const slab_frame &) = delete;

		const int power;
		unsigned int num_reserved;
		freelist *available;
		rbtree_hook partial_hook;
	};
	static_assert(sizeof(slab_frame) <= kVirtualAreaPadding, "Padding too small");

	struct frame_less {
		bool operator() (const frame &a, const frame &b) {
			return a.address < b.address;
		}
	};
	
	using frame_tree_type = frg::rbtree<
		frame,
		&frame::frame_hook,
		frame_less
	>;

	using partial_tree_type = frg::rbtree<
		slab_frame,
		&slab_frame::partial_hook,
		frame_less
	>;

	// Like in jemalloc, we always allocate a slab completely (the head_slb) before
	// moving to the next slab. This reduces external fragmentation.
	struct bucket {
		bucket()
		: head_slb{nullptr} { }

		Mutex bucket_mutex;
		slab_frame *head_slb;
		partial_tree_type partial_tree;
	};

private:
	frame *_find_frame(uintptr_t address);
	slab_frame *_construct_slab(int power, size_t area_size);
	frame *_construct_large(size_t area_size);

private:
	Policy &_plcy;

	Mutex _tree_mutex;
	frame_tree_type _frame_tree;
	size_t _usedPages;
	bucket _bkts[kNumPowers];
};

// --------------------------------------------------------
// slab_allocator
// --------------------------------------------------------

template<typename Policy, typename Mutex>
slab_allocator<Policy, Mutex>::slab_allocator(Policy &plcy)
: _plcy{plcy}, _usedPages{0} { }

template<typename Policy, typename Mutex>
void *slab_allocator<Policy, Mutex>::allocate(size_t length) {
	// malloc() is allowed to either return null or a unique value.
	// However, some programs always interpret null returns as failure,
	// so we round up the length.
	if(!length)
		length = 1;

	if(length <= (uintptr_t(1) << kMaxPower)) {
		int power = nextPower(length);
		FRG_ASSERT(length <= (uintptr_t(1) << power));
		FRG_ASSERT(power <= kMaxPower);
		if(power < kMinPower)
			power = kMinPower;
		
		int index = power - kMinPower;
		auto bkt = &_bkts[index];

		unique_lock<Mutex> bucket_guard(bkt->bucket_mutex);

		freelist *object;
		if(bkt->head_slb) {
			auto slb = bkt->head_slb;

			object = slb->available;
			FRG_ASSERT(object);
			FRG_ASSERT(slb->contains(object));
			if(object->link && !slb->contains(object->link))
				FRG_ASSERT(!"slab_allocator corruption. Possible write to unallocated object");
			slb->available = object->link;
			slb->num_reserved++;

			if(!slb->available) {
				bkt->partial_tree.remove(slb);
				bkt->head_slb = bkt->partial_tree.first();
			}
		}else{
			// Call into the Policy without holding locks.
			bucket_guard.unlock();

			size_t area_size = uintptr_t(1) << (kMaxPower + 2);
			auto slb = _construct_slab(power, area_size);

			object = slb->available;
			FRG_ASSERT(object);
			FRG_ASSERT(slb->contains(object));
			if(object->link && !slb->contains(object->link))
				FRG_ASSERT(!"slab_allocator corruption. Possible write to unallocated object");
			slb->available = object->link;
			slb->num_reserved++;

			unique_lock<Mutex> tree_guard(_tree_mutex);
			_frame_tree.insert(slb);
			_usedPages += (slb->length + kVirtualAreaPadding) / kPageSize;
			tree_guard.unlock();

			// Finally, re-lock the bucket to attach the new slab.
			bucket_guard.lock();

			FRG_ASSERT(slb->available);
			bkt->partial_tree.insert(slb);
			if(!bkt->head_slb || slb->address < bkt->head_slb->address)
				bkt->head_slb = slb;
		}
	
		bucket_guard.unlock();

		//if(logAllocations)
		//	std::cout << "frg/slab: Allocate small-object at " << object << std::endl;
		object->~freelist();
		return object;
	}else{
		size_t area_size = length;
		if((area_size % kPageSize) != 0)
			area_size += kPageSize - length % kPageSize;
		auto fra = _construct_large(area_size);
		
		unique_lock<Mutex> tree_guard(_tree_mutex);
		_frame_tree.insert(fra);
		_usedPages += (fra->length + kVirtualAreaPadding) / kPageSize;
		tree_guard.unlock();

		//if(logAllocations)
		//	std::cout << "frg/slab: Allocate large-object at " <<
		//			(void *)fra->address << std::endl;
		return reinterpret_cast<void *>(fra->address);
	}
}

template<typename Policy, typename Mutex>
void *slab_allocator<Policy, Mutex>::realloc(void *pointer, size_t new_length) {
	if(!pointer) {
		return allocate(new_length);
	}else if(!new_length) {
		free(pointer);
		return nullptr;
	}
	auto address = reinterpret_cast<uintptr_t>(pointer);

	unique_lock<Mutex> tree_guard(_tree_mutex);
	auto fra = _find_frame(address);
	tree_guard.unlock();
	if(!fra)
		FRG_ASSERT(!"Pointer is not part of any virtual area");

	if(fra->type == frame_type::slab) {
		auto slb = static_cast<slab_frame *>(fra);
		size_t item_size = size_t(1) << slb->power;

		if(new_length < item_size)
			return pointer;

		void *new_pointer = allocate(new_length);
		if(!new_pointer)
			return nullptr;
		memcpy(new_pointer, pointer, item_size);
		free(pointer);
		//infoLogger() << "[slab] realloc " << new_pointer << frg::endLog;
		return new_pointer;
	}else{
		FRG_ASSERT(fra->type == frame_type::large);
		FRG_ASSERT(address == fra->address);
		
		if(new_length < fra->length)
			return pointer;
		
		void *new_pointer = allocate(new_length);
		if(!new_pointer)
			return nullptr;
		memcpy(new_pointer, pointer, fra->length);
		free(pointer);
		//infoLogger() << "[slab] realloc " << new_pointer << frg::endLog;
		return new_pointer;
	}
}

template<typename Policy, typename Mutex>
void slab_allocator<Policy, Mutex>::free(void *pointer) {	
	if(!pointer)
		return;
	
	//if(logAllocations)
	//	std::cout << "frg/slab: Free " << pointer << std::endl;

	auto address = reinterpret_cast<uintptr_t>(pointer);

	unique_lock<Mutex> tree_guard(_tree_mutex);
	auto fra = _find_frame(address);
	if(!fra)
		FRG_ASSERT(!"Pointer is not part of any virtual area");

	// First, we handle cases that need to operate with the _tree_mutex held.
	if(fra->type == frame_type::large) {
		//if(logAllocations)
		//	std::cout << "    From area " << fra << std::endl;
		FRG_ASSERT(address == fra->address);
//				infoLogger() << "[" << pointer
//						<< "] Large free from varea " << fra << endLog;
		
		// Remove the virtual area from the area-list.
		_frame_tree.remove(fra);
		_usedPages -= (fra->length + kVirtualAreaPadding) / kPageSize;

		// Call into the Policy without holding locks.
		tree_guard.unlock();
		
		_plcy.unmap((uintptr_t)fra->address - kVirtualAreaPadding,
				fra->length + kVirtualAreaPadding);
		return;
	}

	// As we deallocate from a slab, we can drop the _tree_mutex now.
	FRG_ASSERT(fra->type == frame_type::slab);
	tree_guard.unlock();

	auto slb = static_cast<slab_frame *>(fra);
	FRG_ASSERT(reinterpret_cast<uintptr_t>(slb)
			== (address & ~((size_t(1) << (kMaxPower + 2)) - 1)));
	int index = slb->power - kMinPower;
	auto bkt = &_bkts[index];

	size_t item_size = size_t(1) << slb->power;
	FRG_ASSERT(((address - slb->address) % item_size) == 0);
//				infoLogger() << "[" << pointer
//						<< "] Small free from varea " << slb << endLog;

	unique_lock<Mutex> bucket_guard(bkt->bucket_mutex);

	bool was_unavailable = !slb->available;
	FRG_ASSERT(slb->num_reserved);

	auto object = new (pointer) freelist;
	FRG_ASSERT(!slb->available || slb->contains(slb->available));
	object->link = slb->available;
	slb->available = object;

	if(was_unavailable) {
		bkt->partial_tree.insert(slb);
		if(!bkt->head_slb || slb->address < bkt->head_slb->address)
			bkt->head_slb = slb;
	}
}

template<typename Policy, typename Mutex>
void slab_allocator<Policy, Mutex>::deallocate(void *pointer, size_t size) {	
	if(!pointer)
		return;

	//if(logAllocations)
	//	std::cout << "frg/slab: Free " << pointer << std::endl;

	if(size > (size_t(1) << kMaxPower)) {
		this->free(pointer);
		return;
	}

	auto address = reinterpret_cast<uintptr_t>(pointer);

	auto slb = reinterpret_cast<slab_frame *>(address & ~((size_t(1) << (kMaxPower + 2)) - 1));
	FRG_ASSERT(slb->contains(pointer));

	int index = slb->power - kMinPower;
	auto bkt = &_bkts[index];

	size_t item_size = size_t(1) << slb->power;
	FRG_ASSERT(((address - slb->address) % item_size) == 0);
//				infoLogger() << "[" << pointer
//						<< "] Small free from varea " << slb << endLog;

	unique_lock<Mutex> bucket_guard(bkt->bucket_mutex);

	bool was_unavailable = !slb->available;
	FRG_ASSERT(slb->num_reserved);

	auto object = new (pointer) freelist;
	FRG_ASSERT(!slb->available || slb->contains(slb->available));
	object->link = slb->available;
	slb->available = object;

	if(was_unavailable) {
		bkt->partial_tree.insert(slb);
		if(!bkt->head_slb || slb->address < bkt->head_slb->address)
			bkt->head_slb = slb;
	}
}


template<typename Policy, typename Mutex>
auto slab_allocator<Policy, Mutex>::_find_frame(uintptr_t address)
-> frame * {
	auto current = _frame_tree.get_root();
	while(current) {
		if(address < current->address) {
			current = frame_tree_type::get_left(current);
		}else if(address >= current->address + current->length) {
			current = frame_tree_type::get_right(current);
		}else{
			FRG_ASSERT(address >= current->address
					&& address < current->address + current->length);
			return current;
		}
	}

	return nullptr;
}

template<typename Policy, typename Mutex>
auto slab_allocator<Policy, Mutex>::_construct_slab(int power, size_t area_size)
-> slab_frame * {
//	frg::infoLogger() << "Allocate new area for " << (void *)area_size << frg::endLog;

	// Allocate virtual memory for the slab.
	FRG_ASSERT((area_size % kPageSize) == 0);
	uintptr_t address = _plcy.map(2 * area_size);
	address = (address + area_size - 1) & ~(area_size - 1);
	
	auto item_size = size_t(1) << power;
	auto overhead = (sizeof(slab_frame) + item_size - 1) & ~(item_size - 1);
	FRG_ASSERT(overhead < area_size);
	auto slb = new ((void *)address) slab_frame(address + overhead, area_size - overhead, power);

	//if(logAllocations)
	//	std::cout << "frb/slab: New area at " << area << std::endl;

	// Partition the slab into individual objects.
	size_t num_items = slb->length / item_size;
	FRG_ASSERT(num_items > 0);
	FRG_ASSERT((slb->length % item_size) == 0);

	freelist *first = nullptr;
	for(size_t i = 0; i < num_items; i++) {
		if(i * item_size < overhead)
			continue;
		auto p = reinterpret_cast<char *>(slb->address) + i * item_size;
		FRG_ASSERT(slb->contains(p));
		auto object = new (p) freelist;
		object->link = first;
		first = object;
		//infoLogger() << "[slab] fill " << chunk << frg::endLog;
	}
	slb->available = first;

	return slb;
}

template<typename Policy, typename Mutex>
auto slab_allocator<Policy, Mutex>::_construct_large(size_t area_size)
-> frame * {
//	frg::infoLogger() << "Allocate new area for " << (void *)area_size << frg::endLog;

	// Allocate virtual memory for the frame.
	FRG_ASSERT((area_size % kPageSize) == 0);
	uintptr_t address = _plcy.map(area_size + kVirtualAreaPadding);
	
	auto fra = new ((void *)address) frame(frame_type::large,
			address + kVirtualAreaPadding, area_size);

	//if(logAllocations)
	//	std::cout << "frb/slab: New area at " << area << std::endl;

	return fra;
}

} // namespace frg

#endif // FRG_SLAB_HPP
