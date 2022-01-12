//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// <cuda/std/iterator>

// insert_iterator

// insert_iterator<Cont>& operator*();

#include <cuda/std/iterator>
#if defined(_LIBCUDACXX_HAS_VECTOR)
#include <cuda/std/vector>
#include <cuda/std/cassert>
#include "nasty_containers.h"

#include "test_macros.h"

template <class C>
void
test(C c)
{
    cuda::std::insert_iterator<C> i(c, c.end());
    cuda::std::insert_iterator<C>& r = *i;
    assert(&r == &i);
}

int main(int, char**)
{
    test(cuda::std::vector<int>());
    test(nasty_vector<int>());

  return 0;
}
#else
int main(int, char**)
{
  return 0;
}
#endif