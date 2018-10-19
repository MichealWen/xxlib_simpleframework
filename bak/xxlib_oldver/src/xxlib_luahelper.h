#pragma once
#include <cassert>
#include <tuple>
#include <type_traits>
#include <utility>
#include <array>
#include <initializer_list>
#include "xxlib_mpobject.h"
#include "lua.hpp"

namespace xxlib
{
	// todo: �Ľ���, Ĭ����·���ഫ��ȫ���� luaBinder. �����������������������õ� tuple. ����� LuaTypeIndex ������ type �� index

	//typedef typename decltype(luaBinder)::Tuple LuaBinderTuple;

	//template<typename T>
	//using LuaTypeIndex = xxlib::TupleIndexOf<T, LuaBinderTuple>;


	/*

	struct A {};
	struct B :A {};
	struct C :B {};
	struct D {};

	xxlib::LuaBinder<A, B, C, D> luaBinder;

	*/

	template<typename ... Types>
	struct LuaBinder
	{
		typedef std::tuple<MPObject, Types...> Tuple;
		std::array<uint16_t, 1 + sizeof...(Types)> pids;	// 0: MPObject

		LuaBinder()
		{
			Fill(std::make_index_sequence<std::tuple_size<Tuple>::value>());
		}

		// ���� ������� �жϸ��ӹ�ϵ
		bool IsBaseOf(uint16_t baseTypeId, uint16_t typeId)
		{
			assert(baseTypeId < std::tuple_size<Tuple>::value && typeId < std::tuple_size<Tuple>::value);
			for (; typeId != baseTypeId; typeId = pids[typeId])
			{
				if (!typeId) return false;
			}
			return true;
		}

		// ���� ���� �жϸ��ӹ�ϵ
		template<typename BT, typename T>
		bool IsBaseOf()
		{
			return IsBaseOf(TupleIndexOf<BT, Tuple>::value, TupleIndexOf<T, Tuple>::value);
		}

		// ��������������Ԫ����õ� L( һֱռ�� stack �ռ� )
		void CreateMetatables(lua_State* L)
		{
			assert(0 == lua_gettop(L));
			for (int i = 0; i < std::tuple_size<Tuple>::value; ++i)
			{
				CreateMetatable(L, i);
			}
		}

		// ���� T*, SceneObjRef<T> ����Ҫ TryTo �� ��ʵ self ʱ, ��������������Ϸ���.
		template<typename T>
		inline bool LuaCheck(lua_State* L, int idx)
		{
			static_assert(TupleIndexOf<T, Tuple>::value, "T is not in Types.");
			if (!lua_getmetatable(L, idx)) return false;	// ... mt?
			bool b = false;
			if (lua_rawgeti(L, -1, 1) == LUA_TNUMBER)		// ... mt, tidx?
			{
				b = IsBaseOf((uint16_t)lua_tointeger(L, -1), TupleIndexOf<T, Tuple>::value);
			}
			lua_pop(L, 2);									// ...
			return b;
		}

	protected:
		void CreateMetatable(lua_State* L, int idx)
		{
			lua_newtable(L);									// t

																// д����������
			lua_pushinteger(L, idx);							// t, idx
			lua_rawseti(L, -2, 1);								// t

																// ��� __index
			lua_pushstring(L, "__index");						// t, k
			lua_pushvalue(L, -2);								// t, k, t
			lua_rawset(L, -3);									// t
		}

		template<size_t... Indexs, typename T>
		void Fill(std::index_sequence<Indexs...> const& idxs, T* const& ct)
		{
			uint16_t pid = 0;
			std::initializer_list<int>{ ((
				std::is_base_of<std::tuple_element_t<Indexs, Tuple>, T>::value && !std::is_same<std::tuple_element_t<Indexs, Tuple>, T>::value
				? (pid = TupleIndexOf<std::tuple_element_t<Indexs, Tuple>, Tuple>::value) : 0
				), 0)... };
			pids[TupleIndexOf<T, Tuple>::value] = pid;
		}

		template<size_t... Indexs>
		void Fill(std::index_sequence<Indexs...> const& idxs)
		{
			std::initializer_list<int>{ (Fill(idxs, (std::tuple_element_t<Indexs, Tuple>*)nullptr), 0)... };
		}

		//// �ж� Ŀ������ �Ƿ�ӻ���ָ������( ĳ����ָ���Ƿ���ת����Ŀ������ ) ��ȡ�� dynamic_cast
		//template<typename T>
		//bool IsInheritOf(MPObject* base)
		//{
		//	return IsBaseOf(base->typeId(), TupleIndexOf<T, Tuple>::value);
		//}
	};

	/*****************s*******************************************************************/
	// Lua_Container / Lua_CreatContainer
	/************************************************************************************/

	// �ڴ�����ȫ��������
	constexpr auto Lua_Container = "objs";

	// ����ȫ������
	inline void Lua_CreatContainer(lua_State* L)
	{
		lua_newtable(L);						// t
		lua_setglobal(L, Lua_Container);		//
	}


	/************************************************************************************/
	// Lua_Metatable
	/************************************************************************************/

	// ��������ģ��
	// �������������Ԫ��� �±� ��ӳ���ϵ
	template<typename T>
	struct Lua_Metatable;

	/************************************************************************************/
	// LuaCheck
	/************************************************************************************/

	// ���� T, T*, SceneObjRef<T> ����Ҫ TryTo �� ��ʵ self ʱ, ��������������Ϸ���.
	// Ҫ�� ud/lud �߱� mt, �� mt[1] Ҫ���� Lua_Metatable<T>::index
	template<typename T>
	inline bool LuaCheck(lua_State* L, int idx)
	{
		auto rtv = lua_getmetatable(L, idx);			// ... mt?
		if (!rtv) return false;
		// todo: ��� mt �Ƿ�Ϊ T ���� Parent �� mt
		lua_pop(L, 1);
		return true;
	}


	/************************************************************************************/
	// LuaError
	/************************************************************************************/

	// �򻯳���������. �÷�: return LuaError("...");
	inline int LuaError(lua_State* L, char const* errmsg)
	{
		lua_pushstring(L, errmsg);
		lua_error(L);
		return 0;
	}

	/************************************************************************************/
	// LuaSelf<T>::Get
	/************************************************************************************/

	// �� self תΪ T* ����̬����. Ϊ�����ʾת��ʧ�ܻ�ָ���Ұ
	// ���� lua_islightuserdata ��� is_base_of<MPObject, T> ���жϵ������������
	template<typename T, typename ENABLE = void>
	struct LuaSelf;

	template<typename T>
	struct LuaSelf<T, std::enable_if_t< std::is_base_of<MPObject, T>::value>>
	{
		static T* Get(lua_State* L)
		{
			if (lua_islightuserdata(L, 1))
			{
				return (T*)lua_touserdata(L, 1);
			}
			auto& ptr = *(SceneObjRef<T>*)lua_touserdata(L, 1);
			if (ptr) return ptr.pointer;
			return nullptr;
		}
	};
	template<typename T>
	struct LuaSelf<T, std::enable_if_t<!std::is_base_of<MPObject, T>::value>>
	{
		static T* Get(lua_State* L)
		{
			return (T*)lua_touserdata(L, 1);
		}
	};


	/************************************************************************************/
	// Lua<T, cond>
	/************************************************************************************/

	// ��װ��һ���������������·�ɵ� Push / To ����


	template<typename T, typename ENABLE = void>
	struct Lua
	{
		static void PushUp_Mt(lua_State* L);
		static void Push(lua_State* L, std::string const& v);
		static void To(lua_State* L, std::string& v, int idx);
		static bool TryTo(lua_State* L, std::string& v, int idx);
	};

	// void
	template<>
	struct Lua<void, void>
	{
		static inline void PushUp_Mt(lua_State* L)
		{
			lua_pushnil(L);
		}
	};

	// string
	template<>
	struct Lua<std::string, void>
	{
		static inline void PushUp_Mt(lua_State* L)
		{
			lua_pushnil(L);
		}

		static inline void Push(lua_State* L, std::string const& v)
		{
			lua_pushlstring(L, v.data(), v.size());
		}
		static inline void To(lua_State* L, std::string& v, int idx)
		{
			size_t len;
			auto s = lua_tolstring(L, idx, &len);
			v.assign(s, len);
		}
		static inline bool TryTo(lua_State* L, std::string& v, int idx)
		{
			if (!lua_isstring(L, idx)) return false;
			To(L, v, idx);
			return true;
		}
	};

	// char*
	template<>
	struct Lua<char const*, void>
	{
		static inline void PushUp_Mt(lua_State* L)
		{
			lua_pushnil(L);
		}

		static inline void Push(lua_State* L, char const* const& v)
		{
			lua_pushstring(L, v);
		}
		static inline void To(lua_State* L, char const*& v, int idx)
		{
			v = lua_tostring(L, idx);
		}
		static inline bool TryTo(lua_State* L, char const*& v, int idx)
		{
			if (!lua_isstring(L, idx)) return false;
			v = lua_tostring(L, idx);
			return true;
		}
	};

	// fp
	template<typename T>
	struct Lua<T, std::enable_if_t<std::is_floating_point<T>::value>>
	{
		static inline void PushUp_Mt(lua_State* L)
		{
			lua_pushnil(L);
		}

		static inline void Push(lua_State* L, T const& v)
		{
			lua_pushnumber(L, (lua_Number)v);
		}
		static inline void To(lua_State* L, T& v, int idx)
		{
			v = (T)lua_tonumber(L, idx);
		}
		static inline bool TryTo(lua_State* L, T& v, int idx)
		{
			if (!lua_isnumber(L, idx)) return false;
			v = (T)lua_tonumber(L, idx);
			return true;
		}
	};

	// int
	template<typename T>
	struct Lua<T, std::enable_if_t<std::is_integral<T>::value || std::is_enum<T>::value>>
	{
		static inline void PushUp_Mt(lua_State* L)
		{
			lua_pushnil(L);
		}

		static inline void Push(lua_State* L, T const& v)
		{
			lua_pushinteger(L, (lua_Integer)v);
		}
		static inline void To(lua_State* L, T& v, int idx)
		{
			v = (T)lua_tointeger(L, idx);
		}
		static inline bool TryTo(lua_State* L, T& v, int idx)
		{
			if (!lua_isinteger(L, idx)) return false;
			v = (T)lua_tointeger(L, idx);
			return true;
		}
	};

	// T*
	template<typename T>
	struct Lua<T, std::enable_if_t<std::is_pointer<T>::value>>
	{
		typedef std::remove_pointer_t<T> TT;

		static inline void PushUp_Mt(lua_State* L)
		{
			lua_pushvalue(L, Lua_Metatable<TT>::index);
		}

		static inline void Push(lua_State* L, T const& v)
		{
			lua_pushlightuserdata(L, v);								// lud
			lua_getupvalue(L, -lua_gettop(L) - 1, 1);					// lud, mt
			lua_setmetatable(L, -2);									// lud
		}
		static inline void To(lua_State* L, T& v, int idx)
		{
			v = (T)lua_touserdata(L, idx);
		}
		static inline bool TryTo(lua_State* L, T& v, int idx)
		{
			if (!lua_islightuserdata(L, idx)) return false;
			if (!LuaCheck<TT>(L, idx)) return false;
			v = (T)lua_touserdata(L, idx);
			return true;
		}
	};

	// T
	template<typename T>
	struct Lua<T, std::enable_if_t<std::is_class<T>::value && !IsMPObject<T>::value>>
	{
		static inline void PushUp_Mt(lua_State* L)
		{
			lua_pushvalue(L, Lua_Metatable<T>::index);
		}

		static inline void Push(lua_State* L, T const& v)
		{
			auto p = (T*)lua_newuserdata(L, sizeof(v));					// ud
			(*p) = v;
			lua_getupvalue(L, -lua_gettop(L) - 1, 1);					// ud, mt
			lua_setmetatable(L, -2);									// ud
		}
		static inline void To(lua_State* L, T& v, int idx)
		{
			v = *(T*)lua_touserdata(L, idx);
		}
		static inline bool TryTo(lua_State* L, T& v, int idx)
		{
			if (!lua_isuserdata(L, idx)) return false;
			if (!LuaCheck<T>(L, idx)) return false;
			v = *(T*)lua_touserdata(L, idx);
			return true;
		}
	};

	// SceneObjRef<T>
	template<typename T>
	struct Lua<T, std::enable_if_t<std::is_class<T>::value && IsMPObject<T>::value>>
	{
		typedef typename T::type TT;

		static inline void PushUp_Mt(lua_State* L)
		{
			lua_pushvalue(L, Lua_Metatable<TT>::index);
		}

		static inline void Push(lua_State* L, T const& v)
		{
			auto p = (T*)lua_newuserdata(L, sizeof(v));					// ud
			(*p) = v;
			lua_getupvalue(L, -lua_gettop(L) - 1, 1);					// ud, mt
			lua_setmetatable(L, -2);									// ud
		}
		static inline void To(lua_State* L, T& v, int idx)
		{
			v = *(T*)lua_touserdata(L, idx);
		}
		static inline bool TryTo(lua_State* L, T& v, int idx)
		{
			if (!lua_isuserdata(L, idx)) return false;
			if (!LuaCheck<TT>(L, idx)) return false;
			v = *(T*)lua_touserdata(L, idx);
			return true;
		}
	};

	// todo: more ���� 
	// todo: std::vector<T>
	// todo: std::vector<T*>
	// todo: std::vector<SceneObjRef<T>>


	/************************************************************************************/
	// Lua_PushMetatable, Lua_CopyItemsFromParentMetatable
	/************************************************************************************/

	// ����һ������ metatable ��ջ. __index ִ�з�ʽΪ���, ��Ԫ��ָ������
	template<typename T>
	inline void Lua_PushMetatable(lua_State* L, int numFieldsCapacity, int numMethodsCapacity)
	{
		lua_createtable(L, numFieldsCapacity, numMethodsCapacity);	// t
		assert(xxlib::Lua_Metatable<T>::index == lua_gettop(L));
		// д��λ�� L �е��±�
		auto top = lua_gettop(L);
		lua_pushinteger(L, top);							// t, idx
		lua_rawseti(L, -2, 1);								// t
															// ��� __index
		lua_pushstring(L, "__index");						// t, k
		lua_pushvalue(L, -2);								// t, k, t
		lua_rawset(L, -3);									// t
	}




	// ��ָ�� index �� clone Ԫ�ص� -1 ��
	template<typename Parent>
	inline void Lua_CopyItemsFromParentMetatable(lua_State* L)
	{
		lua_pushnil(L);
		while (lua_next(L, Lua_Metatable<Parent>::index) != 0)
		{
			lua_pushvalue(L, -2);
			lua_insert(L, -2);
			lua_rawset(L, -4);
		}
	}


	/************************************************************************************/
	// Lua_CreateCoroutine
	/************************************************************************************/

	// Ϊ״̬��( ������ MPObject )��Э�̷��� LUA_REGISTRYINDEX ������ָ��
	// ��Ҫ��״̬�����캯��������. o ���� this
	inline lua_State* Lua_CreateCoroutine(lua_State* L, void* key)
	{
		auto co = lua_newthread(L);							// key, co
		lua_rawsetp(L, LUA_REGISTRYINDEX, key);				//
		return co;
	}


	/************************************************************************************/
	// Lua_ReleaseCoroutine
	/************************************************************************************/

	// �Ƴ�һ��Э�� ( λ�� LUA_REGISTRYINDEX �� )
	inline void Lua_ReleaseCoroutine(lua_State* L, void* key)
	{
		if (!L) return;										// ����� scene ����( L ���� )�ͻᵼ��������
		lua_pushnil(L);
		lua_rawsetp(L, LUA_REGISTRYINDEX, key);
	}

	/************************************************************************************/
	// Lua_CreateSceneObjRef
	/************************************************************************************/

	// todo: �� co ֱ�Ӵ�ֵ( ��ǰ�����൱���ǻᴴ����2��ʵ���� )

	// Ϊ MPObject �� SceneObjRef<T> userdata ����ȫ������( key Ϊ pureVersionNumber )
	// ��Ҫ������ LUA Э��״̬�� �� MPObject ֮���캯��������. o ���� this
	template<typename T>
	inline void Lua_CreateSceneObjRef(lua_State* L, T* o)
	{
		static_assert(std::is_base_of<MPObject, T>::value, "the T must be inherit of MPObject");
		auto rtv = lua_getglobal(L, Lua_Container);			// objs
		assert(rtv == LUA_TTABLE);
		auto p = lua_newuserdata(L, sizeof(SceneObjRef<T>));// objs, ud
		new (p) SceneObjRef<T>(o);
		lua_pushvalue(L, Lua_Metatable<T>::index);			// objs, ud, mt
		lua_setmetatable(L, -2);
		lua_rawseti(L, -2, o->pureVersionNumber());			// objs
		lua_pop(L, 1);										//
	}


	/************************************************************************************/
	// Lua_ReleaseSceneObjRef
	/************************************************************************************/

	// ��ȫ������ɱЭ��
	// ��Ҫ��״̬����������������. o ���� this
	inline void Lua_ReleaseSceneObjRef(lua_State* L, MPObject* o)
	{
		if (!L) return;										// ����� scene ����( L ���� )�ͻᵼ��������
		auto rtv = lua_getglobal(L, Lua_Container);			// objs
		assert(rtv == LUA_TTABLE);
		lua_pushnil(L);										// objs, nil
		lua_rawseti(L, -2, o->pureVersionNumber());			// objs
		lua_pop(L, 1);										//
	}


	/************************************************************************************/
	// Lua_CreatePointerToGlobal
	/************************************************************************************/

	// Ϊָ�����ʹ��� lightuserdata ���ŵ�ȫ�� _G( ��Ҫ����ȫ�ֵ����� SceneManager ֮�� )
	template<typename T>
	inline void Lua_CreatePointerToGlobal(lua_State* L, T* o, char const* name)
	{
		lua_pushlightuserdata(L, o);						// lud
		lua_pushvalue(L, Lua_Metatable<T>::index);			// lud, mt
		lua_setmetatable(L, -2);							// lud
		lua_setglobal(L, name);
	}


	/************************************************************************************/
	// Lua_Resume
	/************************************************************************************/

	inline int Lua_Resume(lua_State* co, std::string& err)
	{
		assert(co);
		int status = lua_resume(co, nullptr, 0);
		if (status == LUA_YIELD)
		{
			return 0;	// todo: �ݴ溯���ķ���ֵ? 
		}
		else if (status == LUA_ERRRUN && lua_isstring(co, -1))
		{
			err = lua_tostring(co, -1);
			lua_pop(co, -1);
			return -1;
		}
		else
		{
			return 1;	// LUA_OK
		}
	}




	/************************************************************************************/
	// Lua_TupleFiller
	/************************************************************************************/

	template<typename Tuple, std::size_t N>
	struct Lua_TupleFiller
	{
		static bool Fill(lua_State* L, Tuple& t)
		{
			auto rtv = Lua<std::tuple_element_t<N - 1, Tuple>>::TryTo(L, std::get<N - 1>(t), -(int)(std::tuple_size<Tuple>::value - N + 1));
			if (!rtv) return false;
			return Lua_TupleFiller<Tuple, N - 1>::Fill(L, t);
		}
	};
	template<typename Tuple>
	struct Lua_TupleFiller < Tuple, 1 >
	{
		static bool Fill(lua_State* L, Tuple& t)
		{
			return Lua<std::tuple_element_t<0, Tuple>>::TryTo(L, std::get<0>(t), -(int)(std::tuple_size<Tuple>::value));
		}
	};

	template<typename Tuple>
	bool Lua_FillTuple(lua_State* L, Tuple& t)
	{
		return Lua_TupleFiller<Tuple, std::tuple_size<Tuple>::value>::Fill(L, t);
	}

	/************************************************************************************/
	// Lua_CallFunc
	/************************************************************************************/

	// todo: linux �²���ͨ��

	// ��֪����: ���Ϊ YIELD ��ʽִ�еĺ���, ������ֱ�ӷ���ֵ.
	// �в��� �з���ֵ
	template<bool YIELD, typename T, typename R, typename ...Args>
	int Lua_CallFunc(std::enable_if_t<sizeof...(Args) && !std::is_void<R>::value, lua_State*> L, T* o, R(T::* f)(Args...))
	{
		std::tuple<Args...> t;
		if (Lua_FillTuple(L, t))
		{
			auto rtv = FuncTupleCaller(o, f, t, std::make_index_sequence<sizeof...(Args)>());
			if (YIELD) return lua_yield(L, 0);
			Lua<R>::Push(L, rtv);
			return 1;
		}
		return 0;
	}
	// �޲��� �з���ֵ
	template<bool YIELD, typename T, typename R, typename ...Args>
	int Lua_CallFunc(std::enable_if_t<!sizeof...(Args) && !std::is_void<R>::value, lua_State*> L, T* o, R(T::* f)(Args...))
	{
		auto rtv = (o->*f)();
		if (YIELD) return lua_yield(L, 0);
		Lua<R>::Push(L, rtv);
		return 1;
	}
	// �в��� �޷���ֵ
	template<bool YIELD, typename T, typename R, typename ...Args>
	int Lua_CallFunc(std::enable_if_t<sizeof...(Args) && std::is_void<R>::value, lua_State*> L, T* o, R(T::* f)(Args...))
	{
		std::tuple<Args...> t;
		if (Lua_FillTuple(L, t))
		{
			FuncTupleCaller(o, f, t, std::make_index_sequence<sizeof...(Args)>());
			if (YIELD) return lua_yield(L, 0);
			else return 0;
		}
		return 0;
	}
	// �޲��� �޷���ֵ
	template<bool YIELD, typename T, typename R, typename ...Args>
	int Lua_CallFunc(std::enable_if_t<!sizeof...(Args) && std::is_void<R>::value, lua_State*> L, T* o, R(T::* f)(Args...))
	{
		(o->*f)();
		if (YIELD) return lua_yield(L, 0);
		else return 0;
	}


	/************************************************************************************/
	// Lua_BindFunc_Ensure
	/************************************************************************************/

	template<typename T>
	int Lua_BindFunc_Ensure_Impl(lua_State* L)
	{
		auto tn = typeid(T).name();
		auto top = lua_gettop(L);
		if (top != 1 || !LuaCheck<T>(L, 1))
		{
			lua_pushliteral(L, "error!!! func args num wrong.");
			lua_error(L);
			return 0;
		}
		T* self = LuaSelf<T>::Get(L);
		lua_pushboolean(L, self ? true : false);
		return 1;
	}

	// ����ȷ�� ud �Ƿ�Ϸ��� Ensure ָ��
	template<typename T>
	inline void Lua_BindFunc_Ensure(lua_State* L)
	{
		static_assert(std::is_base_of<MPObject, T>::value, "only MPObject* struct have Ensure func.");
		lua_pushstring(L, "Ensure");
		lua_pushcfunction(L, Lua_BindFunc_Ensure_Impl<T>);
		lua_rawset(L, -3);
	}

}



/************************************************************************************/
// xxLua_BindXXXXXXXXX
/************************************************************************************/

#define xxLua_BindField(LUA, T, F, writeable)									\
lua_pushstring(LUA, #F);														\
xxlib::Lua<decltype(xxlib::GetFieldType(&T::F))>::PushUp_Mt(LUA);				\
lua_pushcclosure(LUA, [](lua_State* L)											\
{																				\
	auto top = lua_gettop(L);													\
	if (!top || !xxlib::LuaCheck<T>(L, 1))										\
	{																			\
		return xxlib::LuaError(L, "error!!! : ? bad self type?");				\
	}																			\
	T* self = xxlib::LuaSelf<T>::Get(L);										\
	if (!self)																	\
	{																			\
		return xxlib::LuaError(L, "error!!! self is nil!");						\
	}																			\
	if (top == 1)																\
	{																			\
		xxlib::Lua<decltype(self->F)>::Push(L, self->F);				 		\
		return 1;																\
	}																			\
	if (top == 2)																\
	{																			\
		if (!writeable)															\
		{																		\
			return xxlib::LuaError(L, "error!!! readonly!");					\
		}																		\
		else if (!xxlib::Lua<decltype(self->F)>::TryTo(L, self->F, 2))			\
		{																		\
			return xxlib::LuaError(L, "error!!! bad data type!");				\
		}																		\
		return 0;																\
	}																			\
	return xxlib::LuaError(L, "error!!! too many args!");						\
}, 1);																			\
lua_rawset(LUA, -3);


#define xxLua_BindFunc(LUA, T, F, YIELD)										\
lua_pushstring(LUA, #F);														\
xxlib::Lua<decltype(xxlib::GetFuncReturnType(&T::F))>::PushUp_Mt(LUA);			\
lua_pushcclosure(LUA, [](lua_State* L)											\
{																				\
	auto top = lua_gettop(L);													\
	auto numArgs = xxlib::GetFuncArgsCount(&T::F);								\
	if (top != numArgs + 1 || !xxlib::LuaCheck<T>(L, 1))						\
	{																			\
		return xxlib::LuaError(L, "error!!! wrong num args or bad self type.");	\
	}																			\
	T* self = xxlib::LuaSelf<T>::Get(L);										\
	if (!self)																	\
	{																			\
		return xxlib::LuaError(L, "error!!! self is nil!");						\
	}																			\
	return xxlib::Lua_CallFunc<YIELD>(L, self, &T::F);							\
}, 1);																			\
lua_rawset(LUA, -3);

