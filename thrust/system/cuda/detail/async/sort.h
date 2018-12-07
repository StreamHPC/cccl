/******************************************************************************
 * Copyright (c) 2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the NVIDIA CORPORATION nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL NVIDIA CORPORATION BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************/

// TODO: Move into system::cuda

#pragma once

#include <thrust/detail/config.h>
#include <thrust/detail/cpp11_required.h>

#if THRUST_CPP_DIALECT >= 2011

#if THRUST_DEVICE_COMPILER == THRUST_DEVICE_COMPILER_NVCC

#include <thrust/system/cuda/config.h>

#include <thrust/system/cuda/detail/async/customization.h>
#include <thrust/system/cuda/detail/async/copy.h>
#include <thrust/system/cuda/detail/sort.h>
#include <thrust/detail/alignment.h>
#include <thrust/system/cuda/future.h>
#include <thrust/type_traits/is_trivially_relocatable.h>
#include <thrust/type_traits/is_contiguous_iterator.h>
#include <thrust/type_traits/is_operator_less_or_greater_function_object.h>
#include <thrust/type_traits/logical_metafunctions.h>
#include <thrust/iterator/iterator_traits.h>
#include <thrust/detail/static_assert.h>
#include <thrust/distance.h>

#include <type_traits>

THRUST_BEGIN_NS

namespace system { namespace cuda { namespace detail
{

// Non-ContiguousIterator input and output iterators
template <
  typename DerivedPolicy
, typename ForwardIt, typename Size, typename StrictWeakOrdering
>
THRUST_RUNTIME_FUNCTION
auto async_stable_sort_n(
  execution_policy<DerivedPolicy>& policy,
  ForwardIt                        first,
  Size                             n,
  StrictWeakOrdering               comp
) ->
  typename std::enable_if<
    negation<is_contiguous_iterator<ForwardIt>>::value
  , unique_eager_future<
      void
    , typename thrust::detail::allocator_traits<
        decltype(get_async_device_allocator(policy))
      >::template rebind_traits<void>::pointer
    >
  >::type
{
  using T = typename iterator_traits<ForwardIt>::value_type;

  auto const device_alloc = get_async_device_allocator(policy);

  // Create device-side buffer.

  // FIXME: Combine this temporary allocation with the main one for CUB.
  auto device_buffer = uninitialized_allocate_unique_n<T>(device_alloc, n);

  auto const device_buffer_ptr = device_buffer.get();

  // Copy from the input into the buffer.

  auto new_policy0 = thrust::detail::derived_cast(policy).after(
    std::move(device_buffer)
  );

  auto f0 = async_copy_n(
    // TODO: We have to cast back to the right execution_policy class. Ideally,
    // we should be moving here.
    static_cast<thrust::cuda::execution_policy<decltype(new_policy0)>&>(
      new_policy0
    )
  , static_cast<thrust::cuda::execution_policy<decltype(new_policy0)>&>(
      new_policy0
    )
  , first
  , n
  , device_buffer_ptr
  );

  // Sort the buffer.

  auto new_policy1 = thrust::detail::derived_cast(policy).after(
    std::move(f0)
  );

  auto f1 = async_sort_n(
    // TODO: We have to cast back to the right execution_policy class. Ideally,
    // we should be moving here.
    static_cast<thrust::cuda::execution_policy<decltype(new_policy1)>&>(
      new_policy1
    )
  , device_buffer_ptr
  , n
  , comp
  );

  // Copy from the buffer into the input.
  // FIXME: Combine this with the potential memcpy at the end of the main sort
  // routine.

  auto new_policy2 = thrust::detail::derived_cast(policy).after(
    std::move(f1)
  );

  return async_copy_n(
    // TODO: We have to cast back to the right execution_policy class. Ideally,
    // we should be moving here.
    static_cast<thrust::cuda::execution_policy<decltype(new_policy2)>&>(
      new_policy2
    )
  , static_cast<thrust::cuda::execution_policy<decltype(new_policy2)>&>(
      new_policy2
    )
  , device_buffer_ptr
  , n
  , first
  );
}

// ContiguousIterator iterators
// Non-Scalar value type or user-defined StrictWeakOrdering
template <
  typename DerivedPolicy
, typename ForwardIt, typename Size, typename StrictWeakOrdering
>
THRUST_RUNTIME_FUNCTION
auto async_stable_sort_n(
  execution_policy<DerivedPolicy>& policy,
  ForwardIt                        first,
  Size                             n,
  StrictWeakOrdering               comp
) ->
  typename std::enable_if<
    conjunction<
      is_contiguous_iterator<ForwardIt>
    , disjunction<
        negation<
          std::is_scalar<
            typename iterator_traits<ForwardIt>::value_type
          >
        >
      , negation<
          is_operator_less_or_greater_function_object<StrictWeakOrdering>
        >
      >
    >::value
  , unique_eager_future<
      void
    , typename thrust::detail::allocator_traits<
        decltype(get_async_device_allocator(policy))
      >::template rebind_traits<void>::pointer
    >
  >::type
{
  using T = typename thrust::iterator_traits<ForwardIt>::value_type;

  auto const device_alloc = get_async_device_allocator(policy);

  using pointer
    = typename thrust::detail::allocator_traits<decltype(device_alloc)>::
      template rebind_traits<void>::pointer;

  unique_eager_future_promise_pair<void, pointer> fp;

  // Determine temporary device storage requirements.

  size_t tmp_size = 0;
  thrust::cuda_cub::throw_on_error(
    thrust::cuda_cub::__merge_sort::doit_step<
      /* Sort items? */ std::false_type, /* Stable? */ std::true_type
    >(
      nullptr
    , tmp_size
    , first
    , static_cast<thrust::detail::uint8_t*>(nullptr) // Items.
    , n
    , comp
    , nullptr // Null stream, just for sizing.
    , THRUST_DEBUG_SYNC_FLAG
    )
  , "after merge sort sizing"
  );

  // Allocate temporary storage.

  auto content = uninitialized_allocate_unique_n<thrust::detail::uint8_t>(
    device_alloc, tmp_size
  );

  // The array was dynamically allocated, so we assume that it's suitably
  // aligned for any type of data. `malloc`/`cudaMalloc`/`new`/`std::allocator`
  // make this guarantee.
  auto const content_ptr = content.get();

  void* const tmp_ptr = static_cast<void*>(
    thrust::raw_pointer_cast(content_ptr)
  );

  // Set up stream with dependencies.

  cudaStream_t const user_raw_stream = thrust::cuda_cub::stream(policy);

  if (thrust::cuda_cub::default_stream() != user_raw_stream)
  {
    fp = depend_on<void, pointer>(
      nullptr
    , std::tuple_cat(
        std::make_tuple(
          std::move(content)
        , unique_stream(nonowning, user_raw_stream)
        )
      , extract_dependencies(
          std::move(thrust::detail::derived_cast(policy))
        )
      )
    );
  }
  else
  {
    fp = depend_on<void, pointer>(
      nullptr
    , std::tuple_cat(
        std::make_tuple(
          std::move(content)
        )
      , extract_dependencies(
          std::move(thrust::detail::derived_cast(policy))
        )
      )
    );
  }

  // Run merge sort.

  thrust::cuda_cub::throw_on_error(
    thrust::cuda_cub::__merge_sort::doit_step<
      /* Sort items? */ std::false_type, /* Stable? */ std::true_type
    >(
      tmp_ptr
    , tmp_size
    , first
    , static_cast<thrust::detail::uint8_t*>(nullptr) // Items.
    , n
    , comp
    , fp.future.stream().native_handle()
    , THRUST_DEBUG_SYNC_FLAG
    )
  , "after merge sort sizing"
  );

  return std::move(fp.future);
}

template <typename T, typename Size, typename StrictWeakOrdering>
THRUST_RUNTIME_FUNCTION
typename std::enable_if<
  is_operator_less_function_object<StrictWeakOrdering>::value
, cudaError_t
>::type
invoke_radix_sort(
  cudaStream_t                           stream
, void*                                  tmp_ptr
, std::size_t                            tmp_size
, thrust::cuda_cub::cub::DoubleBuffer<T> keys
, Size                                   n
, StrictWeakOrdering
)
{
  return thrust::cuda_cub::cub::DeviceRadixSort::SortKeys(
    tmp_ptr
  , tmp_size
  , keys
  , n
  , 0
  , sizeof(T) * 8
  , stream
  , THRUST_DEBUG_SYNC_FLAG
  );
}

template <typename T, typename Size, typename StrictWeakOrdering>
THRUST_RUNTIME_FUNCTION
typename std::enable_if<
  is_operator_greater_function_object<StrictWeakOrdering>::value
, cudaError_t
>::type
invoke_radix_sort(
  cudaStream_t                           stream
, void*                                  tmp_ptr
, std::size_t                            tmp_size
, thrust::cuda_cub::cub::DoubleBuffer<T> keys
, Size                                   n
, StrictWeakOrdering
)
{
  return thrust::cuda_cub::cub::DeviceRadixSort::SortKeysDescending(
    tmp_ptr
  , tmp_size
  , keys
  , n
  , 0
  , sizeof(T) * 8
  , stream
  , THRUST_DEBUG_SYNC_FLAG
  );
}

// ContiguousIterator iterators
// Scalar value type
// operator< or operator>
template <
  typename DerivedPolicy
, typename ForwardIt, typename Size, typename StrictWeakOrdering
>
THRUST_RUNTIME_FUNCTION
auto async_stable_sort_n(
  execution_policy<DerivedPolicy>& policy
, ForwardIt                        first
, Size                             n
, StrictWeakOrdering               comp
) ->
  typename std::enable_if<
    conjunction<
      is_contiguous_iterator<ForwardIt>
    , std::is_scalar<
        typename iterator_traits<ForwardIt>::value_type
      >
    , is_operator_less_or_greater_function_object<StrictWeakOrdering>
    >::value
  , unique_eager_future<
      void
    , typename thrust::detail::allocator_traits<
        decltype(get_async_device_allocator(policy))
      >::template rebind_traits<void>::pointer
    >
  >::type
{
  using T = typename iterator_traits<ForwardIt>::value_type;

  auto const device_alloc = get_async_device_allocator(policy);

  using pointer
    = typename thrust::detail::allocator_traits<decltype(device_alloc)>::
      template rebind_traits<void>::pointer;

  unique_eager_future_promise_pair<void, pointer> fp;

  thrust::cuda_cub::cub::DoubleBuffer<T> keys(
    raw_pointer_cast(&*first), nullptr
  );

  // Determine temporary device storage requirements.

  size_t tmp_size = 0;
  thrust::cuda_cub::throw_on_error(
    invoke_radix_sort(
      nullptr // Null stream, just for sizing.
    , nullptr
    , tmp_size
    , keys
    , n
    , comp
    )
  , "after radix sort sizing"
  );

  // Allocate temporary storage.

  size_t keys_temp_storage = thrust::detail::aligned_storage_size(
    sizeof(T) * n, 128
  );

  auto content = uninitialized_allocate_unique_n<thrust::detail::uint8_t>(
    device_alloc, keys_temp_storage + tmp_size
  );

  // The array was dynamically allocated, so we assume that it's suitably
  // aligned for any type of data. `malloc`/`cudaMalloc`/`new`/`std::allocator`
  // make this guarantee.
  auto const content_ptr = content.get();

  keys.d_buffers[1] = thrust::detail::aligned_reinterpret_cast<T*>(
    thrust::raw_pointer_cast(content_ptr)
  );

  void* const tmp_ptr = static_cast<void*>(
    thrust::raw_pointer_cast(content_ptr + keys_temp_storage)
  );

  // Set up stream with dependencies.

  cudaStream_t const user_raw_stream = thrust::cuda_cub::stream(policy);

  if (thrust::cuda_cub::default_stream() != user_raw_stream)
  {
    fp = depend_on<void, pointer>(
      nullptr
    , std::tuple_cat(
        std::make_tuple(
          std::move(content)
        , unique_stream(nonowning, user_raw_stream)
        )
      , extract_dependencies(
          std::move(thrust::detail::derived_cast(policy))
        )
      )
    );
  }
  else
  {
    fp = depend_on<void, pointer>(
      nullptr
    , std::tuple_cat(
        std::make_tuple(
          std::move(content)
        )
      , extract_dependencies(
          std::move(thrust::detail::derived_cast(policy))
        )
      )
    );
  }

  // Run radix sort.

  thrust::cuda_cub::throw_on_error(
    invoke_radix_sort(
      fp.future.stream().native_handle()
    , tmp_ptr
    , tmp_size
    , keys
    , n
    , comp
    )
  , "after radix sort launch"
  );

  if (0 != keys.selector)
  {
    auto new_policy0 = thrust::detail::derived_cast(policy).after(
      std::move(fp.future)
    );

    using return_future = decltype(fp.future);
    return return_future(async_copy_n(
      // TODO: We have to cast back to the right execution_policy class.
      // Ideally, we should be moving here.
      static_cast<thrust::cuda::execution_policy<decltype(new_policy0)>&>(
        new_policy0
      )
    , static_cast<thrust::cuda::execution_policy<decltype(new_policy0)>&>(
        new_policy0
      )
    , keys.d_buffers[1]
    , n
    , keys.d_buffers[0]
    ));
  }
  else
    return std::move(fp.future);
}

}}} // namespace system::cuda::detail

namespace cuda_cub
{

// ADL entry point.
template <
  typename DerivedPolicy
, typename ForwardIt, typename Sentinel, typename StrictWeakOrdering
>
THRUST_RUNTIME_FUNCTION
auto async_stable_sort(
  execution_policy<DerivedPolicy>& policy,
  ForwardIt                        first,
  Sentinel                         last,
  StrictWeakOrdering               comp
)
THRUST_DECLTYPE_RETURNS(
  thrust::system::cuda::detail::async_stable_sort_n(
    policy, first, distance(first, last), comp
  )
)

} // cuda_cub

THRUST_END_NS

#endif // THRUST_DEVICE_COMPILER == THRUST_DEVICE_COMPILER_NVCC

#endif // THRUST_CPP_DIALECT >= 2011

