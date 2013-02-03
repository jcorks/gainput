
#ifndef GAINPUTCONTAINERS_H_
#define GAINPUTCONTAINERS_H_


namespace gainput
{


// -- MurmurHash3 begin --
// http://code.google.com/p/smhasher/wiki/MurmurHash3
// MurmurHash3 was written by Austin Appleby, and is placed in the public
// domain. The author hereby disclaims copyright to this source code.

inline uint32_t rotl32(uint32_t x, int8_t r)
{
	return (x << r) | (x >> (32 - r));
}

inline uint32_t getblock(const uint32_t * p, int i)
{
	return p[i];
}


inline uint32_t fmix(uint32_t h)
{
	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;

	return h;
}

/// Calculates MurmurHash3 for the given key.
/**
 * \param key The key to calculate the hash of.
 * \param len Length of the key in bytes.
 * \param seed Seed for the hash.
 * \param[out] out The hash value, a uint32_t in this case.
 */
inline void MurmurHash3_x86_32(const void * key, int len,
		uint32_t seed, void * out)
{
	const uint8_t * data = (const uint8_t*)key;
	const int nblocks = len / 4;

	uint32_t h1 = seed;

	const uint32_t c1 = 0xcc9e2d51;
	const uint32_t c2 = 0x1b873593;

	//----------
	// body

	const uint32_t * blocks = (const uint32_t *)(data + nblocks*4);

	for(int i = -nblocks; i; i++)
	{
		uint32_t k1 = getblock(blocks,i);

		k1 *= c1;
		k1 = rotl32(k1,15);
		k1 *= c2;

		h1 ^= k1;
		h1 = rotl32(h1,13); 
		h1 = h1*5+0xe6546b64;
	}

	//----------
	// tail

	const uint8_t * tail = (const uint8_t*)(data + nblocks*4);

	uint32_t k1 = 0;

	switch(len & 3)
	{
		case 3: k1 ^= tail[2] << 16;
		case 2: k1 ^= tail[1] << 8;
		case 1: k1 ^= tail[0];
			k1 *= c1; k1 = rotl32(k1,15); k1 *= c2; h1 ^= k1;
	};

	//----------
	// finalization

	h1 ^= len;

	h1 = fmix(h1);

	*(uint32_t*)out = h1;
}

// -- MurmurHash3 end --


/// A std::vector-like data container for POD-types.
/**
 * \tparam T A POD-type to hold in this container.
 */
template<class T>
class Array
{
public:
	static const unsigned DefaultCapacity = 8;

	typedef T* iterator;
	typedef const T* const_iterator;
	typedef T value_type;

	Array(Allocator& allocator, unsigned capacity = DefaultCapacity) :
		allocator_(allocator),
		size_(0),
		capacity_(capacity)
	{
		data_ = static_cast<T*>(allocator_.Allocate(sizeof(T)*capacity_));
	}

	~Array()
	{
		allocator_.Delete(data_);
	}

	iterator begin() { return data_; }
	const_iterator begin() const { return data_; }
	iterator end() { return data_ + size_; }
	const_iterator end() const { return data_ + size_; }

	T& operator[] (unsigned i)
	{
		GAINPUT_ASSERT(i < size_);
		return data_[i];
	}

	const T& operator[] (unsigned i) const
	{
		GAINPUT_ASSERT(i < size_);
		return data_[i];
	}

	void push_back(const value_type& val)
	{
		if (size_ + 1 > capacity_)
		{
			reserve(size_ + 1);
		}
		data_[size_++] = val;
	}

	void reserve(unsigned capacity)
	{
		if (capacity <= capacity_)
			return;
		capacity = (capacity_*capacity_) < capacity ? capacity : (capacity_*capacity_);
		T* newData = static_cast<T*>(allocator_.Allocate(sizeof(T)*capacity));
		memcpy(newData, data_, sizeof(T)*capacity_);
		allocator_.Deallocate(data_);
		data_ = newData;
		capacity_ = capacity;
	}

	void swap(Array<T>& x)
	{
		GAINPUT_ASSERT(&allocator_ == &x.allocator_);
		
		const unsigned size = size_;
		const unsigned capacity = capacity_;
		T* data = data_;

		size_ = x.size_;
		capacity_ = x.capacity_;
		data_ = x.data_;

		x.size_ = size;
		x.capacity_ = capacity;
		x.data_ = data;
	}

#if 0
	// Untested
	iterator erase(iterator pos)
	{
		if (size_ == 0)
			return end();
		GAINPUT_ASSERT(pos >= begin() && pos < end());
		memcpy(pos, pos+1, sizeof(T)*(end()-(pos+1)));
		--size_;
		return pos;
	}
#endif

	void clear() { size_ = 0; }

	bool empty() const { return size_ == 0; }
	unsigned size() const { return size_; }

private:
	Allocator& allocator_;
	unsigned size_;
	unsigned capacity_;
	T* data_;
};

/// A hash table mapping keys to POD-type values.
/**
 * \tparam K The key pointing to a value.
 * \tparam V POD-type being stored in the table.
 */
template<class K, class V>
class HashMap
{
public:
	static const unsigned Seed = 329856235;
	enum { InvalidKey = unsigned(-1) };

	/// An element of the hash table.
	struct Node
	{
		K first;	///< The element's key.
		V second;	///< The element's value.
		uint32_t next;	///< The index of the next element with the same (wrapped) hash; Do not use.
	};

	typedef Node* iterator;
	typedef const Node* const_iterator;


	HashMap(Allocator& allocator) :
		allocator_(allocator),
		keys_(allocator_),
		values_(allocator_)
	{ }

	iterator begin() { return values_.begin(); }
	const_iterator begin() const { return values_.begin(); }
	iterator end() { return values_.begin() + values_.size(); }
	const_iterator end() const { return values_.begin() + values_.size(); }

	unsigned count(const K& k)
	{
		return find(k) != end() ? 1 : 0;
	}

	iterator find(const K& k)
	{
		if (keys_.empty() || values_.empty())
			return end();
		uint32_t h;
		MurmurHash3_x86_32(&k, sizeof(K), Seed, &h);
		const uint32_t ha = h % keys_.size();
		uint32_t vi = keys_[ha];
		while (vi != InvalidKey)
		{
			if (values_[vi].first == k)
			{
				return &values_[vi];
			}
			vi = values_[vi].next;
		}
		return end();
	}
	
	const_iterator find(const K& k) const
	{
		if (keys_.empty() || values_.empty())
			return end();
		uint32_t h;
		MurmurHash3_x86_32(&k, sizeof(K), Seed, &h);
		const uint32_t ha = h % keys_.size();
		uint32_t vi = keys_[ha];
		while (vi != InvalidKey)
		{
			if (values_[vi].first == k)
			{
				return &values_[vi];
			}
			vi = values_[vi].next;
		}
		return end();
	}

	iterator insert(const K& k, const V& v)
	{
		if (keys_.size() + 1 >= values_.size())
		{
			Rehash(values_.size()*2 + 10);
		}

		uint32_t h;
		MurmurHash3_x86_32(&k, sizeof(K), Seed, &h);
		const uint32_t ha = h % keys_.size();
		uint32_t vi = keys_[ha];

		if (vi == InvalidKey)
		{
			keys_[ha] = values_.size();
		}
		else
		{
			for (;;)
			{
				if (values_[vi].next == InvalidKey)
				{
					values_[vi].next = values_.size();
					break;
				}
				else
				{
					vi = values_[vi].next;
				}
			}
		}

		Node node;
		node.first = k;
		node.second = v;
		node.next = InvalidKey;
		values_.push_back(node);

		return &values_[values_.size()-1];
	}

	V& operator[] (const K& k)
	{
		iterator it = find(k);
		if (it == end())
		{
			return insert(k, V())->second;
		}
		else
		{
			return it->second;
		}
	}

	unsigned erase(const K& k)
	{
		if (keys_.empty())
			return 0;
		uint32_t h;
		MurmurHash3_x86_32(&k, sizeof(K), Seed, &h);
		const uint32_t ha = h % keys_.size();
		uint32_t vi = keys_[ha];
		uint32_t prevVi = InvalidKey;
		while (vi != InvalidKey)
		{
			if (values_[vi].first == k)
			{
				if (prevVi == InvalidKey)
				{
					keys_[ha] = values_[vi].next;
				}
				else
				{
					values_[prevVi].next = values_[vi].next;
				}
				values_[vi].first = K();
				values_[vi].second = V();
				values_[vi].next = InvalidKey;
				return 1;
			}
			prevVi = vi;
			vi = values_[vi].next;
		}
		return 0;
	}

	void clear()
	{
		keys_.clear();
		values_.clear();
	}

private:
	Allocator& allocator_;
	Array<uint32_t> keys_;
	Array<Node> values_;

	void Rehash(unsigned newSize)
	{
		Array<uint32_t> keys(allocator_, newSize);
		Array<Node> values(allocator_, values_.size());

		for (unsigned i = 0; i < newSize; ++i)
			keys.push_back(InvalidKey);

		keys_.swap(keys);
		values_.swap(values);

		for (typename Array<Node>::const_iterator it = values.begin();
				it != values.end();
				++it)
		{
			insert(it->first, it->second);
		}
	}

};


/// A ring buffer.
/**
 * \tparam N The number of elements that can be stored in the ring buffer.
 * \tparam T Type of the elements stored in the ring buffer.
 */
template<int N, class T>
class RingBuffer
{
public:
	RingBuffer() :
		nextRead_(0),
		nextWrite_(0)
	{ }


	bool CanGet() const
	{
		 return nextRead_ < nextWrite_;
	}

	unsigned GetCount() const
	{
		return nextWrite_ - nextRead_;
	}

	T Get()
	{
		return buf_[(nextRead_++) % N];
	}

	void Put(T d)
	{
		buf_[(nextWrite_++) % N] = d;
	}

private:
	T buf_[N];
	unsigned nextRead_;
	unsigned nextWrite_;
};



}

#endif

