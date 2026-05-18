#pragma once

// STL
#include <vector>
#include <mutex>

/*
DO NOT DERIVE FROM THIS CLASS!
*/
template <typename _Ty, typename _Alloc = std::allocator<_Ty>>
class tLockedVector
{
public:
	typedef ::std::vector<_Ty, _Alloc> vector_type;
	typedef typename tLockedVector<_Ty, _Alloc> this_type;
	typedef typename vector_type::const_iterator const_iterator;
	typedef typename vector_type::iterator iterator;
	typedef typename vector_type::size_type size_type;

	void lock()
	{
		lockobj_.lock();
	};

	void unlock()
	{
		lockobj_.unlock();
	};

	bool try_lock()
	{
		return lockobj_.try_lock();
	};

	size_type size() const
	{
		return vec_.size();
	};

	void push_back(const _Ty& _val)
	{
		vec_.push_back(_val);
	};

	void push_back(_Ty&& _val)
	{
		vec_.push_back(std::move(_val));
	};

	void pop_back()
	{
		vec_.pop_back();
	};

	void clear()
	{
		vec_.clear();
	};

	iterator erase(const_iterator _where)
	{
		return vec_.erase(_where);
	};

	iterator erase(const_iterator _first, const_iterator _last)
	{
		return vec_.erase(_first, _last);
	};

	const _Ty& at(size_type _idx) const
	{
		return vec_.at(_idx);
	};

	_Ty& at(size_type _idx)
	{
		return vec_.at(_idx);
	};

	const _Ty& back() const
	{
		return vec_.back();
	};

	_Ty& back()
	{
		return vec_.back();
	};

	const _Ty& front() const
	{
		return vec_.front();
	};

	_Ty& front()
	{
		return vec_.front();
	};

	bool empty() const
	{
		return vec_.empty();
	};

	const_iterator begin() const
	{
		return vec_.begin();
	};

	iterator begin()
	{
		return vec_.begin();
	};

	const_iterator end() const
	{
		return vec_.end();
	};

	iterator end()
	{
		return vec_.end();
	};

	const _Ty& operator[] (size_type _idx) const
	{
		return at(_idx);
	};

	_Ty operator[] (size_type _idx)
	{
		return at(_idx);
	};

	tLockedVector& operator=(const tLockedVector& _rhs)
	{
		vec_ = _rhs.vec_;
		return *this;
	};

	tLockedVector() = default;
	tLockedVector(const tLockedVector& _rhs)
	{
		vec_ = _rhs.vec_;
	};

private:
	std::mutex lockobj_; // Object to protect the vector from concurrent access
	vector_type vec_; // Our delegate vector
};