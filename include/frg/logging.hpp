#ifndef FRG_LOGGING_HPP
#define FRG_LOGGING_HPP

#include <utility>
#include <frg/formatting.hpp>
#include <frg/macros.hpp>

namespace frg FRG_VISIBILITY {

struct endlog_t { };
//TODO: Make this inline.
static constexpr endlog_t endlog;

template<typename Sink, size_t Limit = 128>
struct stack_buffer_logger {
	struct item {
		item(stack_buffer_logger *logger)
		: _logger{logger}, _off{0}, _emitted{false} { }

		item(const item &) = delete;

		item &operator= (const item &) = delete;

		~item() {
			// TODO: Warn here.
		}

		template<typename T>
		item &operator<< (T object) {
			format(object, *this);
			return *this;
		}

		item &operator<< (endlog_t) {
			_logger->_emit(_buffer);
			return *this;
		}

		void append(char s) {
			FRG_ASSERT(_off < Limit);
			if(_off + 1 == Limit) {
				_logger->_emit(_buffer);
				_off = 0;
			}
			_buffer[_off++] = s;
			_buffer[_off] = 0;
		}

		void append(const char *str) {
			while(*str) {
				FRG_ASSERT(_off < Limit);
				if(_off + 1 == Limit) {
					_logger->_emit(_buffer);
					_off = 0;
				}
				_buffer[_off++] = *str++;
				_buffer[_off] = 0;
			}
		}

	private:
		stack_buffer_logger *_logger;
		char _buffer[Limit];
		size_t _off;
		bool _emitted;
	};

	// constexpr so that this can be initialized statically.
	constexpr stack_buffer_logger(Sink sink = Sink{})
	: _sink{std::move(sink)} { }

	item operator() () {
		return item{this};
	}

private:
	void _emit(const char *message) {
		_sink(message);
	}

	Sink _sink;
};

} // namespace frg

#endif // FRG_LOGGING_HPP
