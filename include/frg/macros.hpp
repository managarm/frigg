#ifndef FRG_MACROS_HPP
#define FRG_MACROS_HPP

#define FRG_STRINGIFY(x) #x
#define FRG_EVALSTRINGIFY(x) FRG_STRINGIFY(x)

#define FRG_INTF(x) frg_ ## x

extern "C" {
	[[gnu::weak]] void FRG_INTF(log)(const char *cstring);
	[[gnu::weak]] void FRG_INTF(panic)(const char *cstring);
}

// TODO: Switch visibility depending on compilation flags.
#define FRG_VISIBILITY

// TODO: Actually provide assertions.
#define FRG_ASSERT(x) do { \
	if(!(x)) { \
		if(!FRG_INTF(panic)) \
			__builtin_trap(); \
		FRG_INTF(panic)(__FILE__ ":" FRG_EVALSTRINGIFY(__LINE__) \
				": Assertion '" #x "' failed!"); \
		__builtin_trap(); \
	}\
} while(0)

#define FRG_DEBUG_ASSERT(x) do { \
	if(!(x)) { \
		if(!FRG_INTF(log)) \
			break; \
		FRG_INTF(log)(__FILE__ ":" FRG_EVALSTRINGIFY(__LINE__) \
				": Assertion '" #x "' failed!"); \
	}\
} while(0)

#endif // FRG_MACROS_HPP
