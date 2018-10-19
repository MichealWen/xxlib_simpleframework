#pragma once
#pragma warning(disable:4996)
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <type_traits>
#include <vector>
namespace xxlib
{
	/**************************************************************************************************/
	// ����ת string �õ���һЩ��������
	/**************************************************************************************************/

	// ���д��볭�� https://github.com/miloyip/itoa-benchmark/blob/master/src/branchlut.cpp
	// ������, �����˷�����䳤�ȵĹ���

	constexpr char gDigitsLut[200] = {
		'0', '0', '0', '1', '0', '2', '0', '3', '0', '4', '0', '5', '0', '6', '0', '7', '0', '8', '0', '9',
		'1', '0', '1', '1', '1', '2', '1', '3', '1', '4', '1', '5', '1', '6', '1', '7', '1', '8', '1', '9',
		'2', '0', '2', '1', '2', '2', '2', '3', '2', '4', '2', '5', '2', '6', '2', '7', '2', '8', '2', '9',
		'3', '0', '3', '1', '3', '2', '3', '3', '3', '4', '3', '5', '3', '6', '3', '7', '3', '8', '3', '9',
		'4', '0', '4', '1', '4', '2', '4', '3', '4', '4', '4', '5', '4', '6', '4', '7', '4', '8', '4', '9',
		'5', '0', '5', '1', '5', '2', '5', '3', '5', '4', '5', '5', '5', '6', '5', '7', '5', '8', '5', '9',
		'6', '0', '6', '1', '6', '2', '6', '3', '6', '4', '6', '5', '6', '6', '6', '7', '6', '8', '6', '9',
		'7', '0', '7', '1', '7', '2', '7', '3', '7', '4', '7', '5', '7', '6', '7', '7', '7', '8', '7', '9',
		'8', '0', '8', '1', '8', '2', '8', '3', '8', '4', '8', '5', '8', '6', '8', '7', '8', '8', '8', '9',
		'9', '0', '9', '1', '9', '2', '9', '3', '9', '4', '9', '5', '9', '6', '9', '7', '9', '8', '9', '9'
	};

	// Branching for different cases (forward)
	// Use lookup table of two digits

	inline uint32_t u32toa_branchlut(uint32_t value, char* buffer) {
		auto bak = buffer;											// Ϊ���㳤�ȶ���
		if (value < 10000) {
			const uint32_t d1 = (value / 100) << 1;
			const uint32_t d2 = (value % 100) << 1;

			if (value >= 1000)
				*buffer++ = gDigitsLut[d1];
			if (value >= 100)
				*buffer++ = gDigitsLut[d1 + 1];
			if (value >= 10)
				*buffer++ = gDigitsLut[d2];
			*buffer++ = gDigitsLut[d2 + 1];
		}
		else if (value < 100000000) {
			// value = bbbbcccc
			const uint32_t b = value / 10000;
			const uint32_t c = value % 10000;

			const uint32_t d1 = (b / 100) << 1;
			const uint32_t d2 = (b % 100) << 1;

			const uint32_t d3 = (c / 100) << 1;
			const uint32_t d4 = (c % 100) << 1;

			if (value >= 10000000)
				*buffer++ = gDigitsLut[d1];
			if (value >= 1000000)
				*buffer++ = gDigitsLut[d1 + 1];
			if (value >= 100000)
				*buffer++ = gDigitsLut[d2];
			*buffer++ = gDigitsLut[d2 + 1];

			*buffer++ = gDigitsLut[d3];
			*buffer++ = gDigitsLut[d3 + 1];
			*buffer++ = gDigitsLut[d4];
			*buffer++ = gDigitsLut[d4 + 1];
		}
		else {
			// value = aabbbbcccc in decimal

			const uint32_t a = value / 100000000; // 1 to 42
			value %= 100000000;

			if (a >= 10) {
				const unsigned i = a << 1;
				*buffer++ = gDigitsLut[i];
				*buffer++ = gDigitsLut[i + 1];
			}
			else
				*buffer++ = '0' + static_cast<char>(a);

			const uint32_t b = value / 10000; // 0 to 9999
			const uint32_t c = value % 10000; // 0 to 9999

			const uint32_t d1 = (b / 100) << 1;
			const uint32_t d2 = (b % 100) << 1;

			const uint32_t d3 = (c / 100) << 1;
			const uint32_t d4 = (c % 100) << 1;

			*buffer++ = gDigitsLut[d1];
			*buffer++ = gDigitsLut[d1 + 1];
			*buffer++ = gDigitsLut[d2];
			*buffer++ = gDigitsLut[d2 + 1];
			*buffer++ = gDigitsLut[d3];
			*buffer++ = gDigitsLut[d3 + 1];
			*buffer++ = gDigitsLut[d4];
			*buffer++ = gDigitsLut[d4 + 1];
		}
		//*buffer++ = '\0';
		return uint32_t(buffer - bak);							// ��������˶೤
	}

	inline uint32_t i32toa_branchlut(int32_t value, char* buffer) {
		uint32_t u = static_cast<uint32_t>(value);
		if (value < 0) {
			*buffer++ = '-';
			u = ~u + 1;
			return u32toa_branchlut(u, buffer) + 1;				// ��������˶೤
		}
		return u32toa_branchlut(u, buffer);						// ��������˶೤
	}

	inline uint32_t u64toa_branchlut(uint64_t value, char* buffer) {
		auto bak = buffer;										// Ϊ���㳤�ȶ���
		if (value < 100000000) {
			uint32_t v = static_cast<uint32_t>(value);
			if (v < 10000) {
				const uint32_t d1 = (v / 100) << 1;
				const uint32_t d2 = (v % 100) << 1;

				if (v >= 1000)
					*buffer++ = gDigitsLut[d1];
				if (v >= 100)
					*buffer++ = gDigitsLut[d1 + 1];
				if (v >= 10)
					*buffer++ = gDigitsLut[d2];
				*buffer++ = gDigitsLut[d2 + 1];
			}
			else {
				// value = bbbbcccc
				const uint32_t b = v / 10000;
				const uint32_t c = v % 10000;

				const uint32_t d1 = (b / 100) << 1;
				const uint32_t d2 = (b % 100) << 1;

				const uint32_t d3 = (c / 100) << 1;
				const uint32_t d4 = (c % 100) << 1;

				if (value >= 10000000)
					*buffer++ = gDigitsLut[d1];
				if (value >= 1000000)
					*buffer++ = gDigitsLut[d1 + 1];
				if (value >= 100000)
					*buffer++ = gDigitsLut[d2];
				*buffer++ = gDigitsLut[d2 + 1];

				*buffer++ = gDigitsLut[d3];
				*buffer++ = gDigitsLut[d3 + 1];
				*buffer++ = gDigitsLut[d4];
				*buffer++ = gDigitsLut[d4 + 1];
			}
		}
		else if (value < 10000000000000000) {
			const uint32_t v0 = static_cast<uint32_t>(value / 100000000);
			const uint32_t v1 = static_cast<uint32_t>(value % 100000000);

			const uint32_t b0 = v0 / 10000;
			const uint32_t c0 = v0 % 10000;

			const uint32_t d1 = (b0 / 100) << 1;
			const uint32_t d2 = (b0 % 100) << 1;

			const uint32_t d3 = (c0 / 100) << 1;
			const uint32_t d4 = (c0 % 100) << 1;

			const uint32_t b1 = v1 / 10000;
			const uint32_t c1 = v1 % 10000;

			const uint32_t d5 = (b1 / 100) << 1;
			const uint32_t d6 = (b1 % 100) << 1;

			const uint32_t d7 = (c1 / 100) << 1;
			const uint32_t d8 = (c1 % 100) << 1;

			if (value >= 1000000000000000)
				*buffer++ = gDigitsLut[d1];
			if (value >= 100000000000000)
				*buffer++ = gDigitsLut[d1 + 1];
			if (value >= 10000000000000)
				*buffer++ = gDigitsLut[d2];
			if (value >= 1000000000000)
				*buffer++ = gDigitsLut[d2 + 1];
			if (value >= 100000000000)
				*buffer++ = gDigitsLut[d3];
			if (value >= 10000000000)
				*buffer++ = gDigitsLut[d3 + 1];
			if (value >= 1000000000)
				*buffer++ = gDigitsLut[d4];
			if (value >= 100000000)
				*buffer++ = gDigitsLut[d4 + 1];

			*buffer++ = gDigitsLut[d5];
			*buffer++ = gDigitsLut[d5 + 1];
			*buffer++ = gDigitsLut[d6];
			*buffer++ = gDigitsLut[d6 + 1];
			*buffer++ = gDigitsLut[d7];
			*buffer++ = gDigitsLut[d7 + 1];
			*buffer++ = gDigitsLut[d8];
			*buffer++ = gDigitsLut[d8 + 1];
		}
		else {
			const uint32_t a = static_cast<uint32_t>(value / 10000000000000000); // 1 to 1844
			value %= 10000000000000000;

			if (a < 10)
				*buffer++ = '0' + static_cast<char>(a);
			else if (a < 100) {
				const uint32_t i = a << 1;
				*buffer++ = gDigitsLut[i];
				*buffer++ = gDigitsLut[i + 1];
			}
			else if (a < 1000) {
				*buffer++ = '0' + static_cast<char>(a / 100);

				const uint32_t i = (a % 100) << 1;
				*buffer++ = gDigitsLut[i];
				*buffer++ = gDigitsLut[i + 1];
			}
			else {
				const uint32_t i = (a / 100) << 1;
				const uint32_t j = (a % 100) << 1;
				*buffer++ = gDigitsLut[i];
				*buffer++ = gDigitsLut[i + 1];
				*buffer++ = gDigitsLut[j];
				*buffer++ = gDigitsLut[j + 1];
			}

			const uint32_t v0 = static_cast<uint32_t>(value / 100000000);
			const uint32_t v1 = static_cast<uint32_t>(value % 100000000);

			const uint32_t b0 = v0 / 10000;
			const uint32_t c0 = v0 % 10000;

			const uint32_t d1 = (b0 / 100) << 1;
			const uint32_t d2 = (b0 % 100) << 1;

			const uint32_t d3 = (c0 / 100) << 1;
			const uint32_t d4 = (c0 % 100) << 1;

			const uint32_t b1 = v1 / 10000;
			const uint32_t c1 = v1 % 10000;

			const uint32_t d5 = (b1 / 100) << 1;
			const uint32_t d6 = (b1 % 100) << 1;

			const uint32_t d7 = (c1 / 100) << 1;
			const uint32_t d8 = (c1 % 100) << 1;

			*buffer++ = gDigitsLut[d1];
			*buffer++ = gDigitsLut[d1 + 1];
			*buffer++ = gDigitsLut[d2];
			*buffer++ = gDigitsLut[d2 + 1];
			*buffer++ = gDigitsLut[d3];
			*buffer++ = gDigitsLut[d3 + 1];
			*buffer++ = gDigitsLut[d4];
			*buffer++ = gDigitsLut[d4 + 1];
			*buffer++ = gDigitsLut[d5];
			*buffer++ = gDigitsLut[d5 + 1];
			*buffer++ = gDigitsLut[d6];
			*buffer++ = gDigitsLut[d6 + 1];
			*buffer++ = gDigitsLut[d7];
			*buffer++ = gDigitsLut[d7 + 1];
			*buffer++ = gDigitsLut[d8];
			*buffer++ = gDigitsLut[d8 + 1];
		}

		//*buffer = '\0';
		return uint32_t(buffer - bak);							// ��������˶೤
	}

	inline uint32_t i64toa_branchlut(int64_t value, char* buffer) {
		uint64_t u = static_cast<uint64_t>(value);
		if (value < 0) {
			*buffer++ = '-';
			u = ~u + 1;
			return u64toa_branchlut(u, buffer) + 1;				// ��������˶೤
		}
		return u64toa_branchlut(u, buffer);						// ��������˶೤
	}


	/**************************************************************************************************/
	// д�볤�ȴ���Ԥ������( ������ʵ�� )
	/**************************************************************************************************/

	template<typename T> uint32_t StrCalc(T const &in);
	template<> inline uint32_t StrCalc(char        const &in) { return 1; }
	template<> inline uint32_t StrCalc(bool        const &in) { return 5; }
	template<> inline uint32_t StrCalc(int8_t      const &in) { return 4; }
	template<> inline uint32_t StrCalc(uint8_t     const &in) { return 3; }
	template<> inline uint32_t StrCalc(int16_t     const &in) { return 6; }
	template<> inline uint32_t StrCalc(uint16_t    const &in) { return 5; }
	template<> inline uint32_t StrCalc(int32_t     const &in) { return 11; }
	template<> inline uint32_t StrCalc(uint32_t    const &in) { return 10; }
	template<> inline uint32_t StrCalc(int64_t     const &in) { return 20; }
	template<> inline uint32_t StrCalc(uint64_t    const &in) { return 19; }
	template<> inline uint32_t StrCalc(float       const &in) { return 10; }
	template<> inline uint32_t StrCalc(double      const &in) { return 19; }
	template<> inline uint32_t StrCalc(void*       const &in) { return (sizeof(void*) + 1) * 2; }
	template<> inline uint32_t StrCalc(char const* const &in) { if (!in) return 0; return (uint32_t)strlen(in); }

	/**************************************************************************************************/
	// д�뺯��( ������ʵ�� )
	/**************************************************************************************************/

	template<typename T> uint32_t StrWriteTo(char *dstBuf, T const &in);
	template<> inline uint32_t StrWriteTo(char *dstBuf, char        const &in) { *dstBuf = in; return 1; }
	template<> inline uint32_t StrWriteTo(char *dstBuf, bool        const &in) { if (in) { memcpy(dstBuf, "true", 4); return 4; } else { memcpy(dstBuf, "false", 5); return 5; } }
	template<> inline uint32_t StrWriteTo(char *dstBuf, int8_t      const &in) { return i32toa_branchlut(in, dstBuf); }
	template<> inline uint32_t StrWriteTo(char *dstBuf, uint8_t     const &in) { return u32toa_branchlut(in, dstBuf); }
	template<> inline uint32_t StrWriteTo(char *dstBuf, int16_t     const &in) { return i32toa_branchlut(in, dstBuf); }
	template<> inline uint32_t StrWriteTo(char *dstBuf, uint16_t    const &in) { return u32toa_branchlut(in, dstBuf); }
	template<> inline uint32_t StrWriteTo(char *dstBuf, int32_t     const &in) { return i32toa_branchlut(in, dstBuf); }
	template<> inline uint32_t StrWriteTo(char *dstBuf, uint32_t    const &in) { return u32toa_branchlut(in, dstBuf); }
	template<> inline uint32_t StrWriteTo(char *dstBuf, int64_t     const &in) { return i64toa_branchlut(in, dstBuf); }
	template<> inline uint32_t StrWriteTo(char *dstBuf, uint64_t    const &in) { return u64toa_branchlut(in, dstBuf); }
	template<> inline uint32_t StrWriteTo(char *dstBuf, float       const &in) { return sprintf(dstBuf, "%g", in); }
	template<> inline uint32_t StrWriteTo(char *dstBuf, double      const &in) { return sprintf(dstBuf, "%g", in); }
	template<> inline uint32_t StrWriteTo(char *dstBuf, void*       const &in) { return u64toa_branchlut((size_t)in, dstBuf); }
	template<> inline uint32_t StrWriteTo(char *dstBuf, char const* const &in) { if (!in) return 0; else { auto len = (uint32_t)strlen(in); memcpy(dstBuf, in, len); return len; }; }

	/**************************************************************************************************/
	// ��ȡ����( ���� )
	/**************************************************************************************************/

	// todo: �������жϱ߽�??
	//int StrReadFrom(char const *srcBuf, uint32_t const &dataLen, uint32_t &offset, char     &out);
	//int StrReadFrom(char const *srcBuf, uint32_t const &dataLen, uint32_t &offset, bool     &out);
	//int StrReadFrom(char const *srcBuf, uint32_t const &dataLen, uint32_t &offset, int8_t   &out);
	//int StrReadFrom(char const *srcBuf, uint32_t const &dataLen, uint32_t &offset, uint8_t  &out);
	//int StrReadFrom(char const *srcBuf, uint32_t const &dataLen, uint32_t &offset, int16_t  &out);
	//int StrReadFrom(char const *srcBuf, uint32_t const &dataLen, uint32_t &offset, uint16_t &out);
	//int StrReadFrom(char const *srcBuf, uint32_t const &dataLen, uint32_t &offset, int32_t  &out);
	//int StrReadFrom(char const *srcBuf, uint32_t const &dataLen, uint32_t &offset, uint32_t &out);
	//int StrReadFrom(char const *srcBuf, uint32_t const &dataLen, uint32_t &offset, int64_t  &out);
	//int StrReadFrom(char const *srcBuf, uint32_t const &dataLen, uint32_t &offset, uint64_t &out);
	//int StrReadFrom(char const *srcBuf, uint32_t const &dataLen, uint32_t &offset, float    &out);
	//int StrReadFrom(char const *srcBuf, uint32_t const &dataLen, uint32_t &offset, double   &out);


	/**************************************************************************************************/
	// ��������֧��
	/**************************************************************************************************/

	template<uint32_t len>
	uint32_t StrCalc(char const(&in)[len])
	{
		static_assert(len, "must be a literal string");
		return len - 1;
	}
	template<uint32_t len>
	uint32_t StrWriteTo(char *dstBuf, char const(&in)[len])
	{
		static_assert(len, "must be a literal string");
		memcpy(dstBuf, in, len - 1);
		return len - 1;
	}



	/**************************************************************************************************/
	// ö��֧��
	/**************************************************************************************************/

	template<typename T>
	uint32_t StrCalc(std::enable_if_t<std::is_enum<T>::value, T> const &in)
	{
		return StrCalc((std::underlying_type_t<T> const&)in);
	}
	template<typename T>
	uint32_t StrWriteTo(char *dstBuf, std::enable_if_t<std::is_enum<T>::value, T> const &in)
	{
		return StrWriteTo(dstBuf, (std::underlying_type_t<T> const&)in);
	}
	//template<typename T>
	//int StrReadFrom(char const *srcBuf, uint32_t const &dataLen, uint32_t &offset, std::enable_if_t<std::is_enum<T>::value, T> &out)
	//{
	//	return StrReadFrom(srcBuf, dataLen, offset, (std::underlying_type_t<T>&)out);
	//}


	/**************************************************************************************************/
	// ���ģ��֧��
	/**************************************************************************************************/

	template<typename T, typename...TS>
	uint32_t StrCalc(T const &in, TS const& ... ins)
	{
		return StrCalc(in) + StrCalc(ins...);
	}
	template<typename T, typename...TS>
	uint32_t StrWriteTo(char *dstBuf, T const &in, TS const& ... ins)
	{
		uint32_t offset = StrWriteTo(dstBuf, in);
		return offset + StrWriteTo(dstBuf + offset, ins...);
	}
	//template<typename T, typename...TS>
	//int StrReadFrom(char const *srcBuf, uint32_t const &dataLen, uint32_t &offset, T &out, TS &...outs)
	//{
	//	auto rtv = StrReadFrom(srcBuf, dataLen, offset, out);
	//	if (rtv) return rtv;
	//	return StrReadFrom(srcBuf, dataLen, offset, outs...);
	//}






	/**************************************************************************************************/
	// ��������֧��
	/**************************************************************************************************/

	// std array 
	template<typename T, size_t len>
	uint32_t StrCalc(std::array<T, len> const&in)
	{
		uint32_t rtv = 0;
		for (auto& o : in) rtv += StrCalc(o);
		return rtv;
	}

	template<typename T, size_t len>
	uint32_t StrWriteTo(char *dstBuf, std::array<T, len> const&in)
	{
		uint32_t offset = 0;
		offset += StrWriteTo(dstBuf + offset, "{ ");
		for (auto& o : in) offset += StrWriteTo(dstBuf + offset, o, ", ");
		offset += StrWriteTo(dstBuf + offset - 2, " }");
		return offset;
	}

	// ��ͳ����
	template<typename T, size_t len>
	uint32_t StrCalc(T const(&in)[len])
	{
		uint32_t rtv = 0;
		for (auto& o : in) rtv += StrCalc(o);
		return rtv;
	}

	template<typename T, size_t len>
	uint32_t StrWriteTo(char *dstBuf, T const(&in)[len])
	{
		uint32_t offset = 0;
		offset += StrWriteTo(dstBuf + offset, "{ ");
		for (auto& o : in) offset += StrWriteTo(dstBuf + offset, o, ", ");
		offset += StrWriteTo(dstBuf + offset - 2, " }");
		return offset;
	}





	/**************************************************************************************************/
	// c-style char* תΪ������ֵ
	/**************************************************************************************************/

	// c-style char* תΪ���ֳ��ȵ� �з�������. Out ȡֵ��Χ�� int8~64
	template <typename OutType>
	void ToInt(char const * in, OutType & out)
	{
		auto in_ = in;
		if (*in_ == '0')
		{
			out = 0;
			return;
		}
		bool b;
		if (*in_ == '-')
		{
			b = true;
			++in_;
		}
		else b = false;
		OutType r = *in_ - '0';
		char c;
		while ((c = *(++in_))) r = r * 10 + (c - '0');
		out = b ? -r : r;
	}

	// c-style char* (�����м��Ŵ�ͷ) תΪ���ֳ��ȵ� �޷�������. Out ȡֵ��Χ�� uint8, uint16, uint32, uint64
	template <typename OutType>
	void ToUInt(char const * in, OutType & out)
	{
		assert(in);
		auto in_ = in;
		if (*in_ == '0')
		{
			out = 0;
			return;
		}
		OutType r = *(in_)-'0';
		char c;
		while ((c = *(++in_))) r = r * 10 + (c - '0');
		out = r;
	}

	inline void FromString(bool     &out, char const *in) { out = (in[0] == '1' || in[0] == 'T' || in[0] == 't'); }
	inline void FromString(uint8_t  &out, char const *in) { ToUInt(in, out); }
	inline void FromString(uint16_t &out, char const *in) { ToUInt(in, out); }
	inline void FromString(uint32_t &out, char const *in) { ToUInt(in, out); }
	inline void FromString(uint64_t &out, char const *in) { ToUInt(in, out); }
	inline void FromString(int8_t   &out, char const *in) { ToInt(in, out); }
	inline void FromString(int16_t  &out, char const *in) { ToInt(in, out); }
	inline void FromString(int32_t  &out, char const *in) { ToInt(in, out); }
	inline void FromString(int64_t  &out, char const *in) { ToInt(in, out); }
	inline void FromString(double   &out, char const *in) { out = strtod(in, nullptr); }
	inline void FromString(float    &out, char const *in) { out = (float)strtod(in, nullptr); }




	/**************************************************************************************************/
	// Utf8 תΪ char16_t* ����
	/**************************************************************************************************/

	struct Utf8Converter
	{
		std::vector<std::u16string> bufs;
		uint32_t idx = 0;
		Utf8Converter(uint32_t bufCount = 16)			// ��������ڶ��������ʱ�����ظ�ʹ�õ�һ�� buf
		{
			assert(bufCount);
			bufs.resize(bufCount);
		}
		inline char16_t const* Convert(char const * utf8Str)
		{
			assert(utf8Str);
			auto& buf = bufs[idx++];
			if (idx == bufs.size()) idx = 0;

			auto len = strlen(utf8Str);
			buf.resize(strlen(utf8Str));

			uint32_t i = 0, j = 0;
			while (uint8_t c = utf8Str[i++])
			{
				if (c < 0x80u)
				{
					buf[j++] = c;
				}
				else if ((c & 0xC0u) == 0xC0u)
				{
					buf[j++] = ((c & 0x1F) << 6) | (utf8Str[i++] & 0x3F);
				}
				else if ((c & 0xE0) == 0xE0)
				{
					buf[j++] = ((c & 0x0F) << 12) | (((utf8Str[i]) & 0x3F) << 6) | ((utf8Str[i + 1]) & 0x3F);
					i += 2;
				}
			};
			buf.resize(j);
			return buf.c_str();
		}
	};

}
