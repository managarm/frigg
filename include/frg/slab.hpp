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
	slab_allocator(Policy &virt_alloc);

	void *allocate(size_t length);
	void *realloc(void *pointer, size_t new_length);
	void free(void *pointer);

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
	
	enum frame_type {
		null,
		slab,
		large
	};

	// By frame we mean either a slab or a large memory object.
	// Frames are stored in a tree to allow fast lookup by in-frame addresses.
	struct frame {
		frame(frame_type type, uintptr_t address, size_t length)
		: type{type}, baseAddress{address}, length{length}, power{0} { }
		
		const frame_type type;
		const uintptr_t baseAddress;
		const size_t length;

		int power;

		rbtree_hook tree_hook;
	};
	static_assert(sizeof(frame) <= kVirtualAreaPadding, "Padding too small");

	struct frame_less {
		bool operator() (const frame &a, const frame &b) {
			return a.baseAddress < b.baseAddress;
		}
	};
	
	using frame_tree_type = frg::rbtree<
		frame,
		&frame::tree_hook,
		frame_less
	>;

	struct freelist {
		freelist()
		: link{nullptr} { }

		freelist *link;
	};

private:
	frame *_find_frame(uintptr_t address);

	frame *allocateNewArea(frame_type type, size_t area_size);
	void fillSlabArea(frame *area, int power);

private:
	frame_tree_type _frame_tree;

	freelist *_freeList[kNumPowers];
	Policy &_virtAllocator;
	Mutex _mutex;

	size_t _usedPages;
};

// --------------------------------------------------------
// slab_allocator
// --------------------------------------------------------

template<typename Policy, typename Mutex>
slab_allocator<Policy, Mutex>::slab_allocator(Policy &virt_alloc)
: _virtAllocator(virt_alloc), _usedPages(0) {
	for(size_t i = 0; i < kNumPowers; i++)
		_freeList[i] = nullptr;
}

template<typename Policy, typename Mutex>
void *slab_allocator<Policy, Mutex>::allocate(size_t length) {
	unique_lock<Mutex> guard(_mutex);
	
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
		if(_freeList[index] == nullptr) {
			size_t area_size = uintptr_t(1) << kMaxPower;
			frame *area = allocateNewArea(frame_type::slab, area_size);
			fillSlabArea(area, power);
		}
		
		freelist *chunk = _freeList[index];
		FRG_ASSERT(chunk != nullptr);
		_freeList[index] = chunk->link;
		//if(logAllocations)
		//	std::cout << "frg/slab: Allocate small-object at " << chunk << std::endl;
		return chunk;
	}else{
		size_t area_size = length;
		if((area_size % kPageSize) != 0)
			area_size += kPageSize - length % kPageSize;
		frame *area = allocateNewArea(frame_type::large, area_size);
		//if(logAllocations)
		//	std::cout << "frg/slab: Allocate large-object at " <<
		//			(void *)area->baseAddress << std::endl;
		return (void *)area->baseAddress;
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

	unique_lock<Mutex> guard(_mutex);

	auto address = reinterpret_cast<uintptr_t>(pointer);
	auto current = _find_frame(address);
	if(!current)
		FRG_ASSERT(!"Pointer is not part of any virtual area");

	if(current->type == frame_type::slab) {
		size_t item_size = size_t(1) << current->power;

		if(new_length < item_size)
			return pointer;

		guard.unlock(); // TODO: this is inefficient
		void *new_pointer = allocate(new_length);
		if(!new_pointer)
			return nullptr;
		memcpy(new_pointer, pointer, item_size);
		free(pointer);
		//infoLogger() << "[slab] realloc " << new_pointer << frg::endLog;
		return new_pointer;
	}else{
		FRG_ASSERT(current->type == frame_type::large);
		FRG_ASSERT(address == current->baseAddress);
		
		if(new_length < current->length)
			return pointer;
		
		guard.unlock(); // TODO: this is inefficient
		void *new_pointer = allocate(new_length);
		if(!new_pointer)
			return nullptr;
		memcpy(new_pointer, pointer, current->length);
		free(pointer);
		//infoLogger() << "[slab] realloc " << new_pointer << frg::endLog;
		return new_pointer;
	}
}

template<typename Policy, typename Mutex>
void slab_allocator<Policy, Mutex>::free(void *pointer) {	
	if(!pointer)
		return;

	unique_lock<Mutex> guard(_mutex);
	
	//if(logAllocations)
	//	std::cout << "frg/slab: Free " << pointer << std::endl;

	auto address = reinterpret_cast<uintptr_t>(pointer);
	auto current = _find_frame(address);
	if(!current)
		FRG_ASSERT(!"Pointer is not part of any virtual area");

	if(current->type == frame_type::slab) {
		int index = current->power - kMinPower;
		FRG_ASSERT(current->power <= kMaxPower);
		size_t item_size = size_t(1) << current->power;
		FRG_ASSERT(((address - current->baseAddress) % item_size) == 0);
//				infoLogger() << "[" << pointer
//						<< "] Small free from varea " << current << endLog;

		auto chunk = new (pointer) freelist;
		chunk->link = _freeList[index];
		_freeList[index] = chunk;
		return;
	}else{
		//if(logAllocations)
		//	std::cout << "    From area " << current << std::endl;
		FRG_ASSERT(current->type == frame_type::large);
		FRG_ASSERT(address == current->baseAddress);
//				infoLogger() << "[" << pointer
//						<< "] Large free from varea " << current << endLog;
		
		// Remove the virtual area from the area-list.
		_frame_tree.remove(current);
		
		// deallocate the memory used by the area
		_usedPages -= (current->length + kVirtualAreaPadding) / kPageSize;
		_virtAllocator.unmap((uintptr_t)current->baseAddress - kVirtualAreaPadding,
				current->length + kVirtualAreaPadding);
		return;
	}
}

template<typename Policy, typename Mutex>
auto slab_allocator<Policy, Mutex>::_find_frame(uintptr_t address)
-> frame * {
	auto current = _frame_tree.get_root();
	while(current) {
		if(address < current->baseAddress) {
			current = frame_tree_type::get_left(current);
		}else if(address >= current->baseAddress + current->length) {
			current = frame_tree_type::get_right(current);
		}else{
			FRG_ASSERT(address >= current->baseAddress
					&& address < current->baseAddress + current->length);
			return current;
		}
	}

	return nullptr;
}


template<typename Policy, typename Mutex>
auto slab_allocator<Policy, Mutex>::allocateNewArea(frame_type type, size_t area_size)
-> frame * {
//	frg::infoLogger() << "Allocate new area for " << (void *)area_size << frg::endLog;

	// allocate virtual memory for the chunk
	FRG_ASSERT((area_size % kPageSize) == 0);
	uintptr_t address = _virtAllocator.map(area_size + kVirtualAreaPadding);
	_usedPages += (area_size + kVirtualAreaPadding) / kPageSize;
	
	// setup the virtual area descriptor
	auto area = new ((void *)address) frame(type,
			address + kVirtualAreaPadding, area_size);
	_frame_tree.insert(area);
	
	//if(logAllocations)
	//	std::cout << "frb/slab: New area at " << area << std::endl;

	return area;
}

template<typename Policy, typename Mutex>
void slab_allocator<Policy, Mutex>::fillSlabArea(frame *area, int power) {
	FRG_ASSERT(area->type == frame_type::slab && area->power == 0);
	area->power = power;

	// setup the free chunks in the new area
	size_t item_size = uintptr_t(1) << power;
	size_t num_items = area->length / item_size;
	FRG_ASSERT(num_items > 0);
	FRG_ASSERT((area->length % item_size) == 0);

	int index = power - kMinPower;
	for(size_t i = 0; i < num_items; i++) {
		auto chunk = new ((void *)(area->baseAddress + i * item_size)) freelist;
		chunk->link = _freeList[index];
		_freeList[index] = chunk;
		//infoLogger() << "[slab] fill " << chunk << frg::endLog;
	}
}

} // namespace frg

#endif // FRG_SLAB_HPP
