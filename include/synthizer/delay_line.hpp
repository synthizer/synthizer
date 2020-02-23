#pragma once

#include <type_traits>
#include <array>

namespace synthizer {

	template<typename T, unsigned int SIZE, typename ENABLED=void>
	class DelayLine;

	// The enable_if uses bit tricks to only enable delay lines for powers of 2.
	template<typename T, unsigned int SIZE>
	class DelayLine<T, SIZE, typename std::enable_if<(SIZE > 0 && (SIZE&(SIZE-1)) == 0), void>::type> {
		public:
		T read(unsigned int delay) {
			return this->_delay[(this->_position-delay)&(SIZE-1)];
		}

		// Usually called with 0 but enabling the option to call it with something else might one day prove useful.
		void write(T &val, unsigned int offset=0) {
			this->_delay[(this->_position+offset)&(SIZE-1)] = val;
		}

		void advance() {
			this->_position++;
		}

		private:
		unsigned int _position = 0;
		std::array<T, SIZE> _delay;
	};
}
