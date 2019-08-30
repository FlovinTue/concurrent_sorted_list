#pragma once

#include <assert.h>
#include <concurrent_queue_.h>
#include <atomic>

#undef get_object

template <class Object>
class concurrent_object_pool
{
public:
	concurrent_object_pool(const std::size_t blockSize);
	~concurrent_object_pool();

	inline Object* get_object();
	inline void recycle_object(Object* object);

	inline uint32_t avaliable() const;

	inline void unsafe_destroy();

private:
	void try_alloc_block();

	struct block_node
	{
		Object* myBlock;
		std::atomic<block_node*> myPrevious;
	};

	cq::concurrent_queue<Object*> myUnusedObjects;

	std::atomic<block_node*> myLastBlock;

	const std::size_t myBlockSize;
};

template<class Object>
inline concurrent_object_pool<Object>::concurrent_object_pool(const std::size_t blockSize)
	: myBlockSize(blockSize)
	, myUnusedObjects(blockSize)
	, myLastBlock(nullptr)
{
	try_alloc_block();
}
template<class Object>
inline concurrent_object_pool<Object>::~concurrent_object_pool()
{
	unsafe_destroy();
}
template<class Object>
inline Object * concurrent_object_pool<Object>::get_object()
{
	Object* out;

	while (!myUnusedObjects.try_pop(out)) {
		try_alloc_block();
	}
	return out;
}
template<class Object>
inline void concurrent_object_pool<Object>::recycle_object(Object * object)
{
	myUnusedObjects.push(object);
}
template<class Object>
inline uint32_t concurrent_object_pool<Object>::avaliable() const
{
	return static_cast<uint32_t>(myUnusedObjects.size());
}
template<class Object>
inline void concurrent_object_pool<Object>::unsafe_destroy()
{
	block_node* blockNode(myLastBlock.load(std::memory_order_relaxed));
	while (blockNode) {
		block_node* const previous(blockNode->myPrevious);

		delete[] blockNode->myBlock;
		delete blockNode;

		blockNode = previous;
	}
	myLastBlock = nullptr;

	myUnusedObjects.unsafe_clear();
}
template<class Object>
inline void concurrent_object_pool<Object>::try_alloc_block()
{
	block_node* expected(myLastBlock.load(std::memory_order_relaxed));

	if (myUnusedObjects.size()) {
		return;
	}

	Object* const block(new Object[myBlockSize]);

	block_node* const desired(new block_node);
	desired->myPrevious = expected;
	desired->myBlock = block;

	if (!myLastBlock.compare_exchange_strong(expected, desired)) {
		delete desired;
		delete[] block;

		return;
	}

	for (std::size_t i = 0; i < myBlockSize; ++i) {
		myUnusedObjects.push(&block[i]);
	}
}