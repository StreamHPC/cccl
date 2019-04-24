//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// UNSUPPORTED: libcpp-has-no-threads, pre-sm-70

// <cuda/std/atomic>

// template <>
// struct atomic<integral>
// {
//     bool is_lock_free() const volatile;
//     bool is_lock_free() const;
//     void store(integral desr, memory_order m = memory_order_seq_cst) volatile;
//     void store(integral desr, memory_order m = memory_order_seq_cst);
//     integral load(memory_order m = memory_order_seq_cst) const volatile;
//     integral load(memory_order m = memory_order_seq_cst) const;
//     operator integral() const volatile;
//     operator integral() const;
//     integral exchange(integral desr,
//                       memory_order m = memory_order_seq_cst) volatile;
//     integral exchange(integral desr, memory_order m = memory_order_seq_cst);
//     bool compare_exchange_weak(integral& expc, integral desr,
//                                memory_order s, memory_order f) volatile;
//     bool compare_exchange_weak(integral& expc, integral desr,
//                                memory_order s, memory_order f);
//     bool compare_exchange_strong(integral& expc, integral desr,
//                                  memory_order s, memory_order f) volatile;
//     bool compare_exchange_strong(integral& expc, integral desr,
//                                  memory_order s, memory_order f);
//     bool compare_exchange_weak(integral& expc, integral desr,
//                                memory_order m = memory_order_seq_cst) volatile;
//     bool compare_exchange_weak(integral& expc, integral desr,
//                                memory_order m = memory_order_seq_cst);
//     bool compare_exchange_strong(integral& expc, integral desr,
//                                 memory_order m = memory_order_seq_cst) volatile;
//     bool compare_exchange_strong(integral& expc, integral desr,
//                                  memory_order m = memory_order_seq_cst);
//
//     integral
//         fetch_add(integral op, memory_order m = memory_order_seq_cst) volatile;
//     integral fetch_add(integral op, memory_order m = memory_order_seq_cst);
//     integral
//         fetch_sub(integral op, memory_order m = memory_order_seq_cst) volatile;
//     integral fetch_sub(integral op, memory_order m = memory_order_seq_cst);
//     integral
//         fetch_and(integral op, memory_order m = memory_order_seq_cst) volatile;
//     integral fetch_and(integral op, memory_order m = memory_order_seq_cst);
//     integral
//         fetch_or(integral op, memory_order m = memory_order_seq_cst) volatile;
//     integral fetch_or(integral op, memory_order m = memory_order_seq_cst);
//     integral
//         fetch_xor(integral op, memory_order m = memory_order_seq_cst) volatile;
//     integral fetch_xor(integral op, memory_order m = memory_order_seq_cst);
//
//     atomic() = default;
//     constexpr atomic(integral desr);
//     atomic(const atomic&) = delete;
//     atomic& operator=(const atomic&) = delete;
//     atomic& operator=(const atomic&) volatile = delete;
//     integral operator=(integral desr) volatile;
//     integral operator=(integral desr);
//
//     integral operator++(int) volatile;
//     integral operator++(int);
//     integral operator--(int) volatile;
//     integral operator--(int);
//     integral operator++() volatile;
//     integral operator++();
//     integral operator--() volatile;
//     integral operator--();
//     integral operator+=(integral op) volatile;
//     integral operator+=(integral op);
//     integral operator-=(integral op) volatile;
//     integral operator-=(integral op);
//     integral operator&=(integral op) volatile;
//     integral operator&=(integral op);
//     integral operator|=(integral op) volatile;
//     integral operator|=(integral op);
//     integral operator^=(integral op) volatile;
//     integral operator^=(integral op);
// };

#include <cuda/std/atomic>
#include <cuda/std/new>
#include <cuda/std/cassert>

#include <cmpxchg_loop.h>

#include "test_macros.h"

template <class A, class T>
__host__ __device__
void
do_test()
{
    A obj(T(0));
    assert(obj == T(0));
    cuda::std::atomic_init(&obj, T(1));
    assert(obj == T(1));
    cuda::std::atomic_init(&obj, T(2));
    assert(obj == T(2));
    bool b0 = obj.is_lock_free();
    ((void)b0); // mark as unused
    obj.store(T(0));
    assert(obj == T(0));
    obj.store(T(1), cuda::std::memory_order_release);
    assert(obj == T(1));
    assert(obj.load() == T(1));
    assert(obj.load(cuda::std::memory_order_acquire) == T(1));
    assert(obj.exchange(T(2)) == T(1));
    assert(obj == T(2));
    assert(obj.exchange(T(3), cuda::std::memory_order_relaxed) == T(2));
    assert(obj == T(3));
    T x = obj;
    assert(cmpxchg_weak_loop(obj, x, T(2)) == true);
    assert(obj == T(2));
    assert(x == T(3));
    assert(obj.compare_exchange_weak(x, T(1)) == false);
    assert(obj == T(2));
    assert(x == T(2));
    x = T(2);
    assert(obj.compare_exchange_strong(x, T(1)) == true);
    assert(obj == T(1));
    assert(x == T(2));
    assert(obj.compare_exchange_strong(x, T(0)) == false);
    assert(obj == T(1));
    assert(x == T(1));
    assert((obj = T(0)) == T(0));
    assert(obj == T(0));
    assert(obj++ == T(0));
    assert(obj == T(1));
    assert(++obj == T(2));
    assert(obj == T(2));
    assert(--obj == T(1));
    assert(obj == T(1));
    assert(obj-- == T(1));
    assert(obj == T(0));
    obj = T(2);
    assert((obj += T(3)) == T(5));
    assert(obj == T(5));
    assert((obj -= T(3)) == T(2));
    assert(obj == T(2));
    assert((obj |= T(5)) == T(7));
    assert(obj == T(7));
    assert((obj &= T(0xF)) == T(7));
    assert(obj == T(7));
    assert((obj ^= T(0xF)) == T(8));
    assert(obj == T(8));

    {
        TEST_ALIGNAS_TYPE(A) char storage[sizeof(A)] = {23};
        A& zero = *new (storage) A();
        assert(zero == 0);
        zero.~A();
    }
}

template <class A, class T>
__host__ __device__
void test()
{
    do_test<A, T>();
    do_test<volatile A, T>();
}


int main(int, char**)
{
    test<cuda::std::atomic_char, char>();
    test<cuda::std::atomic_schar, signed char>();
    test<cuda::std::atomic_uchar, unsigned char>();
    test<cuda::std::atomic_short, short>();
    test<cuda::std::atomic_ushort, unsigned short>();
    test<cuda::std::atomic_int, int>();
    test<cuda::std::atomic_uint, unsigned int>();
    test<cuda::std::atomic_long, long>();
    test<cuda::std::atomic_ulong, unsigned long>();
    test<cuda::std::atomic_llong, long long>();
    test<cuda::std::atomic_ullong, unsigned long long>();
#ifndef _LIBCPP_HAS_NO_UNICODE_CHARS
    test<cuda::std::atomic_char16_t, char16_t>();
    test<cuda::std::atomic_char32_t, char32_t>();
#endif  // _LIBCPP_HAS_NO_UNICODE_CHARS
    test<cuda::std::atomic_wchar_t, wchar_t>();

    test<cuda::std::atomic_int8_t,    int8_t>();
    test<cuda::std::atomic_uint8_t,  uint8_t>();
    test<cuda::std::atomic_int16_t,   int16_t>();
    test<cuda::std::atomic_uint16_t, uint16_t>();
    test<cuda::std::atomic_int32_t,   int32_t>();
    test<cuda::std::atomic_uint32_t, uint32_t>();
    test<cuda::std::atomic_int64_t,   int64_t>();
    test<cuda::std::atomic_uint64_t, uint64_t>();

    test<volatile cuda::std::atomic_char, char>();
    test<volatile cuda::std::atomic_schar, signed char>();
    test<volatile cuda::std::atomic_uchar, unsigned char>();
    test<volatile cuda::std::atomic_short, short>();
    test<volatile cuda::std::atomic_ushort, unsigned short>();
    test<volatile cuda::std::atomic_int, int>();
    test<volatile cuda::std::atomic_uint, unsigned int>();
    test<volatile cuda::std::atomic_long, long>();
    test<volatile cuda::std::atomic_ulong, unsigned long>();
    test<volatile cuda::std::atomic_llong, long long>();
    test<volatile cuda::std::atomic_ullong, unsigned long long>();
#ifndef _LIBCPP_HAS_NO_UNICODE_CHARS
    test<volatile cuda::std::atomic_char16_t, char16_t>();
    test<volatile cuda::std::atomic_char32_t, char32_t>();
#endif  // _LIBCPP_HAS_NO_UNICODE_CHARS
    test<volatile cuda::std::atomic_wchar_t, wchar_t>();

    test<volatile cuda::std::atomic_int8_t,    int8_t>();
    test<volatile cuda::std::atomic_uint8_t,  uint8_t>();
    test<volatile cuda::std::atomic_int16_t,   int16_t>();
    test<volatile cuda::std::atomic_uint16_t, uint16_t>();
    test<volatile cuda::std::atomic_int32_t,   int32_t>();
    test<volatile cuda::std::atomic_uint32_t, uint32_t>();
    test<volatile cuda::std::atomic_int64_t,   int64_t>();
    test<volatile cuda::std::atomic_uint64_t, uint64_t>();

  return 0;
}
