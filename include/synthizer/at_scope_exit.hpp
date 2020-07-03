#pragma once

namespace synthizer {

template<typename CALLABLE>
class AtScopeExit {
	public:
	AtScopeExit(CALLABLE callable): callable(callable) {}
	~AtScopeExit() { callable(); }
	private:
	CALLABLE callable;
};


}