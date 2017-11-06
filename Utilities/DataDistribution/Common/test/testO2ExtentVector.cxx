// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

#define BOOST_TEST_MODULE Test O2ExtentVectorTests
#define BOOST_TEST_MAIN
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include "Common/O2ExtentVector.h"

#include <iostream>
#include <iomanip>
#include <csignal>
#include <cstdio>
#include <random>
#include <algorithm>
#include <chrono>

static const double sTestLengthSeconds = 60;

template <typename T>
class ClassO2 {
public:
  static std::size_t gConstructorCnt;
  static std::size_t gCopyConstructorCnt;
  static std::size_t gMoveConstructorCnt;
  static std::size_t gDestructorCnt;

  ClassO2()
  {
    gConstructorCnt++;
  }
  explicit ClassO2(int v) : mVal(v)
  {
    gConstructorCnt++;
  }
  ClassO2(const ClassO2& c)
  {
    mVal = c.mVal;
    gCopyConstructorCnt++;
  }
  ClassO2(ClassO2&& c)
  {
    mVal = std::exchange(c.mVal, -1);
    gMoveConstructorCnt++;
  }
  ~ClassO2()
  {
    gDestructorCnt++;
  }
  ClassO2& operator=(const ClassO2& rhs)
  {
    mVal = rhs.mVal;
    return *this;
  }

  template <class U>
  friend class ClassO2;

  template <typename U>
  inline bool operator==(const ClassO2<U>& c) const
  {
    return mVal == c.mVal;
  }

  template <typename U>
  friend std::ostream& operator<<(std::ostream& o, const ClassO2<U>& c);

private:
  int mVal = -1;
};

template <class T>
std::ostream& operator<<(std::ostream& os, const ClassO2<T>& c)
{
  os << c.mVal;
  return os;
}

template <typename T>
std::size_t ClassO2<T>::gConstructorCnt = 0;
template <typename T>
std::size_t ClassO2<T>::gCopyConstructorCnt = 0;
template <typename T>
std::size_t ClassO2<T>::gMoveConstructorCnt = 0;
template <typename T>
std::size_t ClassO2<T>::gDestructorCnt = 0;

int64_t get_val_between(std::mt19937& rng, int64_t min, int64_t max)
{
  std::uniform_int_distribution<int64_t> uni(min, max);
  return uni(rng);
}

BOOST_AUTO_TEST_CASE(O2ExtentVector_RandomOperationtest)
{
  std::cout << std::endl << "O2ExtentVector_andomOperationtest" << std::endl;
  std::cout << "Running for " << sTestLengthSeconds << " seconds.\n";

  O2ExtentVector<ClassO2<int>> vO2(1);
  std::vector<ClassO2<float>> vStd(1);

  enum VecOps {
    ePushBack = 0,
    eEmplaceBack,
    eClear,
    eResize,
    eResizeVal,
    eShrink,
    eEmplace1,
    eInsertN,
    eIterCopyOut,
    eIterCopyIn,
    eIterCmpInc,
    eIterCmpDec,
    eVecOpsCnt
  };

  using coin = std::bernoulli_distribution;

  struct VecOpsParams {
    VecOps mOp;
    coin mProb;
    unsigned mCnt;
    std::string mOpName;
  };

  coin coinflip(0.5);

  static VecOpsParams ops[eVecOpsCnt] = { { ePushBack, coin(.90), 0 },
                                          { eEmplaceBack, coin(.90), 0 },
                                          { eClear, coin(.05), 0 },
                                          { eResize, coin(.10), 0 },
                                          { eResizeVal, coin(.10), 0 },
                                          { eShrink, coin(.02), 0 },
                                          { eEmplace1, coin(.10), 0 },
                                          { eInsertN, coin(.10), 0 },
                                          { eIterCopyOut, coin(.01), 0 },
                                          { eIterCopyIn, coin(.01), 0 },
                                          { eIterCmpInc, coin(.10), 0 },
                                          { eIterCmpDec, coin(.10), 0 } };

  VecOps current_op = ePushBack;
  std::random_device rd;
  std::mt19937 gen(rd());

  size_t max_size = 0;
  size_t run_count = 0;

  const auto lStartTime = std::chrono::high_resolution_clock::now();

  for (;; current_op = (VecOps)((current_op + 1) % eVecOpsCnt), run_count++) {

    if (!ops[current_op].mProb(gen))
      continue;

    if (std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - lStartTime).count() >
        sTestLengthSeconds) {
      std::cout << "Exiting after " << sTestLengthSeconds << " seconds.\n";
      break;
    }

    ops[current_op].mCnt++;

    switch (current_op) {
      case ePushBack: {
        int val = rand();
        vO2.push_back(ClassO2<int>(val));
        vStd.push_back(ClassO2<float>(val));
        break;
      }

      case eEmplaceBack: {
        int val = rand();
        vO2.emplace_back(val);
        vStd.emplace_back(val);
        break;
      }

      case eClear: {
        vO2.clear();
        vStd.clear();
        break;
      }
      case eResize: {
        const unsigned new_size =
          (unsigned)std::max(int64_t(0), (int64_t)vO2.size() + get_val_between(gen, -1000, 10000));
        vO2.resize(new_size);
        vStd.resize(new_size);
        break;
      }
      case eResizeVal: {
        const unsigned new_size =
          (unsigned)std::max(int64_t(0), (int64_t)vO2.size() + get_val_between(gen, -1000, 10000));
        int val = rand();
        vO2.resize(new_size, ClassO2<int>(val));
        vStd.resize(new_size, ClassO2<float>(val));
        break;
      }
      case eShrink: {
        vO2.shrink_to_fit();
        vStd.shrink_to_fit();
        break;
      }
      case eEmplace1: {
        size_t pos = get_val_between(gen, 0, vO2.size());
        int val = rand();
        vO2.emplace(vO2.begin() + pos, ClassO2<int>(val));
        vStd.emplace(vStd.begin() + pos, ClassO2<float>(val));
        break;
      }
      case eInsertN: {
        size_t n = get_val_between(gen, 0, 5000);
        size_t pos = get_val_between(gen, 0, vO2.size());
        int val = rand();
        vO2.insert(vO2.begin() + pos, n, ClassO2<int>(val));
        vStd.insert(vStd.begin() + pos, n, ClassO2<float>(val));
        break;
      }
      case eIterCopyOut: {
        std::vector<O2ExtentVector<ClassO2<int>>::value_type> tv;
        tv.resize(vO2.size());
        std::copy(vO2.begin(), vO2.end(), tv.begin());
        BOOST_REQUIRE(std::equal(tv.begin(), tv.end(), vStd.begin()));
        break;
      }
      case eIterCopyIn: {
        O2ExtentVector<O2ExtentVector<ClassO2<int>>::value_type> tv;
        tv.reserve(vO2.size() / 2);

        std::copy(vO2.begin(), vO2.end(), std::back_inserter(tv));
        BOOST_REQUIRE(std::equal(tv.begin(), tv.end(), vStd.begin()));
        BOOST_REQUIRE(std::equal(vO2.begin(), vO2.end(), tv.begin()));
        break;
      }

      case eIterCmpInc: {
        auto o2i = vO2.cbegin();
        auto stdi = vStd.cbegin();

        for (; o2i != vO2.end(); o2i++, stdi++) {
          if (!(*o2i == *stdi))
            BOOST_REQUIRE_EQUAL(*o2i, *stdi);
        }
      }

      case eIterCmpDec: {
        // TODO: reverse iterator
        auto o2i = vO2.cend() - 1;
        auto stdi = vStd.cend() - 1;

        for (; o2i >= vO2.begin(); o2i--, stdi--) {
          if (!(*o2i == *stdi))
            BOOST_REQUIRE_EQUAL(*o2i, *stdi);
        }
      }
      case eVecOpsCnt: /* compiler */
        break;
    }

    // compare
    BOOST_REQUIRE_EQUAL(vO2.size(), vStd.size());

    // sanity
    BOOST_REQUIRE_LE(vO2.size(), vO2.capacity());

    for (size_t i = 0; i < vO2.size(); i++) {
      if (!(vO2.at(i) == vStd.at(i)))
        BOOST_REQUIRE_EQUAL(vO2.at(i), vStd.at(i)); // // 2x compare: boost is too slow
    }

    // sum up all constructor cunts
    BOOST_REQUIRE_EQUAL(
      ClassO2<int>::gDestructorCnt + vO2.size(),
      ClassO2<int>::gConstructorCnt + ClassO2<int>::gCopyConstructorCnt + ClassO2<int>::gMoveConstructorCnt);

    max_size = std::max(max_size, vO2.size());
  }

  std::cout << "Max size: " << max_size << std::endl;
  for (auto o : ops)
    std::cout << o.mOp << " : " << o.mCnt << std::endl;

  BOOST_TEST_MESSAGE("DONE");
}

BOOST_AUTO_TEST_CASE(O2ExtentVector_for_each)
{
  std::cout << std::endl << "O2ExtentVector_for_each" << std::endl;

  {
    namespace o2algo = o2::algorithm;

    O2ExtentVector<int> v01;
    std::vector<int> v02;

    for (int i = 0; i < 10000; i++) {
      int v = rand();
      v01.push_back(v);
      v02.push_back(v);
    }

    {
      struct Sum {
        Sum() : sum{ 0 }
        {
        }
        void operator()(const int& n)
        {
          sum += n;
        }
        uint64_t sum;
      };

      Sum s01 = o2algo::for_each(v01.cbegin(), v01.cend(), Sum());
      Sum s02 = for_each(v02.begin(), v02.end(), Sum());

      BOOST_REQUIRE_EQUAL(s01.sum, s02.sum);
    }

    {
      struct Sum {
        void operator()(int& n)
        {
          sum += n;
          n++;
        }
        uint64_t sum = 0;
      };

      Sum s01 = o2algo::for_each(v01.begin() + 2, v01.end() - 3, Sum());
      Sum s02 = for_each(v02.begin() + 2, v02.end() - 3, Sum());

      BOOST_REQUIRE_EQUAL(s01.sum, s02.sum);
    }

    {
      auto print = [](const int& n) { std::cout << " " << n; };

      o2algo::for_each(v01.cbegin(), v01.cbegin() + 5, print);
      std::cout << std::endl;
      for_each(v02.cbegin(), v02.cbegin() + 5, print);
      std::cout << std::endl;
    }

    {
      // lambda compile test
      auto math = [](int& n) { n = n * 2; };
      o2algo::for_each(v01.begin(), v01.end(), math);
    }
  }

  BOOST_TEST_MESSAGE("DONE");
}
