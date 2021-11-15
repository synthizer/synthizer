/**
 * Tests the accuracy of the FastSineBank.
 *
 * Constants are hardcoded in main.
 * */
#include <cstdio>
#include <math.h>
#include <vector>

#include "synthizer/config.hpp"
#include "synthizer/fast_sine_bank.hpp"
#include "synthizer/math.hpp"

using namespace synthizer;

class SlowSineBank {
public:
  SlowSineBank(double _frequency) : frequency(_frequency) {}

  void fillBlock(float *block);
  void addWave(const SineWaveConfig &wave) { this->waves.push_back(wave); }

  double frequency;
  std::vector<SineWaveConfig> waves;
  double time = 0.0;
};

void SlowSineBank::fillBlock(float *block) {
  // To match FastSineBank as much as possible, we increment time as we go, then resync it at the end.
  double old_t = this->time;

  for (std::size_t i = 0; i < config::BLOCK_SIZE; i++) {
    for (auto &w : this->waves) {
      double freq = w.freq_mul * this->frequency;
      double t = 2 * PI * (freq * this->time + w.phase);
      block[i] += ((float)w.gain) * sinf(t);
    }

    this->time = fmod(this->time + 1.0 / config::SR, 1.0);
  }

  this->time = fmod(old_t + config::BLOCK_SIZE / (double)config::SR, 1.0);
}

int main() {
  FastSineBank fast{300.0};
  SlowSineBank slow{300.0};

  std::vector<SineWaveConfig> waves{{1.0, 0.0, 0.5}, {2.0, 0.1, 0.2}, {3.0, 0.03, 0.2}, {4.0, 0.0, 0.01}};
  for (auto &w : waves) {
    slow.addWave(w);
    fast.addWave(w);
  }

  // This value was determined empirically, and then we decided whether we were good with it or not after the fact.
  double max_diff = 0.0002;
  for (std::size_t block = 0; block < 1000000; block++) {
    float slow_res[config::BLOCK_SIZE] = {0.0f};
    float fast_res[config::BLOCK_SIZE] = {0.0f};

    slow.fillBlock(slow_res);
    fast.fillBlock<config::BLOCK_SIZE, false>(fast_res);

    for (std::size_t off = 0; off < config::BLOCK_SIZE; off++) {
      double diff = slow_res[off] - fast_res[off];
      if (fabs(diff) > max_diff) {
        printf("Failed\n");
        printf("Diff at %zu is %f\n", block * config::BLOCK_SIZE + off, diff);
        printf("slow=%f fast=%f\n", slow_res[off], fast_res[off]);
        printf("Block %zu offset %zu\n", block, off);
        return 1;
      }
    }
  }

  return 0;
}
