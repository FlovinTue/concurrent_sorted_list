
#include <assert.h>
#include <vector>
#include <atomic>

template <class T>
struct TinyLess;

template <class T, class Comparator = TinyLess<uint64_t>>
class heap
{
public:
	typedef size_t SizeType;
	typedef uint64_t KeyType;

	heap();
	heap(const SizeType initCapacity);

	const SizeType size() const;

	void push(const T &in, const KeyType key);
	void push(T&& in, const KeyType key);

	bool try_pop(T& out);
	bool try_pop(T& outValue, KeyType& outKey);

	bool compare_try_pop(T& outValue, KeyType& expectedKey);

	bool try_peek_top_key(KeyType& out);

	void clear();

	void shrink_to_fit();

	void reserve(const size_t capacity);

private:
	void trickle(SizeType index);
	void bubble(SizeType index);

	void pop_internal(T& outValue, KeyType& outKey);

	std::vector<std::pair<KeyType, T>> myStorage;

	const Comparator myComparator;
};
template<class T, class Comparator>
heap<T, Comparator>::heap()
	: myComparator(Comparator())
{
}
template<class T, class Comparator>
inline heap<T, Comparator>::heap(const SizeType initCapacity)
	: heap()
{
	myStorage.reserve(initCapacity);
}
template<class T, class Comparator>
const inline typename heap<T, Comparator>::SizeType heap<T, Comparator>::size() const
{
	return myStorage.size();
}
template<class T, class Comparator>
inline void heap<T, Comparator>::push(const T & in, const KeyType key)
{
	myStorage.emplace_back(std::pair<KeyType, T>( key, in));
	bubble(myStorage.size() - 1);
}
template<class T, class Comparator>
inline void heap<T, Comparator>::push(T && in, const KeyType key)
{
	
	myStorage.emplace_back(std::pair<KeyType, T>( key, std::move(in)));
	bubble(myStorage.size() - 1);
}
template<class T, class Comparator>
inline bool heap<T, Comparator>::try_pop(T & out)
{
	KeyType dummy(0);
	return try_pop(out, dummy);
}
template<class T, class Comparator>
inline bool heap<T, Comparator>::try_pop(T & outValue, KeyType & outKey)
{
	if (!myStorage.size()) {
		
		return false;
	}

	pop_internal(outValue, outKey);

	return true;
}
template<class T, class Comparator>
inline void heap<T, Comparator>::trickle(SizeType index)
{
	const SizeType size(myStorage.size());

	if (index == size - 1) {
		return;
	}

	SizeType index_(index);
	while (index_ < size) {
		const SizeType desiredLeftIndex(index_ * 2 + 1);
		const SizeType desiredRightIndex(index_ * 2 + 2);
		const SizeType leftIndex(desiredLeftIndex < size ? desiredLeftIndex : index_);
		const SizeType rightIndex(desiredRightIndex < size ? desiredRightIndex : index_);
		
		const KeyType& leftKey(myStorage[leftIndex].first);
		const KeyType& rightKey(myStorage[rightIndex].first);

		KeyType targetKey(myStorage[index_].first);
		SizeType targetIndex(index_);

		if (myComparator(leftKey, targetKey)) {
			targetIndex = leftIndex;
			targetKey = leftKey;
		}
		if (myComparator(rightKey, targetKey)) {
			targetIndex = rightIndex;
			targetKey = rightKey;
		}

		if (!(targetIndex ^ index_)) {
			break;
		}

		std::swap(myStorage[index_], myStorage[targetIndex]);

		index_ = targetIndex;
	}
}
template<class T, class Comparator>
inline void heap<T, Comparator>::bubble(SizeType index)
{
	SizeType index_(index);
	SizeType parent((index_ - 1) / 2);

	while (true) {
		if (!index_)
			return;

		if (!myComparator(myStorage[index_].first, myStorage[parent].first))
			break;

		std::swap(myStorage[index_], myStorage[parent]);
		index_ = parent;
		parent = (index_ - 1) / 2;
	}
}
template<class T, class Comparator>
inline void heap<T, Comparator>::pop_internal(T & outValue, KeyType & outKey)
{
	outKey = myStorage[0].first;
	outValue = std::move(myStorage[0].second);

	std::swap(myStorage[0], myStorage[myStorage.size() - 1]);
	myStorage.pop_back();

	trickle(0);
}
template<class T, class Comparator>
inline void heap<T, Comparator>::clear()
{
	myStorage.clear();
}
template<class T, class Comparator>
inline void heap<T, Comparator>::shrink_to_fit()
{
	myStorage.shrink_to_fit();
}
template<class T, class Comparator>
inline void heap<T, Comparator>::reserve(const size_t capacity)
{
	myStorage.reserve(capacity);
}
template<class T, class Comparator>
inline bool heap<T, Comparator>::compare_try_pop(T & outValue, KeyType & expectedKey)
{
	if (!myStorage.size()) {
		
		return false;
	}
	if (myStorage[0].first != expectedKey) {
		expectedKey = myStorage[0].first;
		
		return false;
	}
	pop_internal(outValue, expectedKey);

	return true;
}

template<class T, class Comparator>
inline bool heap<T, Comparator>::try_peek_top_key(KeyType & out)
{
	if (!myStorage.size()) {
		
		return false;
	}
	out = myStorage[0].first;

	return true;
}

template <class T>
struct TinyLess
{
	constexpr const bool operator()(const T& a, const T& b) const
	{
		return a < b;
	};
};
