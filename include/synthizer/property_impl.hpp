/*
 * This is the property implementation machinery. To use:
 *
 * At the top of the header defining your type: #include "synthizer/property_internals.hpp" which will define some types
 * and bring in any headers this file needs.
 *
 * Then, in your class header, at the public access level, define the following macros:
 *
 * ```
 * #define PROPERTY_CLASS myclass
 * #define PROPERTY_LIST proplist
 * #define PROPERTY_BASE baseclass
 * ```
 *
 * Then include this header. Afterwords, the above macros will be undefined, you will be at public access level, and
 * your class will have the following functionality inline:
 *
 * - getProperty: read the property of the specified type. Can be used from any thread. makes no system calls but may
 *    block for object properties.
 *  - setProperty: Must be called from the audio thread. Sets the property. makes no system calls but may block for
 *    object properties.  Accepts a defaulted second parameter to disable change tracking, used from the audio thread in
 *    order to propagate changes out without creating cycles, e.g. exposing buffer position while also allowing it to be
 *    written without the audio thread's write counting.
 *  - verifyProperty: Verifies the property is in range. Makes no system calls.
 *  - acquireProperty(proptype &out): sets out to the value of property, and returns true if it has changed since the
 *    last time this specific property was acquired. May block for object properties.
 * - markPropertyUnchanged: clear the changed status of a propperty. Used primarily from initInAudioThread for objects
 *   which need to see properties as not having been changed on their first tick.
 * - All the automation machinery.
 *
 * Note that blocking on object properties is required because we need to safely copy std::shared_ptr. We do so with a
 * simple spinlock, under the assumption that readers are rare.
 *
 * In the above, Property changes to match the name of the property. In addition to these functions, this header defines
 * a number of private types, adds a nested class named clasnameProps and field classname_props to your class at every
 * level of the hierarchy that uses properties which you shouldn't access yourself, and defines the standard property
 * machinery to get/set via enum.
 *
 * A weakness of this implementation is that it is not possible to validate properties with more complex logic. To get
 * around this, objects should either expose complicated properties as functions or force properties to valid values and
 * potentially log in the audio tick. Synthizer designs around not needing to perform such logic as a rule in order to
 * avoid entangling things with the audio thread.
 *
 * Finally, note that properties count as changed on the first tick, unless markPropertyUnchanged is called.
 * */

#ifndef PROPERTY_CLASS
#error "When implementing properties, must define PROPERTY_CLASS to the class the properties are being added to."
#endif

#ifndef PROPERTY_BASE
#error "Forgot to define PROPERTY_BASE when implementing properties. Define this to your immediate base class."
#endif

#ifndef PROPERTY_LIST
#error "Need PROPERTY_LIST defined to know where to get properties from"
#endif

#define PROPCLASS_NAME PROPERTY_CLASS##Props
#define PROPFIELD_NAME PROPERTY_CLASS##_props

/* This class holds the property fields themselves. */
class PROPCLASS_NAME {
public:
/* DV is default value. */
#define INT_P(IGNORED, N, IGNORED2, IGNORED3, IGNORED4, DV) IntProperty N{DV};
#define DOUBLE_P(IGNORED, N, IGNORED2, IGNORED3, IGNORED4, DV) DoubleProperty N{DV};
#define DOUBLE3_P(IGNORED, N, IGNORED2, DV1, DV2, DV3) Double3Property N{{DV1, DV2, DV3}};
#define DOUBLE6_P(IGNORED, N, IGNORED2, DV1, DV2, DV3, DV4, DV5, DV6) Double6Property N{{DV1, DV2, DV3, DV4, DV5, DV6}};
#define OBJECT_P(IGNORED, N, IGNORED2, CLS) ObjectProperty<CLS> N;
#define BIQUAD_P(IGNORED, N, ...) BiquadProperty N{};

  PROPERTY_LIST
};

#undef INT_P
#undef DOUBLE_P
#undef DOUBLE3_P
#undef DOUBLE6_P
#undef OBJECT_P
#undef BIQUAD_P

PROPCLASS_NAME PROPFIELD_NAME{};

#define STANDARD_READ(F) return this->PROPFIELD_NAME.F.read();
#define STANDARD_WRITE(F) this->PROPFIELD_NAME.F.write(val, track_change);
#define STANDARD_ACQUIRE(F) return this->PROPFIELD_NAME.F.acquire(&out);
#define STANDARD_UNCHANGED(F) this->PROPFIELD_NAME.F.markUnchanged();

#define AUTOMATED_WRITE(F) this->PROPFIELD_NAME.F.writeAutomated(this->getAutomationTimeInSamples(), val);

/* Now, define all the methods. */
#define INT_P(E, UNDERSCORE_NAME, CAMEL_NAME, MIN, MAX, DV)                                                            \
  int get##CAMEL_NAME() const { STANDARD_READ(UNDERSCORE_NAME) }                                                       \
                                                                                                                       \
  void set##CAMEL_NAME(int val, bool track_change = true) { STANDARD_WRITE(UNDERSCORE_NAME) }                          \
                                                                                                                       \
  void validate##CAMEL_NAME(const int value) const {                                                                   \
    if (value < MIN || value > MAX) {                                                                                  \
      throw ERange();                                                                                                  \
    }                                                                                                                  \
  }                                                                                                                    \
                                                                                                                       \
  bool acquire##CAMEL_NAME(int &out) { STANDARD_ACQUIRE(UNDERSCORE_NAME) }                                             \
                                                                                                                       \
  void mark##CAMEL_NAME##Unchanged() { STANDARD_UNCHANGED(UNDERSCORE_NAME); }

#define DOUBLE_P(E, UNDERSCORE_NAME, CAMEL_NAME, MIN, MAX, DV)                                                         \
  double get##CAMEL_NAME() const { STANDARD_READ(UNDERSCORE_NAME) }                                                    \
                                                                                                                       \
  void set##CAMEL_NAME(double val, bool track_change = true) { STANDARD_WRITE(UNDERSCORE_NAME) }                       \
                                                                                                                       \
  void set##CAMEL_NAME##Automated(double val) { AUTOMATED_WRITE(UNDERSCORE_NAME) }                                     \
                                                                                                                       \
  void validate##CAMEL_NAME(const double value) const {                                                                \
    if (value < MIN || value > MAX) {                                                                                  \
      throw ERange();                                                                                                  \
    }                                                                                                                  \
  }                                                                                                                    \
                                                                                                                       \
  bool acquire##CAMEL_NAME(double &out) { STANDARD_ACQUIRE(UNDERSCORE_NAME) }                                          \
                                                                                                                       \
  void mark##CAMEL_NAME##Unchanged() { STANDARD_UNCHANGED(UNDERSCORE_NAME); }                                          \
                                                                                                                       \
  std::optional<double> get##CAMEL_NAME##NextBlockValue() {                                                            \
    return this->PROPFIELD_NAME.UNDERSCORE_NAME.getValueForNextBlock();                                                \
  }

#define DOUBLE3_P(E, UNDERSCORE_NAME, CAMEL_NAME, ...)                                                                 \
  std::array<double, 3> get##CAMEL_NAME() const { STANDARD_READ(UNDERSCORE_NAME) }                                     \
                                                                                                                       \
  void set##CAMEL_NAME(const std::array<double, 3> &val, bool track_change = true) { STANDARD_WRITE(UNDERSCORE_NAME) } \
                                                                                                                       \
  void set##CAMEL_NAME##Automated(const std::array<double, 3> &val) { AUTOMATED_WRITE(UNDERSCORE_NAME) }               \
                                                                                                                       \
  void validate##CAMEL_NAME(const std::array<double, 3> &value) const {                                                \
    (void)value;                                                                                                       \
    /* Nothing to do. */                                                                                               \
  }                                                                                                                    \
                                                                                                                       \
  bool acquire##CAMEL_NAME(std::array<double, 3> &out) { STANDARD_ACQUIRE(UNDERSCORE_NAME) }                           \
                                                                                                                       \
  void mark##CAMEL_NAME##Unchanged() { STANDARD_UNCHANGED(UNDERSCORE_NAME); }

#define DOUBLE6_P(E, UNDERSCORE_NAME, CAMEL_NAME, ...)                                                                 \
  std::array<double, 6> get##CAMEL_NAME() const { STANDARD_READ(UNDERSCORE_NAME) }                                     \
                                                                                                                       \
  void set##CAMEL_NAME(const std::array<double, 6> &val, bool track_change = true) { STANDARD_WRITE(UNDERSCORE_NAME) } \
                                                                                                                       \
  void set##CAMEL_NAME##Automated(const std::array<double, 6> &val) { AUTOMATED_WRITE(UNDERSCORE_NAME) }               \
                                                                                                                       \
  void validate##CAMEL_NAME(const std::array<double, 6> &value) const {                                                \
    (void)value;                                                                                                       \
    /* Nothing to do. */                                                                                               \
  }                                                                                                                    \
                                                                                                                       \
  bool acquire##CAMEL_NAME(std::array<double, 6> &out) { STANDARD_ACQUIRE(UNDERSCORE_NAME) }                           \
                                                                                                                       \
  void mark##CAMEL_NAME##Unchanged() { STANDARD_UNCHANGED(UNDERSCORE_NAME); }

#define OBJECT_P(ENUM, UNDERSCORE_NAME, CAMEL_NAME, CLS)                                                               \
  std::weak_ptr<CLS> get##CAMEL_NAME() const { STANDARD_READ(UNDERSCORE_NAME) }                                        \
                                                                                                                       \
  void set##CAMEL_NAME(const std::weak_ptr<CLS> &val, bool track_change = true) { STANDARD_WRITE(UNDERSCORE_NAME) }    \
                                                                                                                       \
  void validate##CAMEL_NAME(const std::weak_ptr<CExposable> &val) const {                                              \
    std::shared_ptr<CExposable> obj = val.lock();                                                                      \
    if (obj != nullptr) {                                                                                              \
      if (std::dynamic_pointer_cast<CLS>(obj) == nullptr) {                                                            \
        throw EHandleType();                                                                                           \
      }                                                                                                                \
    }                                                                                                                  \
  }                                                                                                                    \
                                                                                                                       \
  bool acquire##CAMEL_NAME(std::weak_ptr<CLS> &out) { STANDARD_ACQUIRE(UNDERSCORE_NAME) }                              \
                                                                                                                       \
  void mark##CAMEL_NAME##Unchanged() { STANDARD_UNCHANGED(UNDERSCORE_NAME); }

#define BIQUAD_P(E, UNDERSCORE_NAME, CAMEL_NAME)                                                                       \
  struct syz_BiquadConfig get##CAMEL_NAME() const {                                                                    \
    STANDARD_READ(UNDERSCORE_NAME)                                                                                     \
  }                                                                                                                    \
                                                                                                                       \
  void set##CAMEL_NAME(const struct syz_BiquadConfig &val, bool track_change = true) {                                 \
    STANDARD_WRITE(UNDERSCORE_NAME)                                                                                    \
  }                                                                                                                    \
                                                                                                                       \
  void validate##CAMEL_NAME(const struct syz_BiquadConfig &value) const {                                              \
    (void)value;                                                                                                       \
    return;                                                                                                            \
  }                                                                                                                    \
                                                                                                                       \
  bool acquire##CAMEL_NAME(struct syz_BiquadConfig &out) { STANDARD_ACQUIRE(UNDERSCORE_NAME) }                         \
                                                                                                                       \
  void mark##CAMEL_NAME##Unchanged() { STANDARD_UNCHANGED(UNDERSCORE_NAME); }

PROPERTY_LIST

#undef STANDARD_READ
#undef STANDARD_WRITE
#undef STANDARD_ACQUIRE
#undef INT_P
#undef DOUBLE_P
#undef DOUBLE3_P
#undef DOUBLE6_P
#undef OBJECT_P
#undef BIQUAD_P

/*
 * Now that we have all that, we have to define the methods that the C API uses.
 * */
#define HAS_(P, ...)                                                                                                   \
  case (P):                                                                                                            \
    return true;

#define GET_CONV_(T, P, CAMEL_NAME, CONV)                                                                              \
  case (P): {                                                                                                          \
    auto tmp = this->get##CAMEL_NAME();                                                                                \
    property_impl::PropertyValue ret;                                                                                  \
    ret.emplace<T>(CONV(tmp));                                                                                         \
    return ret;                                                                                                        \
  } break;

#define GET_(T, P, CAMEL_NAME) GET_CONV_(T, P, CAMEL_NAME, [](auto &x) { return x; })

#define VALIDATE_(T, P, CAMEL_NAME, ...)                                                                               \
  case (P): {                                                                                                          \
    auto ptr = std::get_if<T>(&value);                                                                                 \
    if (ptr == nullptr)                                                                                                \
      throw EPropertyType();                                                                                           \
    this->validate##CAMEL_NAME(*ptr);                                                                                  \
    break;                                                                                                             \
  }

#define SET_CONV_(T, P, CAMEL_NAME, CONV, ...)                                                                         \
  case (P): {                                                                                                          \
    auto ptr = std::get_if<T>(&value);                                                                                 \
    if (ptr == nullptr)                                                                                                \
      throw EPropertyType();                                                                                           \
    this->set##CAMEL_NAME(CONV(*ptr));                                                                                 \
  } break;

#define SET_CONV_AUTOMATED_(T, P, CAMEL_NAME, CONV, ...)                                                               \
  case (P): {                                                                                                          \
    auto ptr = std::get_if<T>(&value);                                                                                 \
    if (ptr == nullptr)                                                                                                \
      throw EPropertyType();                                                                                           \
    this->set##CAMEL_NAME##Automated(CONV(*ptr));                                                                      \
  } break;

#define SET_(T, P, CAMEL_NAME) SET_CONV_(T, P, CAMEL_NAME, [](auto &v) { return v; })
#define SET_AUTOMATED_(T, P, CAMEL_NAME) SET_CONV_AUTOMATED_(T, P, CAMEL_NAME, [](auto &v) { return v; })

/* Now implement the methods. */

#define INT_P(...) HAS_(__VA_ARGS__)
#define DOUBLE_P(...) HAS_(__VA_ARGS__)
#define OBJECT_P(...) HAS_(__VA_ARGS__)
#define DOUBLE3_P(...) HAS_(__VA_ARGS__)
#define DOUBLE6_P(...) HAS_(__VA_ARGS__)
#define BIQUAD_P(...) HAS_(__VA_ARGS__)

bool hasProperty(int property) override {
  switch (property) {
    PROPERTY_LIST
  default:
    return PROPERTY_BASE::hasProperty(property);
  }
}

#undef INT_P
#undef DOUBLE_P
#undef DOUBLE3_P
#undef DOUBLE6_P
#undef OBJECT_P
#undef BIQUAD_P

#define INT_P(P, UNDERSCORE_NAME, CAMEL_NAME, ...) GET_(int, P, CAMEL_NAME)
#define DOUBLE_P(P, UNDERSCORE_NAME, CAMEL_NAME, ...) GET_(double, P, CAMEL_NAME)
#define DOUBLE3_P(P, UNDERSCORE_NAME, CAMEL_NAME, ...) GET_(property_impl::arrayd3, P, CAMEL_NAME)
#define DOUBLE6_P(P, UNDERSCORE_NAME, CAMEL_NAME, ...) GET_(property_impl::arrayd6, P, CAMEL_NAME)
#define OBJECT_P(P, UNDERSCORE_NAME, CAMEL_NAME, ...)                                                                  \
  GET_CONV_(std::shared_ptr<CExposable>, P, CAMEL_NAME, [](auto &x) -> std::shared_ptr<CExposable> {                   \
    auto strong = x.lock();                                                                                            \
    return std::static_pointer_cast<CExposable>(strong);                                                               \
  });
#define BIQUAD_P(P, UNDERSCORE_NAME, CAMEL_NAME) GET_(struct syz_BiquadConfig, P, CAMEL_NAME)

property_impl::PropertyValue getProperty(int property) override {
  switch (property) {
    PROPERTY_LIST
  default:
    return PROPERTY_BASE::getProperty(property);
  }
}

#undef INT_P
#undef DOUBLE_P
#undef DOUBLE3_P
#undef DOUBLE6_P
#undef OBJECT_P
#undef BIQUAD_P

#define INT_P(P, UNDERSCORE_NAME, CAMEL_NAME, ...) VALIDATE_(int, P, CAMEL_NAME)
#define DOUBLE_P(P, UNDERSCORE_NAME, CAMEL_NAME, ...) VALIDATE_(double, P, CAMEL_NAME)
#define OBJECT_P(P, UNDERSCORE_NAME, CAMEL_NAME, ...) VALIDATE_(std::shared_ptr<CExposable>, P, CAMEL_NAME)
#define DOUBLE3_P(P, UNDERSCORE_NAME, CAMEL_NAME, ...) VALIDATE_(property_impl::arrayd3, P, CAMEL_NAME)
#define DOUBLE6_P(P, UNDERSCORE_NAME, CAMEL_NAME, ...) VALIDATE_(property_impl::arrayd6, P, CAMEL_NAME)
#define BIQUAD_P(P, UNDERSCORE_NAME, CAMEL_NAME) VALIDATE_(struct syz_BiquadConfig, P, CAMEL_NAME)

void validateProperty(int property, const property_impl::PropertyValue &value) override {
  switch (property) {
    PROPERTY_LIST
  default:
    PROPERTY_BASE::validateProperty(property, value);
  }
}

#undef INT_P
#undef DOUBLE_P
#undef DOUBLE3_P
#undef DOUBLE6_P
#undef OBJECT_P
#undef BIQUAD_P

#define INT_P(P, UNDERSCORE_NAME, CAMEL_NAME, ...) SET_(int, P, CAMEL_NAME)
#define DOUBLE_P(P, UNDERSCORE_NAME, CAMEL_NAME, ...) SET_AUTOMATED_(double, P, CAMEL_NAME)
#define OBJECT_P(P, UNDERSCORE_NAME, CAMEL_NAME, CLS)                                                                  \
  SET_CONV_(std::shared_ptr<CExposable>, P, CAMEL_NAME, ([](auto &v) -> auto {                                         \
              /* validated by the validator; guaranteed to be valid here. */                                           \
              return std::static_pointer_cast<CLS>(v);                                                                 \
            }))
#define DOUBLE3_P(P, UNDERSCORE_NAME, CAMEL_NAME, ...) SET_AUTOMATED_(property_impl::arrayd3, P, CAMEL_NAME)
#define DOUBLE6_P(P, UNDERSCORE_NAME, CAMEL_NAME, ...) SET_AUTOMATED_(property_impl::arrayd6, P, CAMEL_NAME)
#define BIQUAD_P(P, UNDERSCORE_NAME, CAMEL_NAME) SET_(struct syz_BiquadConfig, P, CAMEL_NAME)

void setProperty(int property, const property_impl::PropertyValue &value) override {
  switch (property) {
    PROPERTY_LIST
  default:
    PROPERTY_BASE::setProperty(property, value);
  }
}

#undef INT_P
#undef DOUBLE_P
#undef DOUBLE3_P
#undef DOUBLE6_P
#undef OBJECT_P
#undef BIQUAD_P

// All the automation stuff doesn't use these; leave them defined to empty until we're done.
#define INT_P(...)
#define OBJECT_P(...)
#define BIQUAD_P(...)

void propSubsystemAdvanceAutomation() override {
#define ADVANCER(N) this->PROPFIELD_NAME.N.tickAutomation(this->getAutomationTimeInSamples());

#define DOUBLE_P(C, N, ...) ADVANCER(N)
#define DOUBLE3_P(C, N, ...) ADVANCER(N)
#define DOUBLE6_P(C, N, ...) ADVANCER(N)

  PROPERTY_LIST
  PROPERTY_BASE::propSubsystemAdvanceAutomation();
}

#undef DOUBLE_P
#undef DOUBLE3_P
#undef DOUBLE6_P
#undef TMP
#undef TMP2

void validateAutomation(int property, std::optional<const PropertyAutomationPoint<6> *> point) override {
#define VAL_HELPER(V, CAMEL_N, EXTR)                                                                                   \
  case V:                                                                                                              \
    if (point) {                                                                                                       \
      EXTR this->validate##CAMEL_N(value);                                                                             \
    }                                                                                                                  \
    return;
#define DOUBLE_P(C, IGN, CAMEL_N, ...) VAL_HELPER(C, CAMEL_N, double value = (*point)->values[0];)
#define TMP std::array<double, 3> value{{(*point)->values[0], (*point)->values[1], (*point)->values[2]}};
#define DOUBLE3_P(C, IGN, CAMEL_N, ...) VAL_HELPER(C, CAMEL_N, TMP)
#define TMP2 std::array<double, 6> value = (*point)->values;
#define DOUBLE6_P(C, IGN, CAMEL_N, ...) VAL_HELPER(C, CAMEL_N, TMP2)

  switch (property) {
    PROPERTY_LIST
    // Some objects don't have any properties; silence a warning.
    // Also some objects don't have automatable properties.
  case INT_MAX:
  default:
    break;
  }

  PROPERTY_BASE::validateAutomation(property, point);
}

#undef DOUBLE_P
#undef DOUBLE3_P
#undef DOUBLE6_P
#undef TMP
#undef TMP2

void clearAutomationForProperty(int property) override {
#define CLEARER(C, UNDER_N, ...)                                                                                       \
  case C:                                                                                                              \
    this->PROPFIELD_NAME.UNDER_N.getTimeline()->clear();                                                               \
    return;
#define DOUBLE_P(...) CLEARER(__VA_ARGS__)
#define DOUBLE3_P(...) CLEARER(__VA_ARGS__)
#define DOUBLE6_P(...) CLEARER(__VA_ARGS__)

  switch (property) {
    PROPERTY_LIST
  case INT_MAX:
  default:
    break;
  }

  PROPERTY_BASE::clearAutomationForProperty(property);
}

#undef DOUBLE_P
#undef DOUBLE3_P
#undef DOUBLE6_P
#undef CLEARER

void applyPropertyAutomationPoints(int property, std::size_t points_len, PropertyAutomationPoint<6> *points) override {
#define APPLIER(C, UNDER_N, ...)                                                                                       \
  case C: {                                                                                                            \
    auto t = this->PROPFIELD_NAME.UNDER_N.getTimeline();                                                               \
    t->addPoints(points, points + points_len);                                                                         \
    return;                                                                                                            \
  }
#define DOUBLE_P(...) APPLIER(__VA_ARGS__)
#define DOUBLE3_P(...) APPLIER(__VA_ARGS__)
#define DOUBLE6_P(...) APPLIER(__VA_ARGS__)

  switch (property) {
    PROPERTY_LIST
  case INT_MAX:
  default:
    break;
  }

  PROPERTY_BASE::applyPropertyAutomationPoints(property, points_len, points);
}

#undef DOUBLE_P
#undef DOUBLE3_P
#undef DOUBLE6_P
#undef APPLIER

void clearAllPropertyAutomation() {
#define CLEARER(IGN, UNDER_N, ...) this->PROPFIELD_NAME.UNDER_N.getTimeline()->clear();
#define DOUBLE_P(...) CLEARER(__VA_ARGS__)
#define DOUBLE3_P(...) CLEARER(__VA_ARGS__)
#define DOUBLE6_P(...) CLEARER(__VA_ARGS__)

  PROPERTY_LIST

  PROPERTY_BASE::clearAllPropertyAutomation();
}

#undef PROPERTY_CLASS
#undef PROPERTY_BASE
#undef PROPERTY_LIST
#undef INT_P
#undef DOUBLE_P
#undef OBJECT_P
#undef DOUBLE3_P
#undef DOUBLE6_P
#undef BIQUAD_P
#undef HAS_
#undef GET_
#undef GET_CONV_
#undef VALIDATE_
#undef SET_
#undef SET_CONV_
#undef PROPCLASS_NAME
#undef PROPFIELD_NAME
#undef PROPERTY_CLASS
#undef PROPERTY_LIST
#undef PROPERTY_BASE
#undef CLEARER
