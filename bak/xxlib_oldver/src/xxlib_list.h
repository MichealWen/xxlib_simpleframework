#pragma once
#include "xxlib_defines.h"
#include "xxlib_mpobject.h"
#include "xxlib_bytesutils.h"
#include "xxlib_mempool.h"
#include <cassert>

namespace xxlib
{
	// ���� std vector / .net List �ļ�����
	// autoRelease ��˼�ǵ�Ŀ������Ϊ MPObject* ����ʱ, ����ʱ�� call �� AddRef, ����ʱ�� call �� Release
	// reservedHeaderLen Ϊ���� buf �ڴ����ǰ��ճ�һ���ڴ治��, Ҳ����ʼ��, ���ݲ�����( Ϊ����ͷ�����ݴ������ )
	template<typename T, bool autoRelease = false, uint32_t reservedHeaderLen = 0>
	struct List : public MPObject
	{
		static_assert(!autoRelease || (std::is_pointer<T>::value && IsMPObject<T>::value), "autoRelease == true cond is T* where T : MPObject");

		typedef T ChildType;
		T*			buf;
		uint32_t    bufLen;
		uint32_t    dataLen;

	protected:
		friend MemPool;
		explicit List(uint32_t capacity = 0)
		{
			if (capacity == 0)
			{
				buf = nullptr;
				bufLen = 0;
			}
			else
			{
				auto bufByteLen = Round2n((reservedHeaderLen + capacity) * sizeof(T) + 8) - 8;
				buf = (T*)mempool().Alloc((uint32_t)bufByteLen) + reservedHeaderLen;
				bufLen = uint32_t(bufByteLen / sizeof(T)) - reservedHeaderLen;
			}
			dataLen = 0;
		}
		List(MemPool&) : List(0) {}		// for �����л�
		~List()
		{
			Clear(true);
		}
		List(List const&o) = delete;
		List& operator=(List const&o) = delete;

		template<typename U = T> void ItemAddRef(std::enable_if_t<!autoRelease, U> &p) {}
		template<typename U = T> void ItemAddRef(std::enable_if_t< autoRelease, U> &p) { if (p) p->AddRef(); }
		template<typename U = T> void ItemRelease(std::enable_if_t<!autoRelease, U> &p) {}
		template<typename U = T> void ItemRelease(std::enable_if_t< autoRelease, U> &p) { if (p) p->Release(); }
	public:

		void Reserve(uint32_t capacity)
		{
			if (capacity <= bufLen) return;

			auto newBufByteLen = Round2n(reservedHeaderLen + capacity * sizeof(T) + 8) - 8;
			auto newBuf = (T*)mempool().Alloc((uint32_t)newBufByteLen) + reservedHeaderLen;

			if (std::is_trivial<T>::value || MemmoveSupport<T>::value)
			{
				memcpy(newBuf, buf, dataLen * sizeof(T));
			}
			else
			{
				for (uint32_t i = 0; i < dataLen; ++i)
				{
					new (&newBuf[i]) T((T&&)buf[i]);
					buf[i].~T();
				}
			}
			// memcpy(newBuf - reservedHeaderLen, buf - reservedHeaderLen, reservedHeaderLen * sizeof(T));

			if (buf) mempool().Free(buf - reservedHeaderLen);
			buf = newBuf;
			bufLen = uint32_t(newBufByteLen / sizeof(T)) - reservedHeaderLen;
		}

		// ��������ǰ�ĳ���
		uint32_t Resize(uint32_t len)
		{
			if (len == dataLen) return dataLen;
			else if (len < dataLen && !std::is_trivial<T>::value)
			{
				for (uint32_t i = len; i < dataLen; ++i)
				{
					buf[i].~T();
				}
			}
			else // len > dataLen
			{
				Reserve(len);
				if (!std::is_pod<T>::value)
				{
					for (uint32_t i = dataLen; i < len; ++i)
					{
						new (buf + i) T();
					}
				}
			}
			auto rtv = dataLen;
			dataLen = len;
			return rtv;
		}

		T const& operator[](uint32_t idx) const
		{
			assert(idx < dataLen);
			return buf[idx];
		}
		T& operator[](uint32_t idx)
		{
			assert(idx < dataLen);
			return buf[idx];
		}
		T const& At(uint32_t idx) const
		{
			assert(idx < dataLen);
			return buf[idx];
		}
		T& At(uint32_t idx)
		{
			assert(idx < dataLen);
			return buf[idx];
		}

		uint32_t Count() const
		{
			return dataLen;
		}

		T& Top()
		{
			assert(dataLen > 0);
			return buf[dataLen - 1];
		}
		void Pop()
		{
			assert(dataLen > 0);
			--dataLen;
			ItemRelease(buf[dataLen]);
			buf[dataLen].~T();
		}
		T const& Top() const
		{
			assert(dataLen > 0);
			return buf[dataLen - 1];
		}
		bool TryPop(T& output)
		{
			if (!dataLen) return false;
			output = (T&&)buf[--dataLen];
			ItemRelease(buf[dataLen]);
			buf[dataLen].~T();
			return true;
		}

		void Clear(bool freeBuf = false)
		{
			if (!buf) return;
			for (uint32_t i = 0; i < dataLen; ++i)
			{
				ItemRelease(buf[i]);
				buf[i].~T();
			}
			dataLen = 0;
			if (freeBuf)
			{
				mempool().Free(buf - reservedHeaderLen);
				buf = nullptr;
				bufLen = 0;
			}
		}

		// �Ƴ�ָ��������Ԫ��. Ϊ��������, ���ܲ����ڴ��ƶ�
		void RemoveAt(uint32_t idx)
		{
			assert(idx < dataLen);
			--dataLen;
			if (std::is_trivial<T>::value || MemmoveSupport<T>::value)
			{
				ItemRelease(buf[idx]);
				buf[idx].~T();
				memmove(buf + idx, buf + idx + 1, (dataLen - idx) * sizeof(T));
			}
			else
			{
				for (uint32_t i = idx; i < dataLen; ++i)
				{
					buf[i] = (T&&)buf[i + 1];
				}
				buf[dataLen].~T();
			}
		}

		// �������Ҳ��Ƴ�
		void Remove(T const& v)
		{
			for (uint32_t i = 0; i < dataLen; ++i)
			{
				if (EqualsTo(v, buf[i]))
				{
					RemoveAt(i);
					return;
				}
			}
		}

		// �����Ƴ���, �����Ƿ������������Ϊ
		bool SwapRemoveAt(uint32_t idx)
		{
			assert(idx < dataLen);

			auto& o = buf[idx];
			ItemRelease(o);
			o.~T();

			--dataLen;
			if (dataLen == idx) return false;	// last one
			else if (std::is_trivial<T>::value || MemmoveSupport<T>::value)
			{
				o = buf[dataLen];
			}
			else
			{
				new (&o) T((T&&)buf[dataLen]);
				buf[dataLen].~T();
			}
			return true;
		}

	protected:
		// ��Խ���� ���ӳ�
		void FastAddDirect(T const& v)
		{
			new (buf + dataLen) T(v);
			++dataLen;
		}

		// ��Խ����( ��ֵ�� ) ���ӳ�
		void FastAddDirect(T&& v)
		{
			new (buf + dataLen) T((T&&)v);
			++dataLen;
		}

		// ��Խ����
		void FastAdd(T const& v)
		{
			new (buf + dataLen) T(v);
			ItemAddRef(buf[dataLen]);
			++dataLen;
		}

		// ��Խ����( ��ֵ�� )
		void FastAdd(T&& v)
		{
			new (buf + dataLen) T((T&&)v);
			ItemAddRef(buf[dataLen]);
			++dataLen;
		}

	public:

		// �ƶ� / �������һ�����( ���ӳ� )
		template<typename...VS>
		void AddMultiDirect(VS&&...vs)
		{
			static_assert(sizeof...(vs), "lost args ??");
			Reserve(dataLen + sizeof...(vs));
			std::initializer_list<int>{ (FastAddDirect(std::forward<VS>(vs)), 0)... };
		}

		// �ƶ� / �������һ�����
		template<typename...VS>
		void AddMulti(VS&&...vs)
		{
			static_assert(sizeof...(vs), "lost args ??");
			Reserve(dataLen + sizeof...(vs));
			std::initializer_list<int>{ (FastAdd(std::forward<VS>(vs)), 0)... };
		}

		// ׷��һ�� item( ��ֵ�� )
		void Add(T&& v)
		{
			Emplace((T&&)v);
		}
		// ׷��һ�� item
		void Add(T const& v)
		{
			Emplace(v);
		}

		// ׷��һ�� item( ��ֵ�� ). ���ӳ�
		void AddDirect(T&& v)
		{
			EmplaceDirect((T&&)v);
		}
		// ׷��һ�� item. ���ӳ�
		void AddDirect(T const& v)
		{
			EmplaceDirect(v);
		}

		// �ò���ֱ�ӹ���һ��( ��ǰ��֧��ֱ�Ӿ��� mempool Create MPObject* ) ���ӳ�
		template<typename...Args>
		T& EmplaceDirect(Args &&... args)
		{
			Reserve(dataLen + 1);
			auto& p = buf[dataLen++];
			new (&p) T(std::forward<Args>(args)...);
			return p;
		}

		// �ò���ֱ�ӹ���һ��( ��ǰ��֧��ֱ�Ӿ��� mempool Create MPObject* )
		template<typename...Args>
		T& Emplace(Args &&... args)
		{
			auto& p = EmplaceDirect(std::forward<Args>(args)...);
			ItemAddRef(p);
			return p;
		}

		void InsertAt(uint32_t idx, T&& v)
		{
			EmplaceAt(idx, (T&&)v);
		}
		void InsertAt(uint32_t idx, T const& v)
		{
			EmplaceAt(idx, v);
		}

		// �ò���ֱ�ӹ���һ����ָ��λ��( ��ǰ��֧�־��� mempool ���� MPObject* )
		template<typename...Args>
		T& EmplaceAt(uint32_t idx, Args&&...args)
		{
			Reserve(dataLen + 1);
			if (idx < dataLen)
			{
				if (std::is_trivial<T>::value || MemmoveSupport<T>::value)
				{
					memmove(buf + idx + 1, buf + idx, (dataLen - idx) * sizeof(T));
				}
				else
				{
					new (buf + dataLen) T((T&&)buf[dataLen - 1]);
					for (uint32_t i = dataLen - 1; i > idx; --i)
					{
						buf[i] = (T&&)buf[i - 1];
					}
					buf[idx].~T();
				}
			}
			else idx = dataLen;
			++dataLen;
			new (buf + idx) T(std::forward<Args>(args)...);
			ItemAddRef(buf[idx]);
			return buf[idx];
		}
		void AddRange(T const* items, uint32_t count)
		{
			Reserve(dataLen + count);
			if (std::is_trivial<T>::value || MemmoveSupport<T>::value)
			{
				memcpy(buf, items, count * sizeof(T));
			}
			else
			{
				for (uint32_t i = 0; i < count; ++i)
				{
					new (&buf[dataLen + i]) T((T&&)items[i]);
				}
			}
			for (uint32_t i = 0; i < count; ++i)
			{
				ItemAddRef(buf[dataLen + i]);
			}
			dataLen += count;
		}



		// ����ҵ��ͷ�������. �Ҳ��������� uint32_t(-1)
		uint32_t Find(T const& v)
		{
			for (uint32_t i = 0; i < dataLen; ++i)
			{
				if (EqualsTo(v, buf[i])) return i;
			}
			return uint32_t(-1);
		}

		// ����ҵ��ͷ�������. �Ҳ��������� uint32_t(-1)
		uint32_t Find(std::function<bool(T&)> cond)
		{
			for (uint32_t i = 0; i < dataLen; ++i)
			{
				if (cond(buf[i])) return i;
			}
			return uint32_t(-1);
		}

		// �����Ƿ����
		bool Exists(std::function<bool(T&)> cond)
		{
			return Find(cond) != uint32_t(-1);
		}

		bool TryFill(T& out, std::function<bool(T&)> cond)
		{
			auto idx = Find(cond);
			if (idx == uint32_t(-1)) return false;
			out = buf[idx];
			return true;
		}


		// handler ���� false �� foreach ��ֹ
		void ForEach(std::function<bool(T&)> handler)
		{
			for (uint32_t i = 0; i < dataLen; ++i)
			{
				if (handler(buf[i])) return;
			}
		}




	public:
		// ֻ��Ϊ���� for( auto c : 
		struct Iter
		{
			T *ptr;
			bool operator!=(Iter const& other) { return ptr != other.ptr; }
			Iter& operator++()
			{
				++ptr;
				return *this;
			}
			T& operator*() { return *ptr; }
		};
		Iter begin()
		{
			return Iter{ buf };
		}
		Iter end()
		{
			return Iter{ buf + dataLen };
		}
		Iter begin() const
		{
			return Iter{ buf };
		}
		Iter end() const
		{
			return Iter{ buf + dataLen };
		}


		/*************************************************************************/
		// ʵ�� ToString �ӿ�
		/*************************************************************************/

		virtual void ToString(String &str) const override;


		/**************************************************************************************************************/
		// ���л����
		/**************************************************************************************************************/

		// д
		virtual void ToBBuffer(BBuffer &bb) const override
		{
			ToByteBufferCore(bb);
		}

	protected:
		template<typename U = BBuffer>
		void ToByteBufferCore(std::enable_if_t< IsMPObject<T>::value, U> &bb) const
		{
			bb.Write(dataLen);
			for (uint32_t i = 0; i < dataLen; ++i) bb.Write(buf[i]);
		}

		template<typename U = BBuffer>
		void ToByteBufferCore(std::enable_if_t<!IsMPObject<T>::value, U > &bb) const
		{
			bb.Reserve(bb.dataLen + WriteToBBCalc<T>(dataLen));
			WriteToBB(bb.buf + bb.dataLen, buf, dataLen);
			assert(bb.dataLen <= bb.bufLen);
		}
	public:

		// ��
		virtual int FromBBuffer(BBuffer &bb) override
		{
			return FromBBufferCore(bb);
		}

	protected:
		template<typename U = BBuffer>
		int FromBBufferCore(std::enable_if_t< IsMPObject<T>::value, U> &bb)
		{
			int rtv = 0;
			uint32_t len;
			if ((rtv = bb.Read(len))) return rtv;
			Clear();
			Reserve(len);
			for (uint32_t i = 0; i < len; i++)
			{
				if ((rtv = bb.Read(buf[dataLen]))) goto LabRelease;	// ��;��ʧ����ת�����մ���
				++dataLen;
			}
			return 0;
		LabRelease:
			for (uint32_t i = 0; i < dataLen; i++)
			{
				if (buf[i]) buf[i]->Release();
			}
			dataLen = 0;
			return rtv;
		}
		template<typename U = BBuffer>
		int FromBBufferCore(std::enable_if_t<!IsMPObject<T>::value, U> &bb)
		{
			return ReadFromBB(bb.buf + bb.offset, bb.dataLen, bb.offset, *this);
		}
	public:

		/****************************************************************************************************/
		// ���к��������ֵ����
		/****************************************************************************************************/
	protected:

		// ��䳤�ȹ���
		template<typename VT>
		static uint32_t WriteToBBCalc(uint32_t dataLen)
		{
			return 5 + dataLen * sizeof(VT) + (sizeof(VT) <= 2 || std::is_same<VT, float>::value ? 0 : 1);		// �䳤Ҫ�� sizeof �� 1
		}

		template<typename VT>
		static uint32_t WriteToBB(char *dstBuf, VT* const &in, uint32_t const& dataLen)
		{
			auto writeOffset = BBWriteTo(dstBuf, dataLen);								// д����
			if (sizeof(VT) <= 2 || std::is_same<VT, float>::value)
			{
				memcpy(dstBuf + writeOffset, in, dataLen * sizeof(VT));					// ����ֱ�Ӹ����ڴ�
				writeOffset += dataLen * sizeof(VT);
			}
			else
			{
				for (uint32_t i = 0; i < dataLen; ++i)
				{
					writeOffset += BBWriteTo(dstBuf + writeOffset, in[i]);				// �䳤���д
				}
			}
			return writeOffset;		// ����д�˶೤
		}

		template<typename VT, bool AR, uint32_t HS>
		static int ReadFromBB(char const *srcBuf, uint32_t const &dataLen, uint32_t &offset, List<VT, AR, HS> &out)
		{
			uint32_t len;
			auto rtv = BBReadFrom(srcBuf, dataLen, offset, len);					// ������
			if (!rtv) return rtv;
			//if (len < minLen || (maxLen > 0 && len > maxLen)) return -5;			// todo: �ӳ�������ӳ�������ֵ

			out.Clear();
			if (!len) return 0;

			if (sizeof(VT) <= 2 || std::is_same<VT, float>::value)
			{
				if (offset + len * sizeof(VT) > dataLen) return -1;
				out.Resize(len);
				memcpy(out.buf, srcBuf + offset, len * sizeof(VT));
			}
			else
			{
				out.Resize(len);
				for (uint32_t i = 0; i < len; ++i)
				{
					auto rtv = BBReadFrom(srcBuf, dataLen, offset, out[i]);
					if (rtv)
					{
						out.Clear();
						return rtv;
					}
				}
			}
			return 0;
		}

	};

}
