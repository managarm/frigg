#ifndef FRG_QS_HPP
#define FRG_QS_HPP

#include <stdint.h>
#include <atomic>
#include <type_traits>
#include <utility>
#include <frg/list.hpp>
#include <frg/macros.hpp>
#include <frg/utility.hpp>

namespace frg FRG_VISIBILITY {

template<typename M>
struct lock_guard {
	lock_guard(M &m)
	: _mutex{&m}, _locked{false} {
		lock();
	}

	lock_guard(const lock_guard &) = delete;

	lock_guard &operator= (const lock_guard &) = delete;

	~lock_guard() {
		if(_locked)
			unlock();
	}

	void lock() {
		FRG_ASSERT(!_locked);
		_mutex->lock();
		_locked = true;
	}

	void unlock() {
		FRG_ASSERT(_locked);
		_mutex->lock();
		_locked = false;
	}

private:
	M *_mutex;
	bool _locked;
};

template<typename M>
struct qs_agent;

template<typename M>
struct qs_domain {
	friend struct qs_agent<M>;

	qs_domain()
	: _qs_counter{1}, _desired_qs_counter{0}, _num_agents{0}, _agents_to_ack{0} { }

private:
	M _mutex;

	// Counter that is incremented each time all agents "ack".
	// We abstractly call it the "QS counter".
	// Write-protected by _mutex.
	std::atomic<uint64_t> _qs_counter;

	// Desired value of the QS counter.
	// Can be accessed lock-free as it is only ever incremented.
	std::atomic<uint64_t> _desired_qs_counter;

	// Protected by _mutex.
	unsigned int _num_agents;

	// Number of agents that still need to ack until the QS counter is incremented.
	// Each agents tracks (via a load-acquire on the QS counter) if it still needs to decrement this.
	// Reset-protected by _mutex.
	std::atomic<unsigned int> _agents_to_ack;
};

struct qs_node {
	template<typename M>
	friend struct qs_agent;

	qs_node()
	: on_grace_period{nullptr}, _target_qs_counter{0} { }

	void (*on_grace_period)(qs_node *);

private:
	// Value of the QS counter at which the callback can be called.
	uint64_t _target_qs_counter;

	frg::default_list_hook<qs_node> _queue_node;
};

template<typename M>
struct qs_agent {
	qs_agent(qs_domain<M> *dom)
	: _dom{dom}, _acked_qs_counter{0}, _qs_deferred{false} {
		online();
	}

	void online() {
		assert(!_acked_qs_counter);

		uint64_t ctr;
		{
			lock_guard<M> lock(_dom->_mutex);

			_dom->_num_agents++;
//			std::cout << "Now there are " << _dom->_num_agents << " agents" << std::endl;

			// Increment the QS counter if we're the first agent.
			ctr = _dom->_qs_counter.load(std::memory_order_relaxed);
			if(_dom->_num_agents == 1) {
				assert(!_dom->_agents_to_ack.load(std::memory_order_relaxed));
				_dom->_agents_to_ack.store(1, std::memory_order_relaxed);
				_dom->_qs_counter.store(ctr + 1, std::memory_order_release);
			}
		}

		_acked_qs_counter = ctr;
	}

	void offline() {
		assert(_acked_qs_counter);

		// TODO: We need to handle this case here.
		assert(!_qs_deferred);

		{
			lock_guard<M> lock(_dom->_mutex);
			
			_dom->_num_agents--;

			// We might need to ack before going offline.
			auto ctr = _dom->_qs_counter.load(std::memory_order_relaxed);
			if(_acked_qs_counter != ctr) {
				assert(_acked_qs_counter + 1 == ctr);

				// Now ack the QS.
				if(_dom->_agents_to_ack.fetch_sub(1, std::memory_order_relaxed) == 1) {
					_dom->_agents_to_ack.store(_dom->_num_agents, std::memory_order_relaxed);
					_dom->_qs_counter.store(ctr + 1, std::memory_order_release);
				}
			}
		}

		_acked_qs_counter = 0;
	}

	void quiescent_state() {
		assert(_acked_qs_counter);

		if(_qs_deferred) {
			assert(_acked_qs_counter == _dom->_qs_counter.load(std::memory_order_relaxed));

			auto desired = _dom->_desired_qs_counter.load(std::memory_order_relaxed);
			if(desired > _acked_qs_counter) {
				lock_guard<M> lock(_dom->_mutex);
//				std::cout << "Deferred QS " << (_acked_qs_counter + 1) << ". Resetting ack counter to " << _dom->_num_agents << std::endl;
				_dom->_agents_to_ack.store(_dom->_num_agents, std::memory_order_relaxed);
				_dom->_qs_counter.store(_acked_qs_counter + 1, std::memory_order_release);

				_qs_deferred = false;
			}
		}else{
			// Check if the QS counter incremented concurrently.
			auto ctr = _dom->_qs_counter.load(std::memory_order_acquire);
			if(_acked_qs_counter != ctr) {
				assert(_acked_qs_counter + 1 == ctr);

				// Now ack the QS.
				if(_dom->_agents_to_ack.fetch_sub(1, std::memory_order_relaxed) == 1) {
					auto desired = _dom->_desired_qs_counter.load(std::memory_order_relaxed);
					if(desired > ctr) {
						lock_guard<M> lock(_dom->_mutex);
//						std::cout << "QS " << (ctr + 1) << ". Resetting ack counter to " << _dom->_num_agents << std::endl;
						_dom->_agents_to_ack.store(_dom->_num_agents, std::memory_order_relaxed);
						_dom->_qs_counter.store(ctr + 1, std::memory_order_release);
					}else{
						_qs_deferred = true;
					}
				}

				_acked_qs_counter++;
			}
		}
	}

	void quiescent_barrier() {
		// Advance the desired QS counter.
		auto target = _dom->_qs_counter.load(std::memory_order_relaxed) + 2;
		auto c = _dom->_desired_qs_counter.load(std::memory_order_relaxed);
		while(c < target) {
			if(_dom->_desired_qs_counter.compare_exchange_weak(c, target,
					std::memory_order_relaxed, std::memory_order_relaxed))
				break;
		}

		while(_dom->_qs_counter.load(std::memory_order_relaxed) < target) {
			quiescent_state();
		}
	}

	void await_barrier(qs_node *node) {
		// Advance the desired QS counter.
		auto target = _dom->_qs_counter.load(std::memory_order_relaxed) + 2;
		auto c = _dom->_desired_qs_counter.load(std::memory_order_relaxed);
		while(c < target) {
			if(_dom->_desired_qs_counter.compare_exchange_weak(c, target,
					std::memory_order_relaxed, std::memory_order_relaxed))
				break;
		}

		assert(!node->_target_qs_counter);
		node->_target_qs_counter = target;
		_pending.push_back(node);
	}

	void run() {
		auto ctr = _dom->_qs_counter.load(std::memory_order_relaxed);
		while(!_pending.empty()) {
			auto node = _pending.front();
			if(ctr < node->_target_qs_counter)
				break;
			node->_target_qs_counter = 0;
			node->on_grace_period(node);
			_pending.pop_front();
		}
	}

private:
	qs_domain<M> *_dom;

	// If this value equals the QS counter we already acked.
	uint64_t _acked_qs_counter;

	// True if this agent was the last one to ack a QS and then decided to defer it.
	// This is an optimization to prevent agents from rapidly issuing QSs
	// (as incrementing the QS counter requires taking a lock and thus impacts performance).
	bool _qs_deferred;

	frg::intrusive_list<
		qs_node,
		frg::locate_member<
			qs_node,
			frg::default_list_hook<qs_node>,
			&qs_node::_queue_node
		>
	> _pending;
};

} // namespace frg

#endif // FRG_QS_HPP
