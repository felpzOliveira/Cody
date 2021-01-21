#define S(k) if (n >= (UINT64_C(1) << k)) { i += k; n >>= k; }
