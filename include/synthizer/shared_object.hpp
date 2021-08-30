namespace synthizer {

class SharedObjectOpenError {
public:
  SharedObjectOpenError() {}
};

class MissingSymbolError {
public:
  MissingSymbolError(const char *symbol) : symbol(symbol) {}
  const char *symbol;
};

/**
 * Class representing a shared object.
 *
 * All of the const char *in the following must live for the lifetime of the program.
 * */
class SharedObject {
public:
  SharedObject(const char *path);
  ~SharedObject();
  void *getSymbol(const char *symbol);

private:
  void *handle;
};

} // namespace synthizer
