#ifndef ER4COMMLIB_GLOBAL_ADDENDUM_H
#define ER4COMMLIB_GLOBAL_ADDENDUM_H

#include <vector>
#include <string>
#include <math.h>
#include <limits>
#include <unordered_map>

#include "er4commlib_global.h"

#ifndef ER4COMMLIB_LABVIEW_WRAPPER
namespace er4CommLib {
#endif

/*! \brief Increments a prefix by 1.
 *
 * \param value [out] incremented prefix.
 * \return true if the prefix was successfully incremented; false if the prefix has already the maximum possible value.
 */
static inline bool incrementUnit(ER4CL_ARGOUT UnitPfx_t &value);

/*! \brief Increments a prefix by n.
 *
 * \param value [out] incremented prefix.
 * \param n [in] increment.
 * \return true if the prefix was successfully incremented; false if the prefix surpasses the maximum possible value.
 */
static inline bool incrementUnit(ER4CL_ARGOUT UnitPfx_t &value, ER4CL_ARGIN int n);

/*! \brief Decrements a prefix by 1.
 *
 * \param value [out] decremented prefix.
 * \return true if the prefix was successfully decremented; false if the prefix has already the minimum possible value.
 */
static inline bool decrementUnit(ER4CL_ARGOUT UnitPfx_t &value);

/*! \brief Decrements a prefix by n.
 *
 * \param value [out] decremented prefix.
 * \param n [in] decrement.
 * \return true if the prefix was successfully decremented; false if the prefix surpasses the minimum possible value.
 */
static inline bool decrementUnit(ER4CL_ARGOUT UnitPfx_t &value, ER4CL_ARGIN int n);

static inline bool incrementUnit(ER4CL_ARGOUT UnitPfx_t &value) {
    if (value < UnitPfxNum-1) {
        int intValue = static_cast <int> (value) +1;
        value = static_cast <UnitPfx_t> (intValue);
        return true;

    } else {
        return false;
    }
}

static inline bool incrementUnit(ER4CL_ARGOUT UnitPfx_t &value, ER4CL_ARGIN int n) {
    if (value < UnitPfxNum-n) {
        int intValue = static_cast <int> (value) +n;
        value = static_cast <UnitPfx_t> (intValue);
        return true;

    } else {
        value = UnitPfxNum;
        decrementUnit(value);
        return false;
    }
}

static inline bool decrementUnit(ER4CL_ARGOUT UnitPfx_t &value) {
    if (value > 0) {
        int intValue = static_cast <int> (value) -1;
        value = static_cast <UnitPfx_t> (intValue);
        return true;

    } else {
        return false;
    }
}

static inline bool decrementUnit(ER4CL_ARGOUT UnitPfx_t &value, ER4CL_ARGIN int n) {
    if (value > n-1) {
        int intValue = static_cast <int> (value) -n;
        value = static_cast <UnitPfx_t> (intValue);
        return true;

    } else {
        value = static_cast <UnitPfx_t> (0);
        return false;
    }
}

/*! \brief Array with the strings corresponding to the unit prefixes of type UnitPfx_t. */
static const std::string unitPrefixes[UnitPfxNum] = {
    "f",
    "p",
    "n",
    "u",
    "m",
    " ",
    "k",
    "M",
    "G",
    "T",
    "P"
};

/*! \brief Convenience array with the precomputed powers of 1000 used to make unit conversions. */
static const double powersOf1000[UnitPfxNum] = {
    1.0,
    1.0e3,
    1.0e6,
    1.0e9,
    1.0e12,
    1.0e15,
    1.0e18,
    1.0e21,
    1.0e24,
    1.0e27,
    1.0e30
};

/*! \brief Returns the string corresponding to the prefix.
 *
 * \return String corresponding to the prefix.
 */
inline std::string getPrefix(UnitPfx_t prefix) {
    return unitPrefixes[prefix];
}

/*! \brief Returns the prefix corresponding to the string.
 *  \note Returns UnitPfxNum if the string is not found
 *
 * \return String corresponding to the prefix.
 */
inline UnitPfx_t getPrefixString(std::string text) {
    unsigned int idx;
    for (idx = 0; idx < UnitPfxNum; idx++) {
        if (text == unitPrefixes[idx]) {
            break;
        }
    }
    return static_cast <UnitPfx_t> (idx);
}

/*! \typedef Measurement_t
 */

/*! \struct Measurement_t
 * \brief Structure used manage physical quantities that define a value with its unit and unit prefix.
 */
typedef struct Measurement {
    double value; /*!< Numerical value. */
    UnitPfx_t prefix; /*!< Unit prefix in the range [femto, Peta]. */
    std::string unit; /*!< Unit. \note Can be any string, the library is not aware of real units meaning. */

    /*! \brief Returns the value without prefix.
     *
     * \return Value without prefix, e.g. for 1.5nA returns 1.5e-9.
     */
    double getNoPrefixValue(ER4CL_ARGVOID) {
        return value*multiplier();
    }

    /*! \brief Returns the string corresponding to the prefix.
     *
     * \return String corresponding to the prefix.
     */
    std::string getPrefix(ER4CL_ARGVOID) {
        return unitPrefixes[prefix];
    }

    /*! \brief Returns the string corresponding to the unit with the prefix.
     *
     * \return String corresponding to the unit with the prefix.
     */
    std::string getFullUnit(ER4CL_ARGVOID) {
        return unitPrefixes[prefix] + unit;
    }

    /*! \brief Returns the prefix multiplier.
     *
     * \return Prefix multiplier, e.g. 10^-6 for micro.
     */
    double multiplier(ER4CL_ARGVOID) {
        unsigned int delta;
        bool deltaPositive;

        if (prefix > UnitPfxNone) {
            delta = prefix-UnitPfxNone;
            deltaPositive = true;

        } else {
            delta = UnitPfxNone-prefix;
            deltaPositive = false;
        }

        if (deltaPositive) {
            return powersOf1000[delta];

        } else {
            return 1.0/powersOf1000[delta];
        }
    }

    /*! \brief Returns a string describing the value.
     *
     * \param maxChars [in] maximum number of characters for the returned string.
     * \return String describing the value.
     */
    std::string valueLabel(ER4CL_ARGIN unsigned int maxChars = 8) {
        std::string valueSt = std::to_string(value);
        if (valueSt.length() >= maxChars) {
            valueSt.erase(maxChars);
        }
        size_t dot = valueSt.find_last_of(".");
        size_t notZero = valueSt.find_last_not_of("0");

        if (notZero != std::string::npos) {
            if (dot != std::string::npos) {
                return valueSt.erase(dot < notZero ? notZero+1 : dot);

            } else {
                return valueSt;
            }

        } else {
            return "0";
        }
    }

    /*! \brief Returns a string describing the value with its prefix and unit.
     *
     * \param maxChars [in] maximum number of characters for the returned string.
     * \return String describing the value with its prefix and unit.
     */
    std::string label(ER4CL_ARGIN unsigned int maxChars = 8) {
        return valueLabel(maxChars) + " " + unitPrefixes[prefix] + unit;
    }

    /*! \brief Returns the string describing the value with its prefix and unit in a nice fashion.
     *
     * \return String describing the value with its prefix and unit. The value and the prefix are converted so that the value is in the range [1.0, 1000.0[.
     */
    std::string niceLabel(ER4CL_ARGVOID) {
        Measurement temp;
        temp.value = value;
        temp.prefix = prefix;
        temp.unit = unit;
        temp.nice();

        return temp.label(5);
    }

    /*! \brief Converts #value given the input unit prefix.
     *
     * \param newPrefix [in] Desired unit prefix.
     */
    void convertValue(ER4CL_ARGIN UnitPfx_t newPrefix) {
        unsigned int delta;
        bool deltaPositive;

        if (prefix > newPrefix) {
            delta = prefix-newPrefix;
            deltaPositive = true;

        } else {
            delta = newPrefix-prefix;
            deltaPositive = false;
        }

        if (deltaPositive) {
            value *= powersOf1000[delta];

        } else {
            value /= powersOf1000[delta];
        }
        prefix = newPrefix;
    }

    /*! \brief Converts #value given the input unit multiplier.
     *
     * \param newMultiplier [in] Desired unit multiplier.
     */
    void convertValue(ER4CL_ARGIN double newMultiplier) {
        double multiplier = this->multiplier();
        double gain;
        bool gainPositive;

        if (multiplier > newMultiplier) {
            gain = multiplier/newMultiplier;
            gainPositive = true;

        } else {
            gain = newMultiplier/multiplier;
            gainPositive = false;
        }

        double diff;
        double minDiff = (std::numeric_limits <double>::max)();
        unsigned int minDelta = 0;
        for (unsigned int delta = 0; delta < UnitPfxNum; delta++) {
            diff = fabs(powersOf1000[delta]-gain);
            if (diff < minDiff) {
                minDiff = diff;
                minDelta = delta;
            }
        }

        if (gainPositive) {
            value *= powersOf1000[minDelta];
            decrementUnit(prefix, (int)minDelta);

        } else {
            value /= powersOf1000[minDelta];
            incrementUnit(prefix, (int)minDelta);
        }
    }

    /*! \brief Converts #value and #prefix in order to have a final #value in range [1.0, 1000.0[.
     */
    void nice(ER4CL_ARGVOID) {
        if ((value == 0.0) || isinf(value)) {
            prefix = UnitPfxNone;

        } else {
            while (fabs(value) >= 1000.0) {
                if (incrementUnit(prefix)) {
                    value /= 1000.0;

                } else {
                    break;
                }
            }

            while (fabs(value) < 1.0) {
                if (decrementUnit(prefix)) {
                    value *= 1000.0;

                } else {
                    break;
                }
            }
        }
    }
} Measurement_t;

/*! \brief Overloaded equality check for #Measurement_t. \note No conversion is performed, cause the multiplication can introduce rounding errors.
 *
 * \param a [in] First item of the comparison.
 * \param b [in] Second item of the comparison.
 * \return true if \p a and \p b have same value, prefix and unit.
*/
inline bool operator == (ER4CL_ARGIN const Measurement_t &a, ER4CL_ARGIN const Measurement_t &b) {
    return ((a.value == b.value) && (a.prefix == b.prefix) && (a.unit == b.unit));
}

/*! \brief Overloaded inequality check for #Measurement_t. \note No conversion is performed, cause the multiplication can introduce rounding errors.
 *
 * \param a [in] First item of the comparison.
 * \param b [in] Second item of the comparison.
 * \return true if \p a and \p b have different value, prefix or unit.
*/
inline bool operator != (ER4CL_ARGIN const Measurement_t &a, ER4CL_ARGIN const Measurement_t &b) {
    return !(a == b);
}

/*! \brief Overloaded inequality check for #Measurement_t.
 *
 * \param a [in] First item of the comparison.
 * \param b [in] Second item of the comparison.
 * \return true if \p a is smaller than \p b after they have been converted to have the same prefix. \note Returns false if \p a and \p b have different units.
*/
inline bool operator < (ER4CL_ARGIN const Measurement_t &a, ER4CL_ARGIN const Measurement_t &b) {
    if (a.unit != b.unit) {
        return false;

    } else {
        /*! Not using convertValue method to avoid changing the input structures */
        if (a.prefix < b.prefix) {
            return a.value < (b.value*powersOf1000[b.prefix-a.prefix]);

        } else {
            return (a.value*powersOf1000[a.prefix-b.prefix]) < b.value;
        }
    }
}

/*! \brief Overloaded inequality check for #Measurement_t.
 *
 * \param a [in] First item of the comparison.
 * \param b [in] Second item of the comparison.
 * \return true if \p a is smaller than or equal to \p b after they have been converted to have the same prefix. \note Returns false if \p a and \p b have different units.
*/
inline bool operator <= (ER4CL_ARGIN const Measurement_t &a, ER4CL_ARGIN const Measurement_t &b) {
    if (a.unit != b.unit) {
        return false;

    } else {
        /*! Not using convertValue method to avoid changing the input structures */
        if (a.prefix < b.prefix) {
            return a.value <= (b.value*powersOf1000[b.prefix-a.prefix]);

        } else {
            return (a.value*powersOf1000[a.prefix-b.prefix]) <= b.value;
        }
    }
}

/*! \brief Overloaded inequality check for #Measurement_t.
 *
 * \param a [in] First item of the comparison.
 * \param b [in] Second item of the comparison.
 * \return true if \p a is greater than \p b after they have been converted to have the same prefix. \note Returns false if \p a and \p b have different units.
*/
inline bool operator > (ER4CL_ARGIN const Measurement_t &a, ER4CL_ARGIN const Measurement_t &b) {
    if (a.unit != b.unit) {
        return false;

    } else {
        /*! Not using convertValue method to avoid changing the input structures */
        if (a.prefix < b.prefix) {
            return a.value > (b.value*powersOf1000[b.prefix-a.prefix]);

        } else {
            return (a.value*powersOf1000[a.prefix-b.prefix]) > b.value;
        }
    }
}

/*! \brief Overloaded inequality check for #Measurement_t.
 *
 * \param a [in] First item of the comparison.
 * \param b [in] Second item of the comparison.
 * \return true if \p a is greater than or equal to \p b after they have been converted to have the same prefix. \note Returns false if \p a and \p b have different units.
*/
inline bool operator >= (ER4CL_ARGIN const Measurement_t &a, ER4CL_ARGIN const Measurement_t &b) {
    if (a.unit != b.unit) {
        return false;

    } else {
        /*! Not using convertValue method to avoid changing the input structures */
        if (a.prefix < b.prefix) {
            return a.value >= (b.value*powersOf1000[b.prefix-a.prefix]);

        } else {
            return (a.value*powersOf1000[a.prefix-b.prefix]) >= b.value;
        }
    }
}

/*! \brief Overloaded sum for #Measurement_t.
 *
 * \param a [in] First operand.
 * \param b [in] Second operand.
 * \return A #Measurement_t whose value is the sum of the values of the operands converted to the prefix of the first operand, and the unit equals the unit of the second operand.
 * \note This method assumes the units are compatible and won't check for the sake of speed.
*/
inline Measurement_t operator + (ER4CL_ARGIN const Measurement_t &a, ER4CL_ARGIN const Measurement_t &b) {
    Measurement_t c = b;
    c.convertValue(a.prefix);
    c.value += a.value;
    return c;
}

/*! \brief Overloaded subtraction for #Measurement_t.
 *
 * \param a [in] First operand.
 * \param b [in] Second operand.
 * \return A #Measurement_t whose value is the difference of the values of the operands converted to the prefix of the first operand, and the unit equals the unit of the second operand.
 * \note This method assumes the units are compatible and won't check for the sake of speed.
*/
inline Measurement_t operator - (ER4CL_ARGIN const Measurement_t &a, ER4CL_ARGIN const Measurement_t &b) {
    Measurement_t c = b;
    c.convertValue(a.prefix);
    c.value = a.value-c.value;
    return c;
}

/*! \brief Overloaded multiplication between #Measurement_t and a constant.
 *
 * \param a [in] First operand.
 * \param b [in] Second operand.
 * \return A #Measurement_t whose value is the product of the values and the unit equals the unit of the first operand.
*/
template <class T>
inline Measurement_t operator * (ER4CL_ARGIN const Measurement_t &a, ER4CL_ARGIN const T &b) {
    Measurement_t c = a;
    c.value *= (double)b;
    return c;
}

/*! \brief Overloaded multiplication between #Measurement_t and a constant.
 *
 * \param a [in] First operand.
 * \param b [in] Second operand.
 * \return A #Measurement_t whose value is the product of the values and the unit equals the unit of the second operand.
*/
template <class T>
inline Measurement_t operator * (ER4CL_ARGIN const T &a, ER4CL_ARGIN const Measurement_t &b) {
    return b*a;
}

/*! \brief Overloaded division between #Measurement_t and a constant.
 *
 * \param a [in] First operand.
 * \param b [in] Second operand.
 * \return A #Measurement_t whose value is the ratio of the values and the unit equals the unit of the first operand.
*/
template <class T>
inline Measurement_t operator / (ER4CL_ARGIN const Measurement_t &a, ER4CL_ARGIN const T &b) {
    Measurement_t c = a;
    c.value /= (double)b;
    return c;
}

/*! \struct RangedMeasurement_t
 * \brief Structure used manage physical ranges that define a range with its unit and unit prefix.
 */
typedef struct RangedMeasurement {
    double min; /*!< Minimum value. */
    double max; /*!< Maximum value. */
    double step; /*!< Resolution. */
    UnitPfx_t prefix = UnitPfxNone; /*!< Unit prefix in the range [femto, Peta]. */
    std::string unit = ""; /*!< Unit. \note Can be any string, the library is not aware of real units meaning. */

    /*! \brief Returns the number of steps in the range.
     *
     * \return Number of steps in the range.
     */
    uint32_t steps(ER4CL_ARGVOID) {
        return static_cast <uint32_t> (round(1.0+(max-min)/step));
    }

    /*! \brief Returns the string corresponding to the prefix.
     *
     * \return String corresponding to the prefix.
     */
    std::string getPrefix(ER4CL_ARGVOID) {
        return unitPrefixes[prefix];
    }

    /*! \brief Returns the string corresponding to the unit with the prefix.
     *
     * \return String corresponding to the unit with the prefix.
     */
    std::string getFullUnit(ER4CL_ARGVOID) {
        return unitPrefixes[prefix] + unit;
    }

    /*! \brief Returns the prefix multiplier.
     *
     * \return Prefix multiplier, e.g. 10^-6 for micro.
     */
    double multiplier(ER4CL_ARGVOID) {
        unsigned int delta;
        bool deltaPositive;

        if (prefix > UnitPfxNone) {
            delta = prefix-UnitPfxNone;
            deltaPositive = true;

        } else {
            delta = UnitPfxNone-prefix;
            deltaPositive = false;
        }

        if (deltaPositive) {
            return powersOf1000[delta];

        } else {
            return 1.0/powersOf1000[delta];
        }
    }

    /*! \brief Returns a string describing the max value.
     *
     * \param maxChars [in] maximum number of characters for the returned string.
     * \return String describing the max value.
     */
    std::string valueLabel(ER4CL_ARGIN unsigned int maxChars = 8) {
        std::string valueSt = std::to_string(max);
        if (valueSt.length() >= maxChars) {
            valueSt.erase(maxChars);
        }
        size_t dot = valueSt.find_last_of(".");
        size_t notZero = valueSt.find_last_not_of("0");

        if (notZero != std::string::npos) {
            if (dot != std::string::npos) {
                return valueSt.erase(dot < notZero ? notZero+1 : dot);

            } else {
                return valueSt;
            }

        } else {
            return "0";
        }
    }

    /*! \brief Returns a string describing the max value with its prefix and unit.
     *
     * \param maxChars [in] maximum number of characters for the returned string.
     * \return String describing the max value with its prefix and unit.
     */
    std::string label(ER4CL_ARGIN unsigned int maxChars = 8) {
        return valueLabel(maxChars) + " " + unitPrefixes[prefix] + unit;
    }

    /*! \brief Returns the string describing the max value with its prefix and unit in a nice fashion.
     *
     * \return String describing the max value with its prefix and unit. The max value and the prefix are converted so that the value is in the range [1.0, 1000.0[.
     */
    std::string niceLabel(ER4CL_ARGVOID) {
        Measurement temp;
        temp.value = max;
        temp.prefix = prefix;
        temp.unit = unit;
        temp.nice();

        return temp.label(5);
    }

    /*! \brief Converts #min, #max and #step given the input unit prefix.
     *
     * \param newPrefix [in] Desired unit prefix.
     */
    void convertValues(ER4CL_ARGIN UnitPfx_t newPrefix) {
        unsigned int delta;
        bool deltaPositive;

        if (prefix > newPrefix) {
            delta = prefix-newPrefix;
            deltaPositive = true;

        } else {
            delta = newPrefix-prefix;
            deltaPositive = false;
        }

        if (deltaPositive) {
            min *= powersOf1000[delta];
            max *= powersOf1000[delta];
            step *= powersOf1000[delta];

        } else {
            min /= powersOf1000[delta];
            max /= powersOf1000[delta];
            step /= powersOf1000[delta];
        }
        prefix = newPrefix;
    }

    /*! \brief Converts #min, #max and #step given the input unit multiplier.
     *
     * \param newMultiplier [in] Desired unit multiplier.
     */
    void convertValues(ER4CL_ARGIN double newMultiplier) {
        double multiplier = this->multiplier();
        double gain;
        bool gainPositive;

        if (multiplier > newMultiplier) {
            gain = multiplier/newMultiplier;
            gainPositive = true;

        } else {
            gain = newMultiplier/multiplier;
            gainPositive = false;
        }

        double diff;
        double minDiff = (std::numeric_limits <double>::max)();
        unsigned int minDelta = 0;
        for (unsigned int delta = 0; delta < UnitPfxNum; delta++) {
            diff = fabs(powersOf1000[delta]-gain);
            if (diff < minDiff) {
                minDiff = diff;
                minDelta = delta;
            }
        }

        if (gainPositive) {
            min *= powersOf1000[minDelta];
            max *= powersOf1000[minDelta];
            step *= powersOf1000[minDelta];
            decrementUnit(prefix, (int)minDelta);

        } else {
            min /= powersOf1000[minDelta];
            max /= powersOf1000[minDelta];
            step /= powersOf1000[minDelta];
            incrementUnit(prefix, (int)minDelta);
        }
    }

    /*! \brief Returns the range width.
     *
     * \return Difference between max and min.
     */
    double delta(ER4CL_ARGVOID) {
        return max-min;
    }

    /*! \brief Returns a reasonable amount of decimals to represent the values in the range.
     *
     * \return A reasonable amount of decimals to represent the values in the range.
     */
    int decimals(ER4CL_ARGVOID) {
        int decimals = 0;
        double temp = step;
        while ((fabs(temp-round(temp)) > 0.05 || temp < 1.0) &&
               decimals < 3) { /*!< \todo Rather than asking 3 decimals better asking for 3 digits */
            decimals++;
            temp *= 10.0;
        }
        return decimals;
    }

    /*! \brief Returns a Measurement_t equivalent to the max value of the range.
     *
     * \return Measurement_t equivalent to the max value of the range.
     */
    Measurement_t getMax(ER4CL_ARGVOID) {
        Measurement_t extreme;
        extreme.value = max;
        extreme.prefix = prefix;
        extreme.unit = unit;
        return extreme;
    }

    /*! \brief Returns a Measurement_t equivalent to the min value of the range.
     *
     * \return Measurement_t equivalent to the min value of the range.
     */
    Measurement_t getMin(ER4CL_ARGVOID) {
        Measurement_t extreme;
        extreme.value = min;
        extreme.prefix = prefix;
        extreme.unit = unit;
        return extreme;
    }

    /*! \brief Inverse function of getXth.
     *
     * \return Index x such that the x-th step of the range best matches the input Measurement_t.
     */
    uint32_t getClosestIndex(ER4CL_ARGIN Measurement_t m) {
        m.convertValue(prefix);
        return static_cast <uint32_t> (round((m.value-getMin().value)/step));
    }

    /*! \brief Checks if a Measurement_t is within the range.
     *
     * \return true if the Measurement_t is within the range, false otherwise.
     */
    bool includes(ER4CL_ARGIN const Measurement_t value) {
        if (value >= getMin() && value <= getMax()) {
            return true;

        } else {
            return false;
        }
    }
} RangedMeasurement_t;

/*! \brief Overloaded equality check for #RangedMeasurement_t. \note No conversion is performed, cause the multiplication can introduce rounding errors.
 *
 * \param a [in] First item of the comparison.
 * \param b [in] Second item of the comparison.
 * \return true if \p a and \p b have same value, prefix and unit.
*/
inline bool operator == (const RangedMeasurement_t &a, const RangedMeasurement_t &b) {
    return ((a.min == b.min) && (a.max == b.max) && (a.step == b.step) && (a.prefix == b.prefix) && (a.unit == b.unit));
}

/*! \brief Overloaded inequality check for #RangedMeasurement_t. \note No conversion is performed, cause the multiplication can introduce rounding errors.
 *
 * \param a [in] First item of the comparison.
 * \param b [in] Second item of the comparison.
 * \return true if \p a and \p b have different value, prefix or unit.
*/
inline bool operator != (const RangedMeasurement_t &a, const RangedMeasurement_t &b) {
    return !(a == b);
}

/*! \struct CompensationControl_t
 * \brief Structure used to return detailed information on a specific compensation implemented by the HW.
 */
typedef struct CompensationControl {
    bool implemented = false; /*!< True if the corresponding compensation is implemented by the device. */
    double min = 0.0; /*!< Minimum compensable value. */
    double max = 1.0; /*!< Maximum compensable value globally. */
    double compensable = 1.0; /*!< Maximum compensable value given also the value of the other compensations. */
    double steps = 2; /*!< Number of steps between #min and #max. */
    double step = 1.0; /*!< Resolution. */
    int decimals = 0; /*!< Decimals to represent the compensated value. */
    double value = 0.0; /*!< Compensated value. */
    UnitPfx_t prefix = UnitPfxNone; /*!< Unit prefix in the range [femto, Peta]. */
    std::string unit = ""; /*!< Unit. \note Can be any string, the library is not aware of real units meaning. */
    std::string name = ""; /*!< Name of the compensation. */

    /*! \brief Returns the string corresponding to the prefix.
     *
     * \return String corresponding to the prefix.
     */
    std::string getPrefix(ER4CL_ARGVOID) {
        return unitPrefixes[prefix];
    }

    /*! \brief Returns the string corresponding to the unit with the prefix.
     *
     * \return String corresponding to the unit with the prefix.
     */
    std::string getFullUnit(ER4CL_ARGVOID) {
        return unitPrefixes[prefix] + unit;
    }

    /*! \brief Returns the string describing the compensation with its prefix and unit.
     *
     * \return String describing the compensation with its prefix and unit.
     */
    std::string title(ER4CL_ARGVOID) {
        if (unit != "") {
            return name + " [" + unitPrefixes[prefix] + unit + "]";

        } else {
            return name;
        }
    }
} CompensationControl_t;

#ifndef ER4COMMLIB_LABVIEW_WRAPPER
} // namespace e384CommLib
#endif

#endif // ER4COMMLIB_GLOBAL_ADDENDUM_H
