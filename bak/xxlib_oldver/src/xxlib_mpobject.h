#pragma once
#include "xxlib_defines.h"
#include <cstdint>

namespace xxlib
{
	struct MemPool;

	/************************************************************************/
	// ���� MemPool ������ڴ�( ����ʲôʹ������ )����������һ��ͷ��( λ�ڷ��ص�ָ���������, ֱ���� MemPool ��ʼ�� )
	/************************************************************************/

	struct MPObjectHeader
	{
		// �����汾��, ��Ϊʶ��ǰָ���Ƿ���Ч����Ҫ��ʶ
		union
		{
			uint64_t versionNumber;
			struct
			{
				uint8_t vnArea[7];
				const uint8_t mpIndex;		// ���� / ����ʱ���ڴ������ �±�( ָ�� versionNumber �����λ�ֽ� )
			};
		};
	};

	/************************************************************************/
	// ʹ�� MemPool ֮ Create / Release ʱ���ӵ�ͷ��( ��ǿ��, ͬ��ֱ���� MemPool ��ʼ�� )
	/************************************************************************/

	struct MPObjectHeaderEx : public MPObjectHeader
	{
		// �����������õ����ڴ��ָ��( �������, ���� )
		MemPool *mempool;

		// ���� 0 ������ Dispose
		uint32_t refCount = 1;

		// MemPool ����ʱ�������ID
		uint16_t typeId = 0;

		// ��ǰ������ ToString ������( ���Ǵ�ո�����ֵ )
		uint16_t tsFlags = 0;
	};


	/************************************************************************/
	// ֧�� MemPool ���඼Ӧ��ʵ�ָû���
	/************************************************************************/

	struct String;
	struct BBuffer;
	struct MPObject
	{
	protected:						// protected ������߱���ЩԪ��
		friend MemPool;
		MPObject() {}				// Ĭ�Ϲ���
		MPObject(MemPool&) {}		// for �����л�
		// ���ƹ���, op=			// = delete
		virtual ~MPObject() {}		// ���������, ҲҪλ�� protected

	public:

		// �ӳ�
		inline void AddRef()
		{
			++refCount();
		}

		// ���� �� ����+���ձ�Ұ
		void Release();

		/*
		if (!tsFlags()) return; else tsFlags() = 1;
		// str.append( .......... )........
		tsFlags() = 0;
		*/
		virtual void ToString(String &str) const;

		/*
		this->BaseType::ToBBuffer(bb);
		bb.Write(.............);
		*/
		virtual void ToBBuffer(BBuffer &bb) const {}

		/*
		if (auto rtv = this->BaseType::FromBBuffer(bb)) return rtv;
		return bb.Read(.............);
		*/
		virtual int FromBBuffer(BBuffer &bb) { return 0; }

		// todo: virtual GetHashCode? Equals?

		/************************************************************************/
		// �����Ƿ���ͷ�����ݵĸ��� helpers
		/************************************************************************/

		inline MPObjectHeaderEx& mpHeader() { return *(MPObjectHeaderEx*)((char*)this - sizeof(MPObjectHeaderEx)); }
		inline MPObjectHeaderEx& mpHeader() const { return *(MPObjectHeaderEx*)((char*)this - sizeof(MPObjectHeaderEx)); }
		inline MemPool& mempool() { return *mpHeader().mempool; }
		inline uint64_t const& versionNumber() const { return mpHeader().versionNumber; }
		inline uint64_t pureVersionNumber() const { return versionNumber() & 0x00FFFFFFFFFFFFFFu; }
		inline uint32_t& refCount() { return mpHeader().refCount; }
		inline uint32_t const& refCount() const { return mpHeader().refCount; }
		inline uint16_t& tsFlags() { return mpHeader().tsFlags; }
		inline uint16_t const& tsFlags() const { return mpHeader().tsFlags; }
		inline uint16_t const& typeId() const { return mpHeader().typeId; }

		/************************************************************************/
		// �� Cout ���������
		/************************************************************************/
		template<typename...TS>
		void Cout(TS const&...ps);	// ʵ���� xxlib_cout.h
	};


	/************************************************************************/
	// �� MemPool �İ汾������ϵ�����ָ��֮ģ��������
	/************************************************************************/

	template<typename T>
	struct MPtr
	{
		T* pointer = nullptr;
		uint64_t versionNumber = 0;

		MPtr() {}
		MPtr(MPtr const &o) = default;
		MPtr& operator=(MPtr const& o) = default;
		MPtr(T* p)
			: pointer(p)
			, versionNumber(p ? RefVersionNumber(p) : 0)
		{}
		MPtr& operator=(T* p)
		{
			pointer = p;
			versionNumber = p ? RefVersionNumber(p) : 0;
			return *this;
		}

		template<typename IT>
		MPtr(MPtr<IT> const &p)
		{
			operator=(static_cast<T*>(p.Ensure()));
		}
		template<typename IT>
		MPtr& operator=(MPtr<IT> const& p)
		{
			operator=(static_cast<T*>(p.Ensure()));
		}

		bool operator==(MPtr const &o) const
		{
			return Ensure() == o.Ensure();
		}
		T* Ensure() const
		{
			if (pointer && RefVersionNumber(pointer) == versionNumber) return pointer;
			return nullptr;
		}
		operator bool() const
		{
			return Ensure() != nullptr;
		}
		// ����ֱ�����ǲ���ȫ��
		T* operator->() const
		{
			return (T*)pointer;
		}
		T& operator*()
		{
			return *(T*)pointer;
		}
		T const& operator*() const
		{
			return *(T*)pointer;
		}

	protected:
		template<typename U = MPObject>
		static uint64_t const& RefVersionNumber(std::enable_if_t<std::is_base_of<U, T>::value, U>* p)
		{
			return p->versionNumber();
		}
		template<typename U = MPObject>
		static uint64_t const& RefVersionNumber(std::enable_if_t<!std::is_base_of<U, T>::value>* p)
		{
			return ((MPObjectHeader*)p - 1)->versionNumber;
		}
	};


	/************************************************************************/
	// ������һЩ helper / ����ػ�
	/************************************************************************/

	template<typename T>
	struct IsMPtr
	{
		static const bool value = false;
	};

	template<typename T>
	struct IsMPtr<MPtr<T>>
	{
		static const bool value = true;
	};


	template<typename T>
	struct IsMPObject
	{
		static const bool value = false;
	};
	template<typename T>
	struct IsMPObject<T*>
	{
		static const bool value = !std::is_void<T>::value && std::is_base_of<MPObject, T>::value;
	};
	template<typename T>
	struct IsMPObject<MPtr<T>>
	{
		static const bool value = true;
	};


	// ɨ�����б����Ƿ��� MPObject* �� MPtr<MPObject> ����
	template<typename ...Types>
	struct ExistsMPObject
	{
		template<class Tuple, size_t N> struct TupleScaner {
			static constexpr bool Exec()
			{
				return IsMPObject< std::tuple_element_t<N - 1, Tuple> >::value ? true : TupleScaner<Tuple, N - 1>::Exec();
			}
		};
		template<class Tuple> struct TupleScaner<Tuple, 1> {
			static constexpr bool Exec()
			{
				return IsMPObject< std::tuple_element_t<0, Tuple> >::value;
			}
		};
		static constexpr bool value = TupleScaner<std::tuple<Types...>, sizeof...(Types)>::Exec();
	};



	template<typename T>
	struct MemmoveSupport<MPtr<T>>
	{
		static const bool value = true;
	};


	template<typename T>
	MPtr<T> MakeMPtr(T* p)
	{
		return MPtr<T>(p);
	}

}
