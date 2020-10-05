/* Header file to support fixed-point arithmetic operations.
   In this file, we'll use 17.14 fixed-point, which means that lowest 14 bits will be used as fractional bits.
   Functions are implemented based on the Pintos official document. */

#include <stdint.h>

#define p 17
#define q 14
#define f (1 << q)

int convert_int_to_fp(int n) {
    return n * f; 
}

int convert_fp_to_int_zero(int x) {
    return x / f;
}

int convert_fp_to_int_nearest(int x) {
    if(x >= 0) {
        return (x + (f / 2)) / f;
    }
    else {
        return (x - (f / 2)) / f;
    } 
}

int fp_add_fp(int x, int y) {
    return x + y;
}

int fp_sub_fp(int x, int y) {
    return x - y;
}

int fp_add_int(int x, int n) {
    return x + (n * f);
}

int fp_sub_int(int x, int n) {
    return x - (n * f);
}

int fp_mul_fp(int x, int y) {
    return ((int64_t) x) * y / f;
}

int fp_mul_int(int x, int n) {
    return x * n;
}

int fp_div_fp(int x, int y) {
    return ((int64_t) x) * f / y;
}

int fp_div_int(int x, int n) {
    return x / n;
}