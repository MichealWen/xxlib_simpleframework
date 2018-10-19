#pragma once
#include "xxlib_defines.h"
#include "xxlib_mpobject.h"
#include "xxlib_list.h"
#include "xxlib_dict.h"
#include "xxlib_string.h"
#include <cstdint>

namespace xxlib
{
	struct BBuffer;

	/*******************************************************************************************************/
	// ����id ӳ��( for ���л�ָ�� )
	/*******************************************************************************************************/

	// 0 ���ڱ�ʾ��ָ��
	template<> struct TypeId<MPObject> { static const uint16_t value = 1; };
	template<> struct TypeId<String> { static const uint16_t value = 2; };
	template<> struct TypeId<BBuffer> { static const uint16_t value = 3; };
	// ��ǰ����֧�� Dict ���л�.  PoolQueue ??
	constexpr uint16_t customTypeIdBegin = 4;


	// ������Ҫ���л�����, ����Ҫӳ��Ϊ typeId. ֵ��Ҫ��������ظ�
	// struct TTTTTTT; namespace xxlib { template<> struct TypeId< TTTTTTT > { static const uint16_t value = customTypeIdBegin + __COUNTER__; }; }

	// Ҳ�����������������
#define MapTypeId( typeName ) namespace xxlib { template<> struct TypeId<typeName> { static const uint16_t value = customTypeIdBegin + __COUNTER__; }; }



	// һЩ gcc Ҫ����������������·����
	template<bool>
	struct ByteBufferWriteSwitcher;
	template<>
	struct ByteBufferWriteSwitcher<false>;
	template<>
	struct ByteBufferWriteSwitcher<true>;


	/*******************************************************************************************************/
	// ֧��ָ������л���
	/*******************************************************************************************************/

	struct BBuffer : public List<char, false, 16>
	{
		typedef List<char, false, 16> BaseType;
		uint32_t									offset = 0;				// ��ָ��ƫ����
		Dict<void*, uint32_t>*						ptrStore = nullptr;		// ��ʱ��¼ key: ָ��, value: offset
		Dict<uint32_t, std::pair<void*, uint32_t>>*	idxStore = nullptr;		// ��ʱ��¼ key: ��offset, value: pair<ptr, typeId>

	protected:
		friend MemPool;
		BBuffer(MemPool&mp) : BaseType(mp) {}
		explicit BBuffer(uint32_t capacity = 0) : BaseType(capacity) {}
		~BBuffer()
		{
			// �������ǲ��������л���
			if (ptrStore)
			{
				ptrStore->Release();
				ptrStore = nullptr;
			}
			if (idxStore)
			{
				idxStore->Release();
				idxStore = nullptr;
			}
		}
		BBuffer(BBuffer const&o) = delete;
		BBuffer& operator=(BBuffer const&o) = delete;

	public:

		/*************************************************************************/
		// ��ͳ pod ��дϵ��( ͨ�� bytesutils ������� / �ػ� ʵ�� )
		/*************************************************************************/

		friend ByteBufferWriteSwitcher<true>;
		friend ByteBufferWriteSwitcher<false>;

		template<typename ...TS>
		void Write(TS const & ...vs)
		{
			ByteBufferWriteSwitcher<ExistsMPObject<TS...>::value>::Exec(*this, vs...);
		}

		template<typename T>
		int Read(std::enable_if_t< IsMPObject<T>::value, T> &v)
		{
			return ReadPtr(v);
		}
		template<typename T>
		int Read(std::enable_if_t<!IsMPObject<T>::value, T> &v)
		{
			return BBReadFrom(buf, dataLen, offset, v);
		}

		template<typename T, typename ...TS>
		int Read(T &v, TS&...vs)
		{
			if (auto rtv = Read<T>(v)) return rtv;
			return Read(vs...);
		}
		int Read() { return 0; }


		// ֱ��׷��д��һ�� buf ( ������¼���� )
		void WriteBuf(char const* buf, uint32_t const& dataLen)
		{
			Reserve(this->dataLen + dataLen);
			memcpy(this->buf + this->dataLen, buf, dataLen);
			this->dataLen += dataLen;
		}

	protected:
		/*************************************************************************/
		// ָ�����л�֧��: WritePtr ϵ��
		/*************************************************************************/

		// MPObject ָ��
		void WritePtr(MPObject const* v)
		{
			assert(ptrStore);
			if (WriteNull(v)) return;
			auto typeId = v->typeId();
			assert(typeId);
			Write((uint32_t)typeId);
			if (WriteOffset(v)) return;

			v->ToBBuffer(*this);
		}

		// MPtr<MPObject> ��ָ
		template<typename T>
		void WritePtr(MPtr<T> const &p)
		{
			WritePtr(p.Ensure());
		}

	public:
		// һ�����ֻ���һ��ָ���������л�, ����������ṩһЩ����
		template<typename T>
		void WriteOnce(T const& v)
		{
			BeginWrite();
			Write(v);
			EndWrite();
		}
	private:

		template<typename T>
		bool WriteNull(T* p)
		{
			if (p) return false;
			Write((uint32_t)0);
			return true;
		}

		// д�� offset ֵ, �����Ƿ�����Ҫ�����������( �����л���, ֻ��Ҫ��� offset ���� )
		template<typename T>
		bool WriteOffset(T* p)
		{
			auto rtv = ptrStore->Add((void*)p, dataLen);
			Write(ptrStore->ValueAt(rtv.index));
			return !rtv.success;
		}

		/*************************************************************************/
		// ReadPtr ϵ��
		/*************************************************************************/

		// MPObject* ָ������: ����ʵ������ FromBBuffer �����л�, Read ���������������� nullptr
		template<typename T>
		int ReadPtr(T* & v)
		{
			// get typeid
			uint32_t tid;
			if (auto rtv = Read(tid)) return rtv;

			// isnull ?
			if (tid == 0)
			{
				v = nullptr;
				return 0;
			}

			// get offset
			uint32_t ptr_offset = 0, bb_offset_bak = offset;
			if (auto rtv = Read(ptr_offset)) return rtv;

			// fill or ref
			if (ptr_offset == bb_offset_bak)
			{
				// ensure inherit
				if (!mempool().IsBaseOf(TypeId<T>::value, tid)) return -2;

				// create instance
				v = (T*)mempool().CreatePtrByTypeId((uint16_t)tid);
				if (v == nullptr) return -3;

				// save to dict
				auto addResult = idxStore->Add(ptr_offset, std::make_pair(v, tid));
				if (!addResult.success) return -5;

				// try to fill data
				if (auto rtv = v->FromBBuffer(*this))
				{
					// fail: release resources
					idxStore->RemoveAt(addResult.index);
					mempool().Release(v);						// ������������, ��������Ҫ��Ӧ��ԭ��һ������ݳ���
					v = nullptr;
					return rtv;
				}
			}
			else
			{
				// try get ptr from dict
				typename std::remove_pointer_t<decltype(idxStore)>::ValueType val;
				if (!idxStore->TryGetValue(ptr_offset, val)) return -4;

				// inherit validate
				if (!mempool().IsBaseOf(TypeId<T>::value, std::get<1>(val))) return -2;

				// set val
				v = (T*)std::get<0>(val);
			}
			return 0;
		}

		// ��������� MPtr ��һ���( ��Ҫ����Ϊ����id Ϊ��
		template<typename T>
		int ReadPtr(MPtr<T> & v)
		{
			T* p;
			if (auto rtv = ReadPtr(p))
			{
				v = nullptr;
				return rtv;
			}
			v = p;
			return 0;
		}

		// todo: �����������Ƴ��ȵĹ�������, ��������һ������ӳ���, ������ʱ����������������ֵ, �����Լ�ȥ��. ����Ҫ���ƾ͸ĳ� 0. �Ӷ�����������
		// ��һ��˼·�� FromBBuffer �Ӳ���. ����ÿ�δ�����Ĳ�������. ����������һ�����ƽṹ��, �ṩ���ε��޶�? �ⲿ�־��廹Ҫ������
		//int ReadPtrLimit(T* &v, uint32_t minLen, uint32_t maxLen)		// ���ֺ����������ڲ�ȥ����������ֵ, �Ӷ�����Ҫ��д���巴���л�����.



	public:
#
		// һ�����ֻ���һ��ָ�����������л�, ����������ṩһЩ����
		template<typename T>
		int ReadOnce(T& v)
		{
			BeginRead();
			auto rtv = Read(v);
			EndRead();
			return rtv;
		}


		/*************************************************************************/
		// ָ�����л�֧��: Begin End ϵ��
		/*************************************************************************/

		void BeginWrite()
		{
			if (!ptrStore) mempool().CreateTo(ptrStore, 16);
		}
		void EndWrite()
		{
			assert(ptrStore);
			ptrStore->Clear();
		}
		void BeginRead()
		{
			if (!idxStore) mempool().CreateTo(idxStore, 16);
		}
		void EndRead()
		{
			assert(idxStore);
			idxStore->Clear();
		}

		/*************************************************************************/
		// ʵ�����л��ӿ������л��Լ�
		/*************************************************************************/

		virtual void ToBBuffer(BBuffer &bb) const override 
		{
			bb.Write(dataLen);
			bb.WriteBuf(buf, dataLen);
		}
		virtual int FromBBuffer(BBuffer &bb) override
		{
			uint32_t len = 0;
			if (auto rtv = bb.Read(len)) return rtv;
			if (bb.offset + len > bb.dataLen) return -1;
			WriteBuf(bb.buf + bb.offset, len);
			bb.offset += len;
			return 0;
		}


		/*************************************************************************/
		// ʵ�� ToString �ӿ�
		/*************************************************************************/

		// �ȼ�����
		virtual void ToString(String &str) const override
		{
			str.Append("{ \"typeName\" : \"BBuffer\", \"typeId\" : ", typeId(), ", \"refCount\" : ", refCount(), ", \"versionNumber\" : ", pureVersionNumber(), ", \"len\" : ", dataLen, ", \"data\" : [ ");
			for (size_t i = 0; i < dataLen; i++)
			{
				str.Append((int)(uint8_t)buf[i], ", ");
			}
			if (dataLen) str.dataLen -= 2;
			str.Append(" ] }");
		}

	};




	template<>
	struct ByteBufferWriteSwitcher<false>
	{
		template<typename ...TS>
		static void Exec(BBuffer& bb, TS const& ...vs)
		{
			bb.Reserve(bb.dataLen + BBCalc(vs...));
			bb.dataLen += BBWriteTo(bb.buf + bb.dataLen, vs...);
			assert(bb.dataLen <= bb.bufLen);
		}
	};

	template<>
	struct ByteBufferWriteSwitcher<true>
	{
		template<typename T>
		static void WriteTo(BBuffer& bb, std::enable_if_t< IsMPObject<T>::value, T> const& v)
		{
			bb.WritePtr(v);
		}
		template<typename T>
		static void WriteTo(BBuffer& bb, std::enable_if_t<!IsMPObject<T>::value, T> const& v)
		{
			bb.Reserve(bb.dataLen + BBCalc(v));
			bb.dataLen += BBWriteTo(bb.buf + bb.dataLen, v);
			assert(bb.dataLen <= bb.bufLen);
		}
		template<typename ...TS>
		static void Exec(BBuffer& bb, TS const& ...vs)
		{
			std::initializer_list<int>{ (WriteTo<TS>(bb, vs), 0)... };
		}
	};

}
