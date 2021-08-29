#include "synthizer_constants.h"

#include "synthizer/automation_timeline.hpp"
#include "synthizer/c_api.hpp"
#include "synthizer/config.hpp"
#include "synthizer/error.hpp"
#include "synthizer/memory.hpp"

#include <algorithm>
#include <cassert>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace synthizer {

AutomationTimeline::AutomationTimeline(const std::vector<AutomationPoint> &_points) : points(_points) {
	if (points.size() == 0) {
		throw Error("Automation timelines may not have 0 points");
	}
}

void AutomationTimeline::tick() {
	if (this->finished) {
		return;
	}

	// Advance until we find a point to evaluate.
	// We could binary search but it's almost always going to be the next point.
	while (next_point != this->points.size() &&
		this->points[this->next_point].automation_time <= this->getTimeInSeconds()) {
		this->next_point++;
	}
	if (this->next_point >= this->points.size()) {
		this->current_value = std::nullopt;
		this->finished = true;
		return;
	}

	// If we're not past the first point yet, nothing to do.
	if (this->next_point == 0) {
		return;
	}

	std::size_t last_point = this->next_point - 1;
	const AutomationPoint &p1 = this->points[last_point];
	const AutomationPoint &p2 = this->points[next_point];

	// If the previous point's interplation type is none, then we may not have jumped yet.  We can just unconditionally
	// do that here, since jumping to the same value twice is not a big deal.
	if (p1.interpolation_type == SYZ_INTERPOLATION_TYPE_NONE) {
		this->current_value = p1.value;
	}

	// If p2 is NONE, we don't do anything with it until we cross it.
	if (p2.interpolation_type == SYZ_INTERPOLATION_TYPE_NONE) {
		return;
	}

	// Otherwise, we're fading to it.
	double time_diff = p2.automation_time - p1.automation_time;
	double delta = (this->getTimeInSeconds() - p1.automation_time) / time_diff;
	double w2 = delta;
	double w1 = 1.0 - w2;
	double value = w1 * p1.value + w2 * p2.value;
	this->current_value = value;
}

std::optional<double> AutomationTimeline::getValue() {
	return this->current_value;
}

double AutomationTimeline::getTimeInSeconds() {
	return this->block_time * config::BLOCK_SIZE / (double)config::SR;
}

ExposedAutomationTimeline::ExposedAutomationTimeline(std::size_t points_len,
	const struct syz_AutomationPoint *input_points) {
	if (points_len == 0) {
		throw EValidation("Timelines must have at least 1 point");
	}

	for (std::size_t i = 0; i < points_len; i++) {
		AutomationPoint ap;
		ap.automation_time = input_points[i].automation_time;
		ap.interpolation_type = input_points[i].interpolation_type;
		ap.value = input_points[i].value;
		this->points.emplace_back(std::move(ap));
	}

	std::sort(this->points.begin(), this->points.end(), [](const auto &a, const auto &b) {
		return a.automation_time < b.automation_time;
	});
}

std::shared_ptr<AutomationTimeline> ExposedAutomationTimeline::buildTimeline() {
	return allocateSharedDeferred<AutomationTimeline>(this->points);
}

}

using namespace synthizer;

SYZ_CAPI syz_ErrorCode syz_createAutomationTimeline(syz_Handle *out, unsigned int point_count,
	const struct syz_AutomationPoint *points, void *userdata, syz_UserdataFreeCallback *userdata_free_callback) {
	SYZ_PROLOGUE
	auto x = allocateSharedDeferred<ExposedAutomationTimeline>(point_count, points);
	auto c = std::static_pointer_cast<CExposable>(x);
	c->stashInternalReference(c);
	*out = toC(c);
	return syz_handleSetUserdata(*out, userdata, userdata_free_callback);
	SYZ_EPILOGUE
}
