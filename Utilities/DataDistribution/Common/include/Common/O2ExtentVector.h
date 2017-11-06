// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

#ifndef O2_CONTAINER_O2EXTENTVECTOR_H_
#define O2_CONTAINER_O2EXTENTVECTOR_H_

#include <cassert>

#include "template_helpers.h"
// #include <allocator_helpers.h>

#include <vector>
#include <algorithm>
#include <utility>
#include <cstddef>

#include <iostream>

#pragma once

namespace detail {

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ExtentPosition
//
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class Allocator>
class Extent {
public:
  typedef std::allocator_traits<Allocator> alloc_traits;
  typedef typename Allocator::value_type value_type;
  typedef typename Allocator::value_type& reference;
  typedef typename Allocator::const_reference const_reference;
  typedef typename Allocator::pointer pointer;
  typedef typename alloc_traits::size_type size_type;
  typedef typename alloc_traits::difference_type difference_type;

private:
  Allocator& mAlloc;                        // TODO: remove if not allowing extents with different allocators
  pointer mFancyPointer = pointer(nullptr); // TODO: unique or shared?

  value_type* mStart = nullptr; // simple pointer
  size_type mSize = size_type(0);
  size_type mCapacity = size_type(0);

public:
  Extent() = delete;
  Extent(Extent const&) = delete; // TODO: support copying, and different allocators
  Extent operator=(Extent const&) = delete;

  Extent(Extent&& e) noexcept : mAlloc(e.mAlloc)
  {
    mFancyPointer = std::exchange(e.mFancyPointer, pointer(nullptr));
    mStart = std::exchange(e.mStart, nullptr);
    mSize = std::exchange(e.mSize, size_type(0));
    mCapacity = std::exchange(e.mCapacity, size_type(0));

    assert(mCapacity > 0);
    assert(mSize <= mCapacity);
  }

  Extent(size_type n, Allocator& alloc) : mAlloc(alloc)
  {
    mFancyPointer = alloc_traits::allocate(mAlloc, n); // throws std::bad_alloc
    mStart = static_cast<value_type*>(mFancyPointer);
    mSize = 0;
    mCapacity = n;

    assert(mCapacity > 0);
  }

  ~Extent()
  {
    if (mFancyPointer) {
      detail::destroy_n(mAlloc, mStart, mSize);
      alloc_traits::deallocate(mAlloc, mFancyPointer, mCapacity);
    }
  }

  inline size_type unused() const
  {
    return mCapacity - mSize;
  }
  inline size_type capacity() const
  {
    return mCapacity;
  }
  inline size_type size() const
  {
    return mSize;
  }

  inline const value_type& operator[](size_type off) const
  {
    assert(off >= 0);
    assert(off < mSize);

    return mStart[off];
  }

  inline value_type& operator[](size_type off)
  {
    assert(off >= 0);
    assert(off < mSize);

    return mStart[off];
  }

  template <class... Args>
  inline value_type* emplace_back(Args&&... args)
  {
    assert(unused() > 0);

    value_type* pos = mStart + mSize;
    alloc_traits::construct(mAlloc, pos, std::forward<Args>(args)...);
    mSize++;
    return pos;
  }

  template <class... Args>
  inline value_type* emplace_replace(size_type pos, Args&&... args)
  {
    assert(pos < size());

    auto ins = mStart + pos;
    alloc_traits::destroy(mAlloc, ins);
    alloc_traits::construct(mAlloc, ins, std::forward<Args>(args)...);
    return ins;
  }

  inline void clear() noexcept
  {
    assert(capacity() > 0);
    assert(size() <= capacity());

    detail::destroy_n(mAlloc, mStart, mSize);
    mSize = 0;
  }

  inline void pop_back()
  {
    assert(capacity() > 0);
    assert(size() > 0);
    assert(size() <= capacity());

    detail::destroy_n(mAlloc, mStart + mSize - 1, 1);
    mSize--;
  }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ExtentPosition
//
////////////////////////////////////////////////////////////////////////////////////////////////////

template <class Allocator>
using ExtentVector = typename std::vector<Extent<Allocator>>;
template <class Allocator>
using ExtentIterator = typename std::vector<Extent<Allocator>>::iterator;

template <class T, class Allocator>
class ExtentVectorBase;

template <bool Const, class Allocator>
class ExtentPositionBase {
public:
  typedef std::allocator_traits<Allocator> allocator_traits;
  typedef std::pointer_traits<typename allocator_traits::pointer> pointer_traits;

  typedef typename pointer_traits::element_type value_type;
  typedef typename pointer_traits::difference_type difference_type;

  typedef std::conditional_t<Const, typename pointer_traits::template rebind<const value_type>,
                             typename pointer_traits::pointer> pointer;
  typedef typename std::pointer_traits<pointer>::element_type& reference;
};

// TODO: simple pointer for single-extent stable vector

template <bool Const, class Allocator>
class ExtentPosition : public ExtentPositionBase<Const, Allocator> {

  using pointer_traits = typename ExtentPositionBase<Const, Allocator>::pointer_traits;
  using allocator_traits = typename ExtentPositionBase<Const, Allocator>::allocator_traits;

public:
  typedef std::random_access_iterator_tag iterator_category;

  using value_type = typename ExtentPositionBase<Const, Allocator>::value_type;
  using difference_type = typename ExtentPositionBase<Const, Allocator>::difference_type;
  using pointer = typename ExtentPositionBase<Const, Allocator>::pointer;
  using reference = typename ExtentPositionBase<Const, Allocator>::reference;

  typedef std::conditional_t<
    Const,
    typename pointer_traits::template rebind<const ExtentVectorBase<typename allocator_traits::value_type, Allocator>>,
    typename pointer_traits::template rebind<ExtentVectorBase<typename allocator_traits::value_type, Allocator>>>
    extent_vector_base_ptr;

  typedef typename Extent<Allocator>::size_type extent_size_type;
  typedef std::size_t size_type;

  extent_vector_base_ptr mExtentVectorBase;
  extent_size_type mExtentIndex;
  difference_type mElementIndex;
  difference_type mVectorIndex;

  ExtentPosition() noexcept : mExtentVectorBase(nullptr)
  {
    // null (singular) iterator
  }

  inline ExtentPosition(ExtentPosition<false, Allocator> const& it) noexcept
  {
    mExtentVectorBase = it.mExtentVectorBase;
    mExtentIndex = it.mExtentIndex;
    mElementIndex = it.mElementIndex;
    mVectorIndex = it.mVectorIndex;
  }

  // TODO: this shoud be hidden
  inline ExtentPosition(const extent_vector_base_ptr base, difference_type off) : mExtentVectorBase(base)
  {
    assert(base != nullptr);
    mExtentIndex = extent_size_type(0);
    mElementIndex = difference_type(0);
    mVectorIndex = difference_type(0);
    this->operator+=(off);
  }

  inline bool operator<(ExtentPosition const& e)
  {
    assert(this->mExtentVectorBase == e.mExtentVectorBase);
    return mVectorIndex < e.mVectorIndex;
  }

  inline bool operator==(ExtentPosition const& e)
  {
    assert(this->mExtentVectorBase == e.mExtentVectorBase);
    return mVectorIndex == e.mVectorIndex;
  }

  inline bool operator!=(ExtentPosition const& e)
  {
    assert(this->mExtentVectorBase == e.mExtentVectorBase);
    return mVectorIndex != e.mVectorIndex;
  }

  inline bool operator<=(ExtentPosition const& e)
  {
    assert(this->mExtentVectorBase == e.mExtentVectorBase);
    return mVectorIndex <= e.mVectorIndex;
  }

  inline bool operator>(ExtentPosition const& e)
  {
    assert(this->mExtentVectorBase == e.mExtentVectorBase);
    return mVectorIndex > e.mVectorIndex;
  }

  inline bool operator>=(ExtentPosition const& e)
  {
    assert(this->mExtentVectorBase == e.mExtentVectorBase);
    return mVectorIndex >= e.mVectorIndex;
  }

  inline ExtentPosition& operator--()
  {
    const auto cap = mExtentVectorBase->capacity();

    mVectorIndex--;

    // underflow
    if (mVectorIndex < 0) {
      mExtentIndex = 0;
      mElementIndex = 0;
      return *this;
    }

    // still in overflow?
    if (mVectorIndex >= cap) {
      assert(mExtentIndex == 0 && mElementIndex == 0);
      return *this;
    }

    if (mVectorIndex == cap - 1) {
      // return from overflow
      mExtentIndex = mExtentVectorBase->extent_count() - 1;
      mElementIndex = mExtentVectorBase->extent(mExtentIndex).capacity() - 1;
    } else if (mElementIndex > 0) {
      mElementIndex--;
    } else if (mExtentIndex > 0) {
      mExtentIndex--;
      mElementIndex = mExtentVectorBase->extent(mExtentIndex).capacity() - 1;
    } else {
      // not reachable
      assert(false);
    }
    return *this;
  }

  inline ExtentPosition operator--(int)
  {
    ExtentPosition pos = *this;
    this->operator--();
    return pos;
  }

  inline ExtentPosition& operator++()
  {
    const auto cap = mExtentVectorBase->capacity();

    mVectorIndex++;

    // overflow
    if (mVectorIndex >= cap) {
      mExtentIndex = 0;
      mElementIndex = 0;
      return *this;
    }

    // still in underflow?
    if (mVectorIndex < 0) {
      assert(mExtentIndex == 0 && mElementIndex == 0);
      return *this;
    }

    if (mVectorIndex == 0) {
      // return from underflow
      assert(mExtentIndex == 0 && mElementIndex == 0);
    } else if (mElementIndex < mExtentVectorBase->extent(mExtentIndex).capacity() - 1) {
      mElementIndex++;
    } else if (mExtentIndex < mExtentVectorBase->extent_count() - 1) {
      mExtentIndex++;
      mElementIndex = 0;
    } else {
      // not reachable
      assert(false);
    }
    return *this;
  }

  inline ExtentPosition operator++(int)
  {
    ExtentPosition pos = *this;
    this->operator++();
    return pos;
  }

  inline ExtentPosition& operator+=(difference_type n)
  {
    if (n == 0)
      return *this;

    if (n < 0) {
      *this -= std::abs(n);
      return *this;
    }

    // overflow
    if (mVectorIndex + n >= mExtentVectorBase->capacity()) {
      mVectorIndex += n;
      mExtentIndex = 0;
      mElementIndex = 0;
      return *this;
    }

    // check if in underflow
    if (mVectorIndex < 0) {
      const auto under = std::min(std::abs(mVectorIndex) - 1, n);
      mVectorIndex += under;
      n -= under;
      // use increment to recover from underflow
      if (n > 0) {
        assert(mVectorIndex == -1);
        this->operator++();
        n--;
      }
    }

    // the iterator is not in underflow
    // operation will not cause overflow
    mVectorIndex += n;

    while (n > 0) {
      auto nElems = std::min(
        n, static_cast<difference_type>(mExtentVectorBase->extent(mExtentIndex).capacity() - mElementIndex - 1));

      mElementIndex += nElems;
      n -= nElems;

      if (n > 0) {
        n--;
        mExtentIndex += 1;
        mElementIndex = 0;
      }
    }

    assert(n == 0);
    assert(mExtentIndex >= 0);
    assert(mExtentIndex < mExtentVectorBase->extent_count());
    assert(is_defined());

    return *this;
  }

  inline ExtentPosition& operator-=(difference_type n)
  {
    if (n == 0)
      return *this;

    if (n < 0) {
      *this += std::abs(n);
      return *this;
    }

    // underflow
    if (mVectorIndex - n < 0) {
      mVectorIndex -= n;
      mExtentIndex = 0;
      mElementIndex = 0;
      return *this;
    }

    const auto vcap = mExtentVectorBase->capacity();

    // check if in overflow
    if (mVectorIndex >= vcap) {
      const auto over = std::min(static_cast<difference_type>(mVectorIndex - vcap), n);
      mVectorIndex -= over;
      n -= over;
      // use decrement to recover from overflow
      if (n > 0) {
        assert(mVectorIndex == vcap);
        this->operator--();
        n--;
      }
    }

    // the iterator is not in overflow
    // the operation will not cause underflow
    mVectorIndex -= n;

    while (n > 0) {
      auto nElems = std::min(mElementIndex, n);

      mElementIndex -= nElems;
      n -= nElems;

      if (n > 0) {
        n--;
        mExtentIndex -= 1;
        mElementIndex = mExtentVectorBase->extent(mExtentIndex).capacity() - 1;
      }
    }

    assert(n == 0);
    assert(mExtentIndex >= 0);
    assert(mExtentIndex < mExtentVectorBase->extent_count());
    assert(is_defined());

    return *this;
  }

  inline friend ExtentPosition operator+(const ExtentPosition& x, difference_type off)
  {
    ExtentPosition pos = x;
    pos += off;
    return pos;
  }

  inline friend ExtentPosition operator+(difference_type off, const ExtentPosition& x)
  {
    return x + off;
  }

  inline friend ExtentPosition operator-(const ExtentPosition& x, difference_type off)
  {
    ExtentPosition pos = x;
    pos -= off;
    return pos;
  }

  inline friend ExtentPosition operator-(difference_type off, const ExtentPosition& x)
  {
    return x - off;
  }

  inline friend difference_type operator-(const ExtentPosition& a, const ExtentPosition& b)
  {
    return a.mVectorIndex - b.mVectorIndex;
  }

  inline reference operator*() const
  {
    // XXX: shoul not check in release build
    if (is_dereferenceable()) {
      return mExtentVectorBase->extent(mExtentIndex)[mElementIndex];
    } else {
      assert(false);
      throw std::runtime_error("iterator not dereferenceable");
    }
  }

  inline pointer operator->() const
  {
    return std::pointer_traits<pointer>::pointer_to(this->operator*());
  }

  inline reference operator[](difference_type off) const
  {
    return *(*this + off);
  }

  inline bool is_past_end() const
  {
    return (mExtentVectorBase != nullptr) && (mVectorIndex >= mExtentVectorBase->capacity());
  }
  inline bool is_before_begin() const
  {
    return (mExtentVectorBase != nullptr) && (mVectorIndex < 0);
  }
  inline bool is_defined() const
  {
    return !is_past_end() && !is_before_begin();
  }
  inline bool is_dereferenceable() const
  {
    return (mExtentVectorBase != nullptr) && (!is_before_begin()) && (mVectorIndex < mExtentVectorBase->size());
  }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ExtentVectorBase
//
////////////////////////////////////////////////////////////////////////////////////////////////////

template <class T, class Allocator>
class ExtentVectorBase : Allocator {

public:
  typedef std::allocator_traits<Allocator> alloc_traits;
  typedef typename std::size_t size_type;
  typedef typename alloc_traits::difference_type difference_type;
  typedef std::allocator_traits<Allocator> allocator_traits_type;
  typedef typename allocator_traits_type::value_type value_type;
  typedef typename allocator_traits_type::pointer pointer;

  typedef ExtentPosition<false, Allocator> base_iterator;
  typedef ExtentPosition<true, Allocator> const_base_iterator;

  ExtentVectorBase() : Allocator(), mBackPosition(this, 0)
  {
  }
  explicit ExtentVectorBase(const Allocator& alloc) : Allocator(alloc), mBackPosition(this, 0)
  {
  }

  inline Allocator& allocator()
  {
    return *this;
  }
  inline const Allocator& allocator() const
  {
    return *this;
  }

  inline size_type capacity() const
  {
    return mCapacity;
  }
  inline size_type size() const noexcept
  {
    return mSize;
  }
  inline bool empty() const noexcept
  {
    return mSize == 0;
  }

  const Extent<Allocator>& extent(size_type extent_idx) const
  {
    return mExtents[extent_idx];
  }
  Extent<Allocator>& extent(size_type extent_idx)
  {
    return mExtents[extent_idx];
  }
  // const Extent<Allocator>& extent(size_type extent_idx) const { return mExtents[extent_idx]; }
  // size_type extentCapacity(size_type extent_idx) const { return mExtents[extent_idx].getCapacity(); }
  size_type extent_count() const
  {
    return mExtents.size();
  }

  inline void make_space_for(size_type n, bool exact = false)
  {
    assert((uint64_t)n < ((uint64_t)1 << 40)); // catch overflows

    // count unused capacity of the last extent
    auto free = mCapacity - mSize;
    if (free >= n)
      return;

    n -= free;

    auto new_cap = expand_size(capacity() + n, exact);
    assert(new_cap >= n + capacity());
    auto new_ext_size = new_cap - capacity();
    // create an extent of required size
    // TODO: call allocator to determine the best count (size)
    assert(new_ext_size > 0);
    mExtents.emplace_back(new_ext_size, allocator());
    mCapacity += new_ext_size;
    mBackPosition = base_iterator(this, mSize);
  }

  inline void reserve(size_type new_cap)
  {
    if (capacity() >= new_cap)
      return;

    make_space_for(new_cap - size(), true /* be exact*/);
  }

  inline void shrink_to_fit()
  {
    while ((mExtents.size() > 0) && (mExtents.back().size() == 0)) {
      auto& e = mExtents.back();
      assert(e.capacity() > 0);
      if (e.size() == 0) {
        mCapacity -= e.capacity();
        mExtents.pop_back();
        mBackPosition = base_iterator(this, mSize);
      }
    }
  }

  inline base_iterator get_iterator_at(size_type idx)
  {
    return base_iterator(this, idx);
  }

  inline const_base_iterator get_const_iterator_at(size_type idx) const
  {
    return const_base_iterator(this, idx);
  }

  inline base_iterator begin()
  {
    return base_iterator(this, 0);
  }
  inline const_base_iterator begin() const
  {
    return const_base_iterator(this, 0);
  }
  inline base_iterator end()
  {
    return mBackPosition;
  }
  inline const_base_iterator end() const
  {
    return mBackPosition;
  }

  inline const_base_iterator fill_overwrite(const_base_iterator dst, size_type n, value_type const&& v)
  {
    assert(n > 0);
    assert(mBackPosition - dst >= n);

    for (size_type i = 0; i < n; i++, dst++) {
      assert(dst.mExtentIndex < extent_count());
      auto& e = mExtents[dst.mExtentIndex];

      assert(dst.mElementIndex < e.size());
      e.emplace_replace(dst.mElementIndex, std::move(v));
    }

    return dst;
  }

  inline void fill_expand(size_type n, value_type const&& v)
  {
    make_space_for(n);

    for (size_type i = 0; i < n; i++, mBackPosition++, mSize++) {
      assert(mBackPosition.mExtentIndex < extent_count());
      auto& e = mExtents[mBackPosition.mExtentIndex];
      assert(e.size() == mBackPosition.mElementIndex);

      e.emplace_back(std::move(v));
    }
  }

  inline const_base_iterator fill_n(const_base_iterator dst, value_type const&& v, size_type n)
  {
    if (dst > mBackPosition)
      throw std::runtime_error("writing past the end");

    if (dst < mBackPosition) {
      const auto overlap = std::min(mBackPosition - dst, static_cast<difference_type>(n));
      fill_overwrite(dst, overlap, std::move(v));
      n -= overlap;
    }

    if (n > 0)
      fill_expand(n, std::move(v));

    return dst;
  }

  inline void resize(size_type n, const value_type& v = T{})
  {
    if (n == mSize) {
      return;
    } else if (n > mSize) {
      fill_expand(n - mSize, std::move(v));
    } else {
      while (mSize > n)
        pop_back();
    }
  }

  template <class... Args>
  inline void emplace_back(Args&&... args)
  {
    make_space_for(1);
    assert(mBackPosition.mExtentIndex < extent_count());
    auto& e = mExtents[mBackPosition.mExtentIndex];

    assert(mBackPosition.mElementIndex == e.size());
    e.emplace_back(std::forward<Args>(args)...);

    mSize++;
    mBackPosition++;
  }

  inline void push_back(const T& value)
  {
    this->emplace_back(std::move(value));
  }

  template <class... Args>
  inline base_iterator emplace(const_base_iterator pos, size_type n, Args&&... args)
  {
    if (pos > mBackPosition)
      throw std::out_of_range("insert/emplace past the end");

    if (n == 0)
      return base_iterator(this,
                           pos.mVectorIndex); // defined in http://en.cppreference.com/w/cpp/container/vector/insert

    if (pos == mBackPosition) {
      fill_expand(n, std::forward<Args>(args)...);
      return base_iterator(this, pos.mVectorIndex);
    }

    assert(pos < mBackPosition);

    fill_expand(n, T{});
    const_base_iterator pos_end = mBackPosition - n;
    std::copy_backward(pos, pos_end, mBackPosition);

    fill_overwrite(pos, n, std::forward<Args>(args)...);
    return base_iterator(this, pos.mVectorIndex);
  }

  inline void pop_back()
  {
    if (mSize == 0)
      throw std::out_of_range("pop_back: empty");

    const auto it = mBackPosition - 1;
    assert(it.mExtentIndex < mExtents.size());
    assert(it.mVectorIndex == mSize - 1);

    auto& ext = mExtents[it.mExtentIndex];
    ext.pop_back();
    mSize--;
    mBackPosition = it;
  }

  inline void clear() noexcept
  {
    for (auto& ext : mExtents) {
      mSize -= ext.size();
      ext.clear();
    }

    assert(mSize == 0);
    mBackPosition = base_iterator(this, 0);
  }

  template <typename UnaryFunction>
  inline UnaryFunction for_each(const_base_iterator si, const_base_iterator ei, UnaryFunction func) const
  {
    size_type elems = ei - si;

    auto ext_idx = si.mExtentIndex;
    size_type ext_start_idx = si.mElementIndex;

    while (elems > 0) {
      auto& ext = extent(ext_idx);
      auto cnt = std::min(ext.size() - ext_start_idx, elems);

      auto* first = &ext[ext_start_idx];
      const auto* last = &ext[ext_start_idx + cnt - 1];

      for (; first <= last; first++)
        func(*first);

      elems -= cnt;
      ext_start_idx = 0;
      ext_idx += 1;
    }

    return std::move(func);
  }

  template <typename UnaryFunction>
  inline UnaryFunction for_each(base_iterator si, base_iterator ei, UnaryFunction func)
  {
    size_type elems = ei - si;

    auto ext_idx = si.mExtentIndex;
    size_type ext_start_idx = si.mElementIndex;

    while (elems > 0) {
      auto& ext = extent(ext_idx);
      auto cnt = std::min(ext.size() - ext_start_idx, elems);

      auto* first = &ext[ext_start_idx];
      const auto* last = &ext[ext_start_idx + cnt - 1];

      for (; first <= last; first++)
        func(*first);

      elems -= cnt;
      ext_start_idx = 0;
      ext_idx += 1;
    }

    return std::move(func);
  }

private:
  size_type expand_size(size_type req_cap, bool exact = false)
  {
    static const size_t PAGESIZE_ALIGN = 4096;

    assert(size() <= capacity());
    auto next_cap = std::max(capacity(), size_type(2));

    // If explicit resize requested ignore the expand policies
    if (exact)
      next_cap = std::max(req_cap, capacity());
    else
      next_cap = std::max(next_cap * 3 / 2, req_cap); // TODO: expand policies

    // align to full page
    next_cap = (next_cap * sizeof(T) + PAGESIZE_ALIGN - 1) / PAGESIZE_ALIGN * PAGESIZE_ALIGN / sizeof(T);

    assert((next_cap * sizeof(T)) % PAGESIZE_ALIGN < sizeof(T));
    return next_cap;
  }

private:
  ExtentVector<Allocator> mExtents;
  std::size_t mSize = size_type(0);
  std::size_t mCapacity = size_type(0);
  base_iterator mBackPosition;
};

} // detail

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// O2ExtentVector
//
////////////////////////////////////////////////////////////////////////////////////////////////////

template <class T, class Allocator = std::allocator<T>>
class O2ExtentVector {

public:
  using value_type = T;
  using size_type = std::size_t;
  using reference = typename detail::Extent<Allocator>::reference;
  using const_reference = typename detail::Extent<Allocator>::const_reference;
  using iterator = typename detail::ExtentPosition<false, Allocator>;
  using const_iterator = typename detail::ExtentPosition<true, Allocator>;

  explicit O2ExtentVector(const Allocator& alloc) : mBase(alloc)
  {
  }
  O2ExtentVector() : O2ExtentVector(Allocator())
  {
  }
  O2ExtentVector(size_type n, const T& value, const Allocator& alloc = Allocator()) : mBase(alloc)
  {
    mBase.fill_expand(n, std::move(value));
  }
  explicit O2ExtentVector(size_type n, const Allocator& alloc = Allocator()) : O2ExtentVector(n, T{}, alloc)
  {
  }

#ifdef TODO
  O2ExtentVector(const O2ExtentVector& other) : mBase(other.mBase)
  {
  }
  O2ExtentVector(const O2ExtentVector& other, const Allocator& alloc)
  {
  }
  O2ExtentVector(O2ExtentVector&& other)
  {
  }
  O2ExtentVector(O2ExtentVector&& other, const Allocator& alloc)
  {
  }
#endif

  inline bool empty() const noexcept
  {
    return mBase.empty();
  }
  inline size_type size() const noexcept
  {
    return mBase.size();
  }
  inline size_type max_size() const noexcept
  {
    return std::numeric_limits<size_type>::max();
  }
  inline size_type capacity() const noexcept
  {
    return mBase.capacity();
  }

  inline reference at(size_type pos)
  {
    if (pos >= mBase.size())
      throw std::out_of_range("O2ExtentVector::at() access out of range");

    return *(mBase.get_iterator_at(pos));
  }

  inline const_reference at(size_type pos) const
  {
    if (pos >= size())
      throw std::out_of_range("O2ExtentVector::at() access out of range");

    return *(mBase.get_iterator_at(pos));
  }

  inline reference operator[](size_type pos)
  {
    return *(mBase.get_iterator_at(pos));
  }
  inline const_reference operator[](size_type pos) const
  {
    return *(mBase.get_iterator_at(pos));
  }

  inline reference front()
  {
    return *(mBase.get_iterator_at(0));
  }
  const_reference front() const
  {
    return *(mBase.get_iterator_at(0));
  }

  inline reference back()
  {
    return *(mBase.end() - 1);
  }
  inline const_reference back() const
  {
    return *(mBase.end() - 1);
  }

  template <class... Args>
  inline iterator emplace(const_iterator pos, Args&&... args)
  {
    return mBase.emplace(pos, 1, std::forward<Args>(args)...);
  }
  inline iterator insert(const_iterator pos, const T& value)
  {
    return mBase.emplace(pos, 1, std::move(value));
  }
  inline iterator insert(const_iterator pos, T&& value)
  {
    return mBase.emplace(pos, 1, std::move(value));
  }
  inline iterator insert(const_iterator pos, size_type count, const T& value)
  {
    return mBase.emplace(pos, count, std::move(value));
  }

#ifdef TODO
  template <class InputIt>
  iterator insert(const_iterator pos, InputIt first, InputIt last);
  iterator insert(const_iterator pos, std::initializer_list<T> ilist);
#endif

  inline void push_back(const T& value)
  {
    mBase.push_back(value);
  }
  inline void push_back(T&& value)
  {
    mBase.emplace_back(std::move(value));
  }

  inline void pop_back()
  {
    mBase.pop_back();
  }

  template <class... Args>
  inline void emplace_back(Args&&... args)
  {
    mBase.emplace_back(std::forward<Args>(args)...);
  }

  inline void resize(size_type count)
  {
    mBase.resize(count);
  }
  inline void resize(size_type count, const T& value)
  {
    mBase.resize(count, value);
  }

  inline void clear() noexcept
  {
    mBase.clear();
  }

  inline void reserve(size_type new_cap)
  {
    mBase.reserve(new_cap);
  }
  inline void shrink_to_fit()
  {
    mBase.shrink_to_fit();
  }

  // Iterators
  iterator begin() noexcept
  {
    return mBase.get_iterator_at(0);
  }
  const_iterator begin() const noexcept
  {
    return mBase.get_const_iterator_at(0);
  }
  const_iterator cbegin() const noexcept
  {
    return mBase.get_const_iterator_at(0);
  }

  iterator end() noexcept
  {
    return mBase.end();
  }
  const_iterator end() const noexcept
  {
    return mBase.end();
  }
  const_iterator cend() const noexcept
  {
    return mBase.end();
  }

private:
  detail::ExtentVectorBase<T, Allocator> mBase;
};

/**
 * TODO:
 *   - copy / move constructors ?
 *   - insert/emplace
 *   - reverse iterators
 *   - range inserts
 *   - debug: iterator invalidation
 *   - sealing
 */

namespace o2 { namespace algorithm {

template <class Iterator, typename UnaryFunction>
inline UnaryFunction for_each(Iterator first, Iterator last, UnaryFunction func)
{
  return first.mExtentVectorBase->for_each(first, last, func);
}

} } /* namespace o2::algorithm */

#endif /* end of include guard: O2_CONTAINER_O2EXTENTVECTOR_H */
