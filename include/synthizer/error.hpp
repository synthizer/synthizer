#include <string>

namespace synthizer {

	/*
	 * Base class for all synthizer errors.
	 * 
	 * When Synthizer encounters an error, it throws an instance of this class,  which will be translated by the C API into error codes and messages.
	 * 
	 * Since we don't have a C API yet, right now this doesn't have an error code.
	 * */
class Error {
	public:
	Error(std::string message);
	virtual const std::string &getMessage() const;
	virtual ~Error() {}

	template<typename T>
	bool is() {
		return dynamic_cast<T*>(this) != nullptr;
	}

	template<typename T>
	T* as() {
		auto out = std::dynamic_)cast<T>(this);
		return out;
	}

	private:
	std::string message;
};

}