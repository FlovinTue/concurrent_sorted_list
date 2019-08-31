// Copyright(c) 2019 Flovin Michaelsen
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <atomic>
#include <atomic_shared_ptr.h>
#include <vector>
#include <iostream>
#include <concurrent_object_pool.h>

#ifndef MAKE_UNIQUE_NAME 
#define CONCAT(a,b)  a##b
#define EXPAND_AND_CONCAT(a, b) CONCAT(a,b)
#define MAKE_UNIQUE_NAME(prefix) EXPAND_AND_CONCAT(prefix, __COUNTER__)
#endif

#define CSL_PADD(bytes) const uint8_t MAKE_UNIQUE_NAME(padding)[bytes] {}

namespace gdul
{
namespace csldetail
{

template <class KeyType, class ValueType>
class node;

struct tiny_less;

}

template <class KeyType, class ValueType, class Comparator = csldetail::tiny_less>
class concurrent_sorted_list
{
private:
	template <class Dummy>
	class allocator;

public:
	typedef size_t size_type;
	typedef Comparator comparator_type;
	typedef KeyType key_type;
	typedef ValueType value_type;
	typedef allocator<uint8_t> allocator_type;
	typedef shared_ptr<csldetail::node<key_type, value_type>, allocator_type> shared_ptr_type;
	typedef atomic_shared_ptr<csldetail::node<key_type, value_type>, allocator_type> atomic_shared_ptr_type;
	typedef versioned_raw_ptr<csldetail::node<key_type, value_type>, allocator_type> versioned_raw_ptr_type;

	concurrent_sorted_list();
	~concurrent_sorted_list();

	const size_type size() const;

	void insert(const std::pair<key_type, value_type>& in);
	void insert(std::pair<key_type, value_type>&& in);

	const bool try_pop(value_type& out);
	const bool try_pop(std::pair<key_type, value_type>& out);

	// Compares the value of out.first to that of top key, if they match
	// a pop is attempted. Changes out.first to existing value on faliure
	const bool compare_try_pop(std::pair<key_type, value_type>& out);

	// Top key hint
	const bool try_peek_top_key(key_type& out);

	void unsafe_clear();

private:
	struct alloc_size_rep
	{
		std::pair<key_type, value_type> dummy1;
		atomic_shared_ptr<int> dummy2;
	};
	class alloc_type
	{
		uint8_t myBlock[shared_ptr<alloc_size_rep>::Alloc_Size_Make_Shared];
	};

	template <class Dummy>
	class allocator
	{
	public:
		allocator(concurrent_object_pool<alloc_type>* memPool) : myMemoryPool(memPool) {}
		allocator(const allocator<uint8_t>& other) : myMemoryPool(other.myMemoryPool) {}

		typedef uint8_t value_type;

		uint8_t* allocate(std::size_t /*n*/) {
			return reinterpret_cast<uint8_t*>(myMemoryPool->get_object());
		}
		void deallocate(uint8_t* ptr, std::size_t /*n*/) {
			myMemoryPool->recycle_object(reinterpret_cast<alloc_type*>(ptr));
		}

	private:
		concurrent_object_pool<alloc_type>* myMemoryPool;
	};


	const bool try_insert(shared_ptr_type& entry);
	const bool try_pop_internal(key_type& expectedKey, value_type& outValue, const bool matchKey);


	std::atomic<size_type> mySize;

	CSL_PADD(64 - (sizeof(mySize) % 64));
	concurrent_object_pool<alloc_type> myMemoryPool;
	allocator_type myAllocator;
	CSL_PADD(64 - ((sizeof(myMemoryPool) + sizeof(myAllocator)) % 64));
	shared_ptr_type myFrontSentry;
	comparator_type myComparator;

};

template<class KeyType, class ValueType, class Comparator>
inline concurrent_sorted_list<KeyType, ValueType, Comparator>::concurrent_sorted_list()
	: mySize(0)
	, myMemoryPool(128)
	, myAllocator(&myMemoryPool)
	, myFrontSentry(make_shared<csldetail::node<key_type, value_type>, allocator_type>(myAllocator))
{
	static_assert(std::is_integral<key_type>::value || std::is_floating_point<key_type>::value, "Only integers and floats allowed as key type");
}
template<class KeyType, class ValueType, class Comparator>
inline concurrent_sorted_list<KeyType, ValueType, Comparator>::~concurrent_sorted_list()
{
	unsafe_clear();
}
template<class KeyType, class ValueType, class Comparator>
inline const typename concurrent_sorted_list<KeyType, ValueType, Comparator>::size_type concurrent_sorted_list<KeyType, ValueType, Comparator>::size() const
{
	return mySize.load(std::memory_order_acquire);
}
template<class KeyType, class ValueType, class Comparator>
inline void concurrent_sorted_list<KeyType, ValueType, Comparator>::insert(const std::pair<key_type, value_type>& in)
{
	insert(std::pair<key_type, value_type>(in));
}
template<class KeyType, class ValueType, class Comparator>
inline void concurrent_sorted_list<KeyType, ValueType, Comparator>::insert(std::pair<key_type, value_type>&& in)
{
	shared_ptr_type entry(make_shared<csldetail::node<key_type, value_type>, allocator_type>(myAllocator));
	entry->myKeyValuePair = std::move(in);

	while (!try_insert(entry));

	mySize.fetch_add(1, std::memory_order_relaxed);
}
template<class KeyType, class ValueType, class Comparator>
inline const bool concurrent_sorted_list<KeyType, ValueType, Comparator>::try_pop(value_type & out)
{
	key_type dummy(0);
	return try_pop_internal(dummy, out, false);
}

template<class KeyType, class ValueType, class Comparator>
inline const bool concurrent_sorted_list<KeyType, ValueType, Comparator>::try_pop(std::pair<key_type, value_type>& out)
{
	return try_pop_internal(out.first, out.second, false);
}

template<class KeyType, class ValueType, class Comparator>
inline const bool concurrent_sorted_list<KeyType, ValueType, Comparator>::compare_try_pop(std::pair<key_type, value_type>& out)
{
	return try_pop_internal(out.first, out.second, true);
}

template<class KeyType, class ValueType, class Comparator>
inline const bool concurrent_sorted_list<KeyType, ValueType, Comparator>::try_peek_top_key(key_type & out)
{
	const shared_ptr_type head(myFrontSentry->myNext.load());

	if (!head) {
		return false;
	}

	out = head->myKeyValuePair.first;

	return true;
}

template<class KeyType, class ValueType, class Comparator>
inline void concurrent_sorted_list<KeyType, ValueType, Comparator>::unsafe_clear()
{
	std::vector<csldetail::node<key_type, value_type>*> arr;
	arr.reserve(mySize.load(std::memory_order_acquire));

	csldetail::node<key_type, value_type>* prev(static_cast<csldetail::node<key_type, value_type>*>(myFrontSentry));

	for (size_t i = 0; i < mySize.load(std::memory_order_relaxed); ++i) {
		prev = static_cast<csldetail::node<key_type, value_type>*>(prev->myNext);
		arr.push_back(prev);
	}
	for (typename std::vector<csldetail::node<key_type, value_type>*>::reverse_iterator it = arr.rbegin(); it != arr.rend(); ++it) {
		(*it)->myNext.unsafe_store(nullptr);
	}
	mySize.store(0, std::memory_order_relaxed);
}

template<class KeyType, class ValueType, class Comparator>
inline const bool concurrent_sorted_list<KeyType, ValueType, Comparator>::try_insert(shared_ptr_type& entry)
{
	shared_ptr_type last(nullptr);
	shared_ptr_type current(myFrontSentry->myNext.load());

	csldetail::node<key_type, value_type>* insertionPoint(static_cast<csldetail::node<key_type, value_type>*>(myFrontSentry));

	while (current) {
		if (myComparator(entry->myKeyValuePair.first, current->myKeyValuePair.first)) {
			break;
		}

		shared_ptr_type next(current->myNext.load());

		if (next.get_tag()) {
			next.clear_tag();

			versioned_raw_ptr_type expected(current.get_versioned_raw_ptr());
			if (insertionPoint->myNext.compare_exchange_strong(expected, std::move(next))) {
				shared_ptr_type null(nullptr);
				null.set_tag();
				current->myNext.store(null);
			}

			current = insertionPoint->myNext.load();

			if (current.get_tag()) {
				return false;
			}
		}
		else {
			last = std::move(current);
			current = std::move(next);
			insertionPoint = static_cast<csldetail::node<key_type, value_type>*>(last);
		}
	};

	versioned_raw_ptr_type expected(current.get_versioned_raw_ptr());
	entry->myNext.unsafe_store(std::move(current));

	if (insertionPoint->myNext.compare_exchange_strong(expected, std::move(entry))) {
		return true;
	}

	return false;
}

template<class KeyType, class ValueType, class Comparator>
inline const bool concurrent_sorted_list<KeyType, ValueType, Comparator>::try_pop_internal(key_type & expectedKey, value_type & outValue, const bool matchKey)
{
	const size_type currentSize(mySize.fetch_sub(1, std::memory_order_acq_rel) - 1);
	const size_type difference(std::numeric_limits<size_type>::max() - (currentSize));
	const size_type threshhold(std::numeric_limits<size_type>::max() / 2);

	if (difference < threshhold) {
		mySize.fetch_add(1, std::memory_order_relaxed);
		return false;
	}

	shared_ptr_type head(nullptr);
	shared_ptr_type splice(nullptr);

	versioned_raw_ptr_type expected(nullptr);

	for (;;) {
		head = myFrontSentry->myNext.load();

		const key_type key(head->myKeyValuePair.first);
		if (matchKey & (expectedKey != key)) {
			expectedKey = key;
			return false;
		}

		splice = head->myNext.load_and_tag();
		const bool mine(!splice.get_tag());
		splice.clear_tag();

		expected = head.get_versioned_raw_ptr();
		if (myFrontSentry->myNext.compare_exchange_strong(expected, std::move(splice))) {
			shared_ptr_type null(nullptr);
			null.set_tag();
			head->myNext.store(null);
		}

		if (mine) {
			break;
		}
	}
	expectedKey = head->myKeyValuePair.first;
	outValue = head->myKeyValuePair.second;

	return true;
}
namespace csldetail
{
template <class KeyType, class ValueType>
class node
{
public:
	constexpr node();

	typedef KeyType key_type;
	typedef ValueType value_type;
	typedef typename concurrent_sorted_list<key_type, value_type>::atomic_shared_ptr_type atomic_shared_ptr_type;

	std::pair<key_type, value_type> myKeyValuePair;
	atomic_shared_ptr_type myNext;
};
template<class KeyType, class ValueType>
inline constexpr node<KeyType, ValueType>::node()
	: myKeyValuePair{ std::numeric_limits<key_type>::min(), value_type() }
	, myNext(nullptr)
{
}
struct tiny_less
{
	template <class T>
	constexpr const bool operator()(const T& a, const T& b) const
	{
		return a < b;
	};
};
}
}