#ifndef FRG_SLAB_HPP
#define FRG_SLAB_HPP

#include <string.h>
#include <mutex>
#include <frg/macros.hpp>

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

template<typename VirtualAlloc, typename Mutex>
class slab_allocator {
public:
	slab_allocator(VirtualAlloc &virt_alloc);

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

	struct FreeChunk {
		FreeChunk *nextChunk;

		FreeChunk();
	};
	
	enum AreaType {
		kTypeNone,
		kTypeSlab,
		kTypeLarge
	};

	struct VirtualArea {
		VirtualArea(AreaType type, uintptr_t address, size_t length);
		
		const AreaType type;
		const uintptr_t baseAddress;
		const size_t length;

		int power;

		VirtualArea *right;
	};
	static_assert(sizeof(VirtualArea) <= kVirtualAreaPadding, "Padding too small");
	
	VirtualArea *allocateNewArea(AreaType type, size_t area_size);
	void fillSlabArea(VirtualArea *area, int power);

	VirtualArea *_root;
	FreeChunk *_freeList[kNumPowers];
	VirtualAlloc &_virtAllocator;
	Mutex _mutex;

	size_t _usedPages;
};

// --------------------------------------------------------
// slab_allocator
// --------------------------------------------------------

template<typename VirtualAlloc, typename Mutex>
slab_allocator<VirtualAlloc, Mutex>::slab_allocator(VirtualAlloc &virt_alloc)
: _root(nullptr), _virtAllocator(virt_alloc), _usedPages(0) {
	for(size_t i = 0; i < kNumPowers; i++)
		_freeList[i] = nullptr;
}

template<typename VirtualAlloc, typename Mutex>
void *slab_allocator<VirtualAlloc, Mutex>::allocate(size_t length) {
	std::lock_guard<Mutex> guard(_mutex);
	
	if(length == 0)
		return nullptr;

	if(length <= (uintptr_t(1) << kMaxPower)) {
		int power = nextPower(length);
		assert(length <= (uintptr_t(1) << power));
		assert(power <= kMaxPower);
		if(power < kMinPower)
			power = kMinPower;
		
		int index = power - kMinPower;
		if(_freeList[index] == nullptr) {
			size_t area_size = uintptr_t(1) << kMaxPower;
			VirtualArea *area = allocateNewArea(kTypeSlab, area_size);
			fillSlabArea(area, power);
		}
		
		FreeChunk *chunk = _freeList[index];
		assert(chunk != nullptr);
		_freeList[index] = chunk->nextChunk;
		//if(logAllocations)
		//	std::cout << "frg/slab: Allocate small-object at " << chunk << std::endl;
		return chunk;
	}else{
		size_t area_size = length;
		if((area_size % kPageSize) != 0)
			area_size += kPageSize - length % kPageSize;
		VirtualArea *area = allocateNewArea(kTypeLarge, area_size);
		//if(logAllocations)
		//	std::cout << "frg/slab: Allocate large-object at " <<
		//			(void *)area->baseAddress << std::endl;
		return (void *)area->baseAddress;
	}
}

template<typename VirtualAlloc, typename Mutex>
void *slab_allocator<VirtualAlloc, Mutex>::realloc(void *pointer, size_t new_length) {
	if(!pointer) {
		return allocate(new_length);
	}else if(!new_length) {
		free(pointer);
		return nullptr;
	}

	std::lock_guard<Mutex> guard(_mutex);
	uintptr_t address = (uintptr_t)pointer;

	VirtualArea *current = _root;
	while(current != nullptr) {
		if(address >= current->baseAddress
				&& address < current->baseAddress + current->length) {
			if(current->type == kTypeSlab) {
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
				assert(current->type == kTypeLarge);
				assert(address == current->baseAddress);
				
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

		current = current->right;
	}

	assert(!"Pointer is not part of any virtual area");
	__builtin_unreachable();
}

template<typename VirtualAlloc, typename Mutex>
void slab_allocator<VirtualAlloc, Mutex>::free(void *pointer) {
	std::lock_guard<Mutex> guard(_mutex);
	
	if(!pointer)
		return;
	
	//if(logAllocations)
	//	std::cout << "frg/slab: Free " << pointer << std::endl;

	uintptr_t address = (uintptr_t)pointer;

	VirtualArea *previous = nullptr;
	VirtualArea *current = _root;
	while(current != nullptr) {
		if(address >= current->baseAddress
				&& address < current->baseAddress + current->length) {
			if(current->type == kTypeSlab) {
				int index = current->power - kMinPower;
				assert(current->power <= kMaxPower);
				size_t item_size = size_t(1) << current->power;
				assert(((address - current->baseAddress) % item_size) == 0);
//				infoLogger() << "[" << pointer
//						<< "] Small free from varea " << current << endLog;

				auto chunk = new (pointer) FreeChunk();
				chunk->nextChunk = _freeList[index];
				_freeList[index] = chunk;
				return;
			}else{
				//if(logAllocations)
				//	std::cout << "    From area " << current << std::endl;
				assert(current->type == kTypeLarge);
				assert(address == current->baseAddress);
//				infoLogger() << "[" << pointer
//						<< "] Large free from varea " << current << endLog;
				
				// remove the virtual area from the area-list
				if(previous) {
					previous->right = current->right;
				}else{
					_root = current->right;
				}
				
				// deallocate the memory used by the area
				_usedPages -= (current->length + kVirtualAreaPadding) / kPageSize;
				_virtAllocator.unmap((uintptr_t)current->baseAddress - kVirtualAreaPadding,
						current->length + kVirtualAreaPadding);
				return;
			}
		}
		
		previous = current;
		current = current->right;
	}

	assert(!"Pointer is not part of any virtual area");
}

template<typename VirtualAlloc, typename Mutex>
auto slab_allocator<VirtualAlloc, Mutex>::allocateNewArea(AreaType type, size_t area_size)
-> VirtualArea * {
//	frg::infoLogger() << "Allocate new area for " << (void *)area_size << frg::endLog;

	// allocate virtual memory for the chunk
	assert((area_size % kPageSize) == 0);
	uintptr_t address = _virtAllocator.map(area_size + kVirtualAreaPadding);
	_usedPages += (area_size + kVirtualAreaPadding) / kPageSize;
	
	// setup the virtual area descriptor
	auto area = new ((void *)address) VirtualArea(type,
			address + kVirtualAreaPadding, area_size);
	area->right = _root;
	_root = area;
	
	//if(logAllocations)
	//	std::cout << "frb/slab: New area at " << area << std::endl;

	return area;
}

template<typename VirtualAlloc, typename Mutex>
void slab_allocator<VirtualAlloc, Mutex>::fillSlabArea(VirtualArea *area, int power) {
	assert(area->type == kTypeSlab && area->power == 0);
	area->power = power;

	// setup the free chunks in the new area
	size_t item_size = uintptr_t(1) << power;
	size_t num_items = area->length / item_size;
	assert(num_items > 0);
	assert((area->length % item_size) == 0);

	int index = power - kMinPower;
	for(size_t i = 0; i < num_items; i++) {
		auto chunk = new ((void *)(area->baseAddress + i * item_size)) FreeChunk();
		chunk->nextChunk = _freeList[index];
		_freeList[index] = chunk;
		//infoLogger() << "[slab] fill " << chunk << frg::endLog;
	}
}

// --------------------------------------------------------
// slab_allocator::FreeChunk
// --------------------------------------------------------

template<typename VirtualAlloc, typename Mutex>
slab_allocator<VirtualAlloc, Mutex>::FreeChunk::FreeChunk()
: nextChunk(nullptr) { }

// --------------------------------------------------------
// slab_allocator::VirtualArea
// --------------------------------------------------------

template<typename VirtualAlloc, typename Mutex>
slab_allocator<VirtualAlloc, Mutex>::VirtualArea::VirtualArea(AreaType type,
		uintptr_t address, size_t length)
: type(type), baseAddress(address), length(length), power(0), right(nullptr) { }

} // namespace frg

#endif // FRG_SLAB_HPP
