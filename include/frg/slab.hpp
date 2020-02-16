#ifndef FRG_SLAB_HPP
#define FRG_SLAB_HPP

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <frg/macros.hpp>
#include <frg/mutex.hpp>
#include <frg/rbtree.hpp>

namespace frg FRG_VISIBILITY {

namespace {
	// TODO: We need a frigg logging mechanism to enable this.
	//constexpr bool logAllocations = true;

	constexpr bool enable_checking = false;
}

template<typename T>
struct bitop_impl;

template<>
struct bitop_impl<unsigned long> {
	static constexpr int clz(unsigned long x) {
		return __builtin_clzl(x);
	}
};

template<typename T, size_t N>
constexpr size_t array_size(const T (&)[N]) {
	return N;
}

template<typename Policy, typename Mutex>
class slab_pool {
public:
	slab_pool(Policy &plcy);

	slab_pool(const slab_pool &) = delete;

	slab_pool &operator= (const slab_pool &) = delete;

	void *allocate(size_t length);
	void *realloc(void *pointer, size_t new_length);
	void free(void *pointer);
	void deallocate(void *pointer, size_t size);

	size_t numUsedPages() {
		return _usedPages;
	}

private:
	// The following variables configure the size of the buckets.
	// Bucket size increases both with small_base_exp and small_step_exp. Furthermore,
	// small_step_exp controls how many buckets are between any two power-of-2 buckets.
	// The first bucket has a size of size_t(1) << (small_base_exp + small_step_exp).
	// This approach is taken from jemalloc.
#ifdef __clang__
	static constexpr size_t tiny_sizes[4] = {8, 16, 32, 64};
#else
	static constexpr size_t tiny_sizes[] = {8, 16, 32, 64};
#endif
	static constexpr unsigned int small_base_exp = 6;
	static constexpr unsigned int small_step_exp = 0;

	static_assert(tiny_sizes[array_size(tiny_sizes) - 1]
			== (static_cast<size_t>(1) << (small_base_exp + small_step_exp)),
		"Last tiny bucket must match first small bucket");

	// Computes the size of a given bucket.
	static constexpr size_t bucket_to_size(unsigned int idx) {
		// First, we handle the hard-coded tiny sizes.
		auto tc = array_size(tiny_sizes);
		if(idx < tc)
			return tiny_sizes[idx];

		// Next, we handle the small sizes.
		auto s = 1 << small_step_exp;
		auto ip = (idx - tc + 1) >> small_step_exp;
		auto is = (idx - tc + 1) & (s - 1);
		auto f = small_base_exp + ip;
		return static_cast<size_t>(s + is) << f;
	}

	// The "inverse" of bucket_to_size().
	static constexpr size_t size_to_bucket(size_t size) {
		// First, we handle the hard-coded tiny sizes.
		auto tc = array_size(tiny_sizes);
		if(size <= bucket_to_size(tc - 1)) {
			for(unsigned int i = 0; i < tc - 1; i++)
				if(size <= bucket_to_size(i))
					return i;
			return tc - 1;
		}

		// Next, we handle the small sizes. Variables correspond to those in bucket_to_size().
		auto e = (sizeof(size_t) * 8 - 1) - bitop_impl<size_t>::clz(size);
		auto f = e - small_step_exp;
		auto ip = ((f - small_base_exp) << small_step_exp);
		auto is = ((size - (static_cast<size_t>(1) << e)) + (static_cast<size_t>(1) << f) - 1) >> f;
		return tc - 1 + ip + is;
	}

	// This variable controls the number of buckets that we actually use.
	static constexpr int num_buckets = 13;

	static constexpr size_t max_bucket_size = bucket_to_size(num_buckets - 1);

	// Here, we perform some compile-time verification of the bucket size calculation.
	static constexpr bool test_bucket_calculation(unsigned int n) {
		for(unsigned int i = 0; i < n; i++) {
			if(size_to_bucket(bucket_to_size(i)) != i)
				return false;
			if(size_to_bucket(bucket_to_size(i) + 1) != i + 1)
				return false;
		}
		return true;
	}

	static_assert(test_bucket_calculation(num_buckets),
		"The bucket size calculation seems to be broken");

	static constexpr size_t page_size = 0x1000;

	// Size of the content of a slab.
	static constexpr size_t slabsize = 1 << 18;

	static_assert(!(slabsize & (page_size - 1)),
			"Slab content size must be a multiple of the page size");

	// TODO: Refactor the huge frame padding.
	static constexpr size_t huge_padding = page_size;

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
	static_assert(sizeof(frame) <= huge_padding, "Padding too small");

	struct slab_frame : frame {
		slab_frame(uintptr_t address_, size_t length_, int index_)
		: frame{frame_type::slab, address_, length_},
				index{index_}, num_reserved{0}, available{nullptr} { }

		slab_frame(const slab_frame &) = delete;

		slab_frame &operator= (const slab_frame &) = delete;

		const int index;
		unsigned int num_reserved;
		freelist *available;
		rbtree_hook partial_hook;
	};

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
	slab_frame *_construct_slab(int index);
	frame *_construct_large(size_t area_size);

	void _verify_integrity();
	void _verify_frame_integrity(frame *fra);

private:
	Policy &_plcy;

	Mutex _tree_mutex;
	frame_tree_type _frame_tree;
	size_t _usedPages;
	bucket _bkts[num_buckets];
};

// --------------------------------------------------------
// slab_pool
// --------------------------------------------------------

template<typename Policy, typename Mutex>
slab_pool<Policy, Mutex>::slab_pool(Policy &plcy)
: _plcy{plcy}, _usedPages{0} { }

template<typename Policy, typename Mutex>
void *slab_pool<Policy, Mutex>::allocate(size_t length) {
	if(enable_checking)
		_verify_integrity();

	// malloc() is allowed to either return null or a unique value.
	// However, some programs always interpret null returns as failure,
	// so we round up the length.
	if(!length)
		length = 1;

	if(length <= max_bucket_size) {
		int index = size_to_bucket(length);
		FRG_ASSERT(index <= num_buckets);
		auto bkt = &_bkts[index];

		unique_lock<Mutex> bucket_guard(bkt->bucket_mutex);

		freelist *object;
		if(bkt->head_slb) {
			auto slb = bkt->head_slb;

			object = slb->available;
			FRG_ASSERT(object);
			FRG_ASSERT(slb->contains(object));
			if(object->link && !slb->contains(object->link))
				FRG_ASSERT(!"slab_pool corruption. Possible write to unallocated object");
			slb->available = object->link;
			slb->num_reserved++;

			if(!slb->available) {
				bkt->partial_tree.remove(slb);
				bkt->head_slb = bkt->partial_tree.first();
			}
		}else{
			// Call into the Policy without holding locks.
			bucket_guard.unlock();

			auto slb = _construct_slab(index);

			object = slb->available;
			FRG_ASSERT(object);
			FRG_ASSERT(slb->contains(object));
			if(object->link && !slb->contains(object->link))
				FRG_ASSERT(!"slab_pool corruption. Possible write to unallocated object");
			slb->available = object->link;
			slb->num_reserved++;

			unique_lock<Mutex> tree_guard(_tree_mutex);
			_frame_tree.insert(slb);
			_usedPages += (slb->length + huge_padding) / page_size;
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
		if(enable_checking)
			_verify_integrity();
		return object;
	}else{
		auto area_size = (length + page_size - 1) & ~(page_size - 1);
		auto fra = _construct_large(area_size);

		unique_lock<Mutex> tree_guard(_tree_mutex);
		_frame_tree.insert(fra);
		_usedPages += (fra->length + huge_padding) / page_size;
		tree_guard.unlock();

		//if(logAllocations)
		//	std::cout << "frg/slab: Allocate large-object at " <<
		//			(void *)fra->address << std::endl;
		if(enable_checking)
			_verify_integrity();
		return reinterpret_cast<void *>(fra->address);
	}
}

template<typename Policy, typename Mutex>
void *slab_pool<Policy, Mutex>::realloc(void *pointer, size_t new_length) {
	if(enable_checking)
		_verify_integrity();

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
		size_t item_size = bucket_to_size(slb->index);

		if(new_length <= item_size)
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
void slab_pool<Policy, Mutex>::free(void *pointer) {
	if(enable_checking)
		_verify_integrity();

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
		_usedPages -= (fra->length + huge_padding) / page_size;

		// Call into the Policy without holding locks.
		tree_guard.unlock();

		_plcy.unmap((uintptr_t)fra->address - huge_padding, fra->length + huge_padding);
		if(enable_checking)
			_verify_integrity();
		return;
	}

	// As we deallocate from a slab, we can drop the _tree_mutex now.
	FRG_ASSERT(fra->type == frame_type::slab);
	tree_guard.unlock();

	auto slb = static_cast<slab_frame *>(fra);
	FRG_ASSERT(reinterpret_cast<uintptr_t>(slb) == (address & ~(slabsize - 1)));
	auto bkt = &_bkts[slb->index];

	size_t item_size = bucket_to_size(slb->index);
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

	bucket_guard.unlock();

	if(enable_checking)
		_verify_integrity();
}

template<typename Policy, typename Mutex>
void slab_pool<Policy, Mutex>::deallocate(void *pointer, size_t size) {
	if(!pointer)
		return;

	//if(logAllocations)
	//	std::cout << "frg/slab: Free " << pointer << std::endl;

	if(size > max_bucket_size) {
		this->free(pointer);
		return;
	}

	auto address = reinterpret_cast<uintptr_t>(pointer);

	auto slb = reinterpret_cast<slab_frame *>(address & ~(slabsize - 1));
	FRG_ASSERT(slb->contains(pointer));
	auto bkt = &_bkts[slb->index];

	size_t item_size = bucket_to_size(slb->index);
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
auto slab_pool<Policy, Mutex>::_find_frame(uintptr_t address)
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
auto slab_pool<Policy, Mutex>::_construct_slab(int index)
-> slab_frame * {
//	frg::infoLogger() << "Allocate new area for " << (void *)area_size << frg::endLog;

	// Allocate virtual memory for the slab.
	uintptr_t address = _plcy.map(2 * slabsize);
	address = (address + slabsize - 1) & ~(slabsize - 1);

	auto item_size = bucket_to_size(index);
	size_t overhead = 0;
	while(overhead < sizeof(slab_frame)) // FIXME.
		overhead += item_size;
	FRG_ASSERT(overhead < slabsize);
	auto slb = new ((void *)address) slab_frame(address + overhead, slabsize - overhead, index);

	//if(logAllocations)
	//	std::cout << "frb/slab: New area at " << area << std::endl;

	// Partition the slab into individual objects.
	freelist *first = nullptr;
	for(size_t off = 0; off < slb->length; off += item_size) {
		auto object = new (reinterpret_cast<char *>(slb->address) + off) freelist;
		object->link = first;
		first = object;
		//infoLogger() << "[slab] fill " << chunk << frg::endLog;
	}
	slb->available = first;

	return slb;
}

template<typename Policy, typename Mutex>
auto slab_pool<Policy, Mutex>::_construct_large(size_t area_size)
-> frame * {
//	frg::infoLogger() << "Allocate new area for " << (void *)area_size << frg::endLog;

	// Allocate virtual memory for the frame.
	FRG_ASSERT(!(area_size & (page_size - 1)));
	uintptr_t address = _plcy.map(area_size + huge_padding);

	auto fra = new ((void *)address) frame(frame_type::large,
			address + huge_padding, area_size);

	//if(logAllocations)
	//	std::cout << "frb/slab: New area at " << area << std::endl;

	return fra;
}

template<typename Policy, typename Mutex>
void slab_pool<Policy, Mutex>::_verify_integrity() {
	unique_lock<Mutex> tree_guard(_tree_mutex);
	if(_frame_tree.get_root())
		_verify_frame_integrity(_frame_tree.get_root());
}

template<typename Policy, typename Mutex>
void slab_pool<Policy, Mutex>::_verify_frame_integrity(frame *fra) {
	if(fra->type == frame_type::slab) {
		auto slb = static_cast<slab_frame *>(fra);
		auto bkt = &_bkts[slb->index];
		unique_lock<Mutex> slab_guard(bkt->bucket_mutex);

		auto object = slb->available;
		while(object) {
			FRG_ASSERT(slb->contains(object));
			object = object->link;
		}
	}

	if(_frame_tree.get_left(fra))
		_verify_frame_integrity(frame_tree_type::get_left(fra));
	if(_frame_tree.get_right(fra))
		_verify_frame_integrity(frame_tree_type::get_right(fra));
}

// --------------------------------------------------------
// slab_allocator
// --------------------------------------------------------

template<typename Policy, typename Mutex>
class slab_allocator {
public:
	slab_allocator(slab_pool<Policy, Mutex> *pool)
	: pool_{pool} { }

	void *allocate(size_t size) {
		return pool_->allocate(size);
	}

	void deallocate(void *pointer, size_t size) {
		pool_->deallocate(pointer, size);
	}

	void free(void *pointer) {
		pool_->free(pointer);
	}

	void *reallocate(void *pointer, size_t new_size) {
		return pool_->realloc(pointer, new_size);
	}

private:
	slab_pool<Policy, Mutex> *pool_;
};

} // namespace frg

#endif // FRG_SLAB_HPP
