#include <string>

inline bool stoiValid(std::string str) {
    if (str.empty()) {
        return false;
    }
    for (char c : str) {
        if (c < '0' || c > '9') {
            return false;
        }
    }
    return true;
}

inline bool stofValid(std::string str) {
    if (str.empty()) {
        return false;
    }
    bool decimalPointSeen = false;
    for (char c : str) {
        if (c == '.') {
            if (decimalPointSeen) {
                return false; // Multiple decimal points
            }
            decimalPointSeen = true;
        } else if (c < '0' || c > '9') {
            return false; // Non-numeric character
        }
    }
    return true;
}