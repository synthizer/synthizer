#pragma once

/*
 * The following is Xoshiro256++ modified to be in a class
 * and put in the synthizer namespace.
 * 
 * Most comments are the original.
 * */


/*  Written in 2019 by David Blackman and Sebastiano Vigna (vigna@acm.org)

To the extent possible under law, the author has dedicated all copyright
and related and neighboring rights to this software to the public domain
worldwide. This software is distributed without any warranty.

See <http://creativecommons.org/publicdomain/zero/1.0/>. */

#include <array>
#include <cstdint>

namespace synthizer::xoshiro {

/* This is xoshiro256++ 1.0, one of our all-purpose, rock-solid generators.
   It has excellent (sub-ns) speed, a state (256 bits) that is large
   enough for any parallel application, and it passes all tests we are
   aware of.

   For generating just floating-point numbers, xoshiro256+ is even faster.

   The state must be seeded so that it is not everywhere zero. If you have
   a 64-bit seed, we suggest to seed a splitmix64 generator and use its
   output to fill s. */

static inline std::uint64_t rotl(const uint64_t x, int k) {
	return (x << k) | (x >> (64 - k));
}

class Xoshiro256PlusPlus {
	private:
	std::uint64_t s[4];

	public:
	Xoshiro256PlusPlus(std::array<std::uint64_t, 4> seed) {
		this->s[0] = seed[0];
		this->s[1] = seed[1];
		this->s[2] = seed[2];
		this->s[3] = seed[3];
	}

	std::uint64_t next() {
		const std::uint64_t result = rotl(s[0] + s[3], 23) + s[0];

		const std::uint64_t t = s[1] << 17;

		s[2] ^= s[0];
		s[3] ^= s[1];
		s[1] ^= s[2];
		s[0] ^= s[3];

		s[2] ^= t;

		s[3] = rotl(s[3], 45);

		return result;
	}


	/* This is the jump function for the generator. It is equivalent
	to 2^128 calls to next(); it can be used to generate 2^128
	non-overlapping subsequences for parallel computations. */
	void jump(void) {
		static const std::uint64_t JUMP[] = { 0x180ec6d33cfd0aba, 0xd5a61266f0c9392c, 0xa9582618e03fc9aa, 0x39abdc4529b1661c };

		std::uint64_t s0 = 0;
		std::uint64_t s1 = 0;
		std::uint64_t s2 = 0;
		std::uint64_t s3 = 0;
		for(unsigned int i = 0; i < sizeof JUMP / sizeof *JUMP; i++)
			for(int b = 0; b < 64; b++) {
				if (JUMP[i] & (std::uint64_t(1)) << b) {
					s0 ^= s[0];
					s1 ^= s[1];
					s2 ^= s[2];
					s3 ^= s[3];
				}
				next();	
			}
			
		s[0] = s0;
		s[1] = s1;
		s[2] = s2;
		s[3] = s3;
	}


	/* This is the long-jump function for the generator. It is equivalent to
	2^192 calls to next(); it can be used to generate 2^64 starting points,
	from each of which jump() will generate 2^64 non-overlapping
	subsequences for parallel distributed computations. */
	void long_jump(void) {
		static const std::uint64_t LONG_JUMP[] = { 0x76e15d3efefdcbbf, 0xc5004e441c522fb3, 0x77710069854ee241, 0x39109bb02acbe635 };

		std::uint64_t s0 = 0;
		std::uint64_t s1 = 0;
		std::uint64_t s2 = 0;
		std::uint64_t s3 = 0;
		for(unsigned int i = 0; i < sizeof LONG_JUMP / sizeof *LONG_JUMP; i++)
			for(int b = 0; b < 64; b++) {
				if (LONG_JUMP[i] & std::uint64_t(1) << b) {
					s0 ^= s[0];
					s1 ^= s[1];
					s2 ^= s[2];
					s3 ^= s[3];
				}
				next();	
			}
			
		s[0] = s0;
		s[1] = s1;
		s[2] = s2;
		s[3] = s3;
	}
};

}