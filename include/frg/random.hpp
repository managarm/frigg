#ifndef FRG_RANDOM_HPP
#define FRG_RANDOM_HPP

#include <frg/macros.hpp>

namespace frg FRG_VISIBILITY {

// Adopted from the Mersenne Twister reference implementation.
// Copyright of the reference implementation:
// (C) 1997 - 2002, Makoto Matsumoto and Takuji Nishimura.
// This struct is under the BSD licence.
struct mt19937 {
private:
	static constexpr int n = 624;
	static constexpr int m = 397;
	static constexpr uint32_t matrix_a = 0x9908b0df;
	static constexpr uint32_t msb = 0x80000000;
	static constexpr uint32_t lsbs = 0x7fffffff;

public:
	mt19937() {
		seed(5489);
	}

	void seed(uint32_t s) {
		_st[0] = s;
		for(_ctr = 1; _ctr < n; _ctr++)
			_st[_ctr] = (1812433253 * (_st[_ctr - 1] ^ (_st[_ctr - 1] >> 30)) + _ctr);
	}

	uint32_t operator() () {
		constexpr uint32_t mag01[2] = {0, matrix_a};

		if(_ctr >= n) {
			for(int kk = 0; kk < n - m; kk++) {
				uint32_t y = (_st[kk] & msb) | (_st[kk + 1] & lsbs);
				_st[kk] = _st[kk + m] ^ (y >> 1) ^ mag01[y & 1];
			}

			for(int kk = n - m; kk < n - 1; kk++) {
				uint32_t y = (_st[kk] & msb) | (_st[kk + 1] & lsbs);
				_st[kk] = _st[kk + (m - n)] ^ (y >> 1) ^ mag01[y & 1];
			}

			uint32_t y = (_st[n - 1] & msb) | (_st[0] & lsbs);
			_st[n - 1] = _st[m - 1] ^ (y >> 1) ^ mag01[y & 1];

			_ctr = 0;
		}

		uint32_t res = _st[_ctr++];

		res ^= (res >> 11);
		res ^= (res << 7) & 0x9d2c5680;
		res ^= (res << 15) & 0xefc60000;
		res ^= (res >> 18);

		return res;
	}

private:
	uint32_t _st[n];
	int _ctr;
};

} // namespace frg

#endif // FRG_RANDOM_HPP
