// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

#ifndef O2_CONTAINER_TEMPLATE_HELPERS_
#define O2_CONTAINER_TEMPLATE_HELPERS_

#pragma once

#include <type_traits>

namespace detail {

////////////////////////////////////////////////////////////////////////////////////////////////////
// enable_if, disable_if
////////////////////////////////////////////////////////////////////////////////////////////////////
// enable_if
template <bool T, class B>
using enable_if = std::enable_if<T, B>;

template <bool T, class B>
using enable_if_t = std::enable_if_t<T, B>;

// disable if
template <bool T, class B>
using disable_if = std::enable_if<!T, B>;

template <bool T, class B>
using disable_if_t = std::enable_if_t<!T, B>;

////////////////////////////////////////////////////////////////////////////////////////////////////
// Destroy
////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Destruct n objects of T, allocated with Allocator, if T is trivially destructible
//
template <typename Allocator, typename InputIterator, typename size_type = typename Allocator::size_type,
          typename T = typename Allocator::value_type>
inline enable_if_t<std::is_trivially_destructible<T>::value, void> destroy_n(Allocator, InputIterator, size_type)
{
  // intentionally left blank
  // TODO: trash the memory in DEBUG mode
}

//
// Destruct n objects of T, allocated with Allocator, if T is not trivially destructible
//
template <typename Allocator, typename InputIterator, typename size_type = typename Allocator::size_type,
          typename T = typename Allocator::value_type>
inline disable_if_t<std::is_trivially_destructible<T>::value, void> destroy_n(Allocator& alloc, InputIterator iter,
                                                                              size_type n)
{
  for (; n > 0; n--, iter++) {
    std::allocator_traits<Allocator>::destroy(alloc, &*iter);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Default initialize
////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Construct in place default object value
//
template <typename Allocator, typename ForwardIterator, typename size_type = typename Allocator::size_type,
          typename T = typename Allocator::value_type>
inline ForwardIterator init_default_n(Allocator& alloc, size_type n, ForwardIterator dst)
{
  auto start = dst;
  try
  {
    while (n--)
      std::allocator_traits<Allocator>::construct(alloc, &*(dst++), T{});
  }
  catch (...)
  {
    while (start != dst)
      std::allocator_traits<Allocator>::destroy(alloc, &*(start++));
    // rethrow
    throw;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Move
////////////////////////////////////////////////////////////////////////////////////////////////////
//
// move n objects of T, if T is not copy movable
//
template <typename Allocator, typename InputIterator, typename ForwardIterator,
          typename T = typename Allocator::value_type>
inline disable_if_t<std::is_trivially_copyable<T>::value, void> move_n(Allocator& alloc, InputIterator first,
                                                                       InputIterator last, ForwardIterator dst)
{
  for (; first != last; first++, dst++) {
    std::allocator_traits<Allocator>::construct(alloc, &*dst, std::move(*first));
  }
}
//
// Copy n objects of T, if T is copy movable (POD)
//
template <typename Allocator, typename InputIterator, typename ForwardIterator,
          typename T = typename Allocator::value_type>
inline enable_if_t<std::is_trivially_copyable<T>::value, void> move_n(Allocator&, InputIterator first,
                                                                      InputIterator last, ForwardIterator dst)
{
  typename Allocator::difference_type diff = last - first;
  if (diff > 0) {
    std::memmove(static_cast<void*>(&*dst), static_cast<const void*>(&*first), std::size_t(diff) * sizeof(T));
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Copy range
////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Copy n objects of T, if T is not copy movable
//
template <typename Allocator, typename InputIterator, typename ForwardIterator,
          typename T = typename Allocator::value_type>
inline disable_if_t<std::is_trivially_copyable<T>::value, void> copy_n(InputIterator first, InputIterator last,
                                                                       ForwardIterator dst)
{
  for (; first != last; first++, dst++) {
    *dst = *first;
  }
}
//
// Copy n objects of T, if T is copy movable (POD)
//
template <typename Allocator, typename InputIterator, typename ForwardIterator,
          typename T = typename Allocator::value_type>
inline enable_if_t<std::is_trivially_copyable<T>::value, void> copy_n(InputIterator first, InputIterator last,
                                                                      ForwardIterator dst)
{
  typename Allocator::difference_type diff = last - first;
  if (diff > 0) {
    std::memmove(static_cast<void*>(&*dst), static_cast<const void*>(&*first), std::size_t(diff) * sizeof(T));
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Copy 1 n times
////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Copy 1 objects of T n times, if T is not copy movable
//
template <typename T, typename ForwardIterator, typename size_type>
inline disable_if_t<std::is_trivially_copyable<T>::value, void> fill_n(T const& a, ForwardIterator dst, size_type n)
{
  for (size_type i = 0; i < n; i++, dst++) {
    // *dst = a;
    new (&dst) T(a);
  }
}

//
// Copy 1 objects of T n times, if T is copy movable (POD)
//
template <typename T, typename ForwardIterator, typename size_type>
inline enable_if_t<std::is_trivially_copyable<T>::value, void> fill_n(T const& a, ForwardIterator dst, size_type n)
{
  for (size_type i = 0; i < n; i++, dst++) {
    std::memcpy(static_cast<void*>(&*dst), static_cast<const void*>(&a), sizeof(T));
  }
}

} // namespace detail

#endif /* end of include guard: O2_CONTAINER_TEMPLATE_HELPERS_ */
