#include "synthizer/sources.hpp"

#include "synthizer/config.hpp"
#include "synthizer/context.hpp"
#include "synthizer/generator.hpp"
#include "synthizer/types.hpp"
#include "synthizer/vector_helpers.hpp"

#include <algorithm>
#include <array>
#include <memory>
#include <vector>

namespace synthizer {

void Source::addGenerator(std::shared_ptr<Generator> &generator) {
	if (this->hasGenerator(generator)) return;
	this->generators.emplace_back(generator);
}

void Source::removeGenerator(std::shared_ptr<Generator> &generator) {
	if (this->generators.empty()) return;
	if (this->hasGenerator(generator) == false) return;

	unsigned int index = 0;
	for(; index < this->generators.size(); index++) {
		auto s = this->generators[index].lock();
		if (s == generator) break;
	}

	std::swap(this->generators[this->generators.size()-1], this->generators[index]);
	this->generators.resize(this->generators.size() - 1);
}

bool Source::hasGenerator(std::shared_ptr<Generator> &generator) {
	return weak_vector::contains(this->generators, generator);
}

}