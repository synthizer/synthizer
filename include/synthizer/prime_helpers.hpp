#pragma once

namespace synthizer {

/*
 * Helper functions for getting prime numbers.
 * */

/*
 * The maximum prime this module can ever return.
 * */
unsigned int getMaxPrime();

/*
 * get the closest prime number to a specified integer.
 * */
unsigned int getClosestPrime(unsigned int input);

/*
 * get the closest prime to a specified integer, excluding primes in the specified set.
 *
 * This is used primarily by reverb, in order to avoid duplicate primes.
 * */
unsigned int getClosestPrimeRestricted(unsigned int input, unsigned int set_size, unsigned int *set_values);
} // namespace synthizer