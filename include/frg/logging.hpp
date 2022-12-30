#ifndef FRG_LOGGING_HPP
#define FRG_LOGGING_HPP

#include <utility>
#include <frg/formatting.hpp>
#include <frg/macros.hpp>
#include <frg/detection.hpp>

#if __STDC_HOSTED__
#	if __has_include(<iostream>)
#		include <iostream>
#		define FRIGG_HAS_IOSTREAM
#	endif
#endif

namespace frg FRG_VISIBILITY {

struct endlog_t { };
inline constexpr endlog_t endlog;

template<typename Sink>
using sink_begin_t = decltype(std::declval<Sink>().begin());

template<typename Sink>
using sink_finalize_t = decltype(std::declval<Sink>().finalize(true));

template<typename Sink, size_t Limit = 128>
struct stack_buffer_logger {
	struct item {
		item(stack_buffer_logger *logger)
		: _logger{logger}, _off{0}, _emitted{false}, _done{false} { }

		item(const item &) = delete;

		item &operator= (const item &) = delete;

		~item() {
			_logger->_finalize(_done);
		}

		template<typename T>
		item &operator<< (T &&object) {
			format(std::forward<T>(object), *this);
			return *this;
		}

		item &operator<< (endlog_t) {
			FRG_ASSERT(_off < Limit);
			_buffer[_off] = 0;
			_logger->_emit(_buffer);
			_done = true;
			return *this;
		}

		void append(char s) {
			FRG_ASSERT(_off < Limit);
			if(_off + 1 == Limit) {
				_buffer[_off] = 0;
				_logger->_emit(_buffer);
				_off = 0;
			}
			_buffer[_off++] = s;
		}

		void append(const char *str) {
			while(*str) {
				FRG_ASSERT(_off < Limit);
				if(_off + 1 == Limit) {
					_buffer[_off] = 0;
					_logger->_emit(_buffer);
					_off = 0;
				}
				_buffer[_off++] = *str++;
			}
		}

	private:
		stack_buffer_logger *_logger;
		char _buffer[Limit];
		size_t _off;
		bool _emitted;
		bool _done;
	};

	// constexpr so that this can be initialized statically.
	constexpr stack_buffer_logger(Sink sink = Sink{})
	: _sink{std::move(sink)} { }

	item operator() () {
		if constexpr (is_detected_v<sink_begin_t, Sink>)
			_sink.begin();

		return item{this};
	}

private:
	void _emit(const char *message) {
		_sink(message);
	}

	void _finalize(bool done) {
		if constexpr (is_detected_v<sink_finalize_t, Sink>)
			_sink.finalize(done);
	}

	Sink _sink;
};

template <typename Container>
struct container_logger {
	constexpr container_logger(Container &cont)
	: cont_{cont} { }

	auto &operator<<(auto &&t) {
		format(std::forward<decltype(t)>(t), *this);
		return *this;
	}

	void append(typename Container::value_type s) {
		cont_.push_back(s);
	}

	void append(const typename Container::value_type *str) {
		// For std::basic_string
		if constexpr ( requires { cont_.insert(cont_.size(), str); } )
			cont_.insert(cont_.size(), str);
		else {
			while (*str)
				append(*str++);
		}
	}

private:
	Container &cont_;
};

template <typename Container> requires (
		requires (Container &cont, typename Container::value_type c) {
			cont.push_back(c);
		})
constexpr auto output_to(Container &cont) {
	return container_logger{cont};
}

template<typename Out>
concept is_ostream_like = requires(Out &out, const char *str, char c) { out << str; out << c; };

template<typename Out> requires is_ostream_like<Out>
struct ostream_out {
	Out &output;

	ostream_out(Out &out) : output(out) { }

	void append(const char *str) {
		output << str;
	}

	void append(char c) {
		output << c;
	}

	auto &operator<<(auto &&t) {
		format(std::forward<decltype(t)>(t), *this);
		return *this;
	}

	auto &operator<<(endlog_t) {
#ifdef FRIGG_HAS_IOSTREAM
		output << std::endl;
#else
		output << '\n';
#endif // FRIGG_HAS_IOSTREAM
		return *this;
	}
};

template<typename Out> requires is_ostream_like<Out>
inline auto to(Out &x) {
	return ostream_out { x };
}


} // namespace frg

#endif // FRG_LOGGING_HPP
