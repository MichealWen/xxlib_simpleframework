﻿#pragma once
namespace xx
{
	template<typename T>
	List<T>::List(MemPool* mempool, size_t const& capacity)
		: Object(mempool)
	{
		if (capacity == 0)
		{
			buf = nullptr;
			bufLen = 0;
		}
		else
		{
			// 充分利用 2^n 空间
			auto bufByteLen = MemPool::Round2n(capacity * sizeof(T) + sizeof(MemHeader)) - sizeof(MemHeader);
			buf = (T*)mempool->Alloc(bufByteLen);
			if (!buf) throw - 1;
			bufLen = bufByteLen / sizeof(T);
		}
		dataLen = 0;
	}

	template<typename T>
	List<T>::~List()
	{
		Clear(true);
	}

	template<typename T>
	List<T>::List(List<T> &&o)
		: Object(o.mempool)
		, buf(o.buf)
		, bufLen(o.bufLen)
		, dataLen(o.dataLen)
	{
		o.buf = nullptr;
		o.bufLen = 0;
		o.dataLen = 0;
	}


	template<typename T>
	void List<T>::Reserve(size_t const& capacity)
	{
		if (capacity <= bufLen) return;

		auto newBufByteLen = MemPool::Round2n(capacity * sizeof(T) + sizeof(MemHeader)) - sizeof(MemHeader);
		auto newBuf = (T*)mempool->Alloc(newBufByteLen);
		if (!newBuf) throw - 1;

		if constexpr(std::is_trivial<T>::value || IsPtr_v<T>)
		{
			memcpy(newBuf, buf, dataLen * sizeof(T));
		}
		else
		{
			for (size_t i = 0; i < dataLen; ++i)
			{
				new (&newBuf[i]) T((T&&)buf[i]);
				buf[i].~T();
			}
		}

		if (buf) mempool->Free(buf);
		buf = newBuf;
		bufLen = size_t(newBufByteLen / sizeof(T));
	}

	template<typename T>
	size_t List<T>::Resize(size_t const& len)
	{
		if (len == dataLen) return len;
		else if (len < dataLen)
		{
			for (size_t i = len; i < dataLen; ++i)
			{
				buf[i].~T();
			}
		}
		else // len > dataLen
		{
			Reserve(len);
			for (size_t i = dataLen; i < len; ++i)
			{
				new (buf + i) T();
			}
		}
		auto rtv = dataLen;
		dataLen = len;
		return rtv;
	}

	template<typename T>
	T const& List<T>::operator[](size_t const& idx) const noexcept
	{
		assert(idx < dataLen);
		return buf[idx];
	}

	template<typename T>
	T& List<T>::operator[](size_t const& idx) noexcept
	{
		assert(idx < dataLen);
		return buf[idx];
	}

	template<typename T>
	T const& List<T>::At(size_t const& idx) const noexcept
	{
		assert(idx < dataLen);
		return buf[idx];
	}

	template<typename T>
	T& List<T>::At(size_t const& idx) noexcept
	{
		assert(idx < dataLen);
		return buf[idx];
	}

	template<typename T>
	T& List<T>::Top() noexcept
	{
		assert(dataLen > 0);
		return buf[dataLen - 1];
	}

	template<typename T>
	void List<T>::Pop() noexcept
	{
		assert(dataLen > 0);
		--dataLen;
		buf[dataLen].~T();
	}

	template<typename T>
	T const& List<T>::Top() const noexcept
	{
		assert(dataLen > 0);
		return buf[dataLen - 1];
	}

	template<typename T>
	bool List<T>::TryPop(T& output) noexcept
	{
		if (!dataLen) return false;
		output = (T&&)buf[--dataLen];
		buf[dataLen].~T();
		return true;
	}

	template<typename T>
	void List<T>::Clear(bool const& freeBuf) noexcept
	{
		if (!buf) return;
		if (dataLen)
		{
			for (size_t i = dataLen - 1; i != (size_t)-1; --i)
			{
				buf[i].~T();
			}
			dataLen = 0;
		}
		if (freeBuf)
		{
			mempool->Free(buf);
			buf = nullptr;
			bufLen = 0;
		}
	}

	template<typename T>
	void List<T>::RemoveAt(size_t const& idx) noexcept
	{
		assert(idx < dataLen);
		--dataLen;
		if constexpr(std::is_trivial<T>::value || IsPtr_v<T>)
		{
			buf[idx].~T();
			memmove(buf + idx, buf + idx + 1, (dataLen - idx) * sizeof(T));
		}
		else
		{
			for (size_t i = idx; i < dataLen; ++i)
			{
				buf[i] = (T&&)buf[i + 1];
			}
			buf[dataLen].~T();
		}
	}

	template<typename T>
	void List<T>::SwapRemoveAt(size_t const& idx)
	{
		if (idx + 1 < dataLen)
		{
			std::swap(buf[idx], buf[dataLen - 1]);
		}
		dataLen--;
		buf[dataLen].~T();
	}

	template<typename T>
	void List<T>::Remove(T const& v) noexcept
	{
		for (size_t i = 0; i < dataLen; ++i)
		{
			if (EqualsTo(v, buf[i]))
			{
				RemoveAt(i);
				return;
			}
		}
	}

	template<typename T>
	template<typename...Args>
	T& List<T>::Emplace(Args &&... args)
	{
		Reserve(dataLen + 1);
		auto& p = buf[dataLen++];
		if constexpr(std::is_base_of_v<Object, T>)
		{
			new (&p) T(mempool, std::forward<Args>(args)...);
		}
		else
		{
			new (&p) T(std::forward<Args>(args)...);
		}
		return p;
	}

	template<typename T>
	template<typename...Args>
	T& List<T>::EmplaceAt(size_t idx, Args&&...args)
	{
		Reserve(dataLen + 1);
		if (idx < dataLen)
		{
			if constexpr(std::is_trivial<T>::value || IsPtr_v<T>)
			{
				memmove(buf + idx + 1, buf + idx, (dataLen - idx) * sizeof(T));
			}
			else
			{
				new (buf + dataLen) T((T&&)buf[dataLen - 1]);
				for (size_t i = dataLen - 1; i > idx; --i)
				{
					buf[i] = (T&&)buf[i - 1];
				}
				buf[idx].~T();
			}
		}
		else idx = dataLen;
		++dataLen;
		new (buf + idx) T(std::forward<Args>(args)...);
		return buf[idx];
	}


	template<typename T>
	void List<T>::Add(T&& v)
	{
		Emplace((T&&)v);
	}

	template<typename T>
	void List<T>::Add(T const& v)
	{
		Emplace(v);
	}

	template<typename T>
	void List<T>::InsertAt(size_t const& idx, T&& v)
	{
		EmplaceAt(idx, (T&&)v);
	}

	template<typename T>
	void List<T>::InsertAt(size_t const& idx, T const& v)
	{
		EmplaceAt(idx, v);
	}

	template<typename T>
	void List<T>::AddRange(T const* items, size_t count)
	{
		Reserve(dataLen + count);
		if constexpr(std::is_trivial<T>::value || IsPtr_v<T>)
		{
			std::memcpy(buf, items, count * sizeof(T));
		}
		else
		{
			for (size_t i = 0; i < count; ++i)
			{
				new (&buf[dataLen + i]) T((T&&)items[i]);
			}
		}
		dataLen += count;
	}

	template<typename T>
	size_t List<T>::Find(T const& v) noexcept
	{
		for (size_t i = 0; i < dataLen; ++i)
		{
			if (EqualsTo(v, buf[i])) return i;
		}
		return size_t(-1);
	}

	template<typename T>
	size_t List<T>::Find(std::function<bool(T&)> cond)
	{
		for (size_t i = 0; i < dataLen; ++i)
		{
			if (cond(buf[i])) return i;
		}
		return size_t(-1);
	}

	template<typename T>
	bool List<T>::Exists(std::function<bool(T&)> cond)
	{
		return Find(cond) != size_t(-1);
	}

	template<typename T>
	bool List<T>::TryFill(T& out, std::function<bool(T&)> cond)
	{
		auto idx = Find(cond);
		if (idx == size_t(-1)) return false;
		out = buf[idx];
		return true;
	}


	template<typename T>
	void List<T>::ForEachRevert(std::function<bool(T&)> handler)
	{
		for (size_t i = dataLen - 1; i != (size_t)-1; --i)
		{
			if (handler(buf[i])) return;
		}
	}

}
