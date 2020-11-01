import pyprimes

import array_writer
import hrtf_writer
import mit

hrtf_writer.write_hrtf_data(mit.hrtf_data)

def primes(limit):
    """Get all primes including the first prime above the specified threshold."""
    for i in pyprimes.primes():
        if i > limit:
            yield i
            return
        yield i

arrays = array_writer.ArrayWriter()
#We primarily use primes for delay line lengths, and our default sampling rate is 44100. Give us at least 5 seconds worth.
arrays.add_array("primes", "unsigned int", primes(44100 * 5))
arrays.write_arrays()
