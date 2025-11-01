#pragma once
#include <stdlib.h>
#include <string>
#include <iostream>

constexpr size_t MAX_STRING_LENGTH = 2048;

const short emptyChar = 0;

// Returns the length in bytes, not characters
template<typename T>
static size_t WStringLength(T* str)
{
	T* ptr = str;
	while (*ptr++);

	return (ptr - str) * sizeof(T);
}

template<typename T>
static bool WStringStartsWith(T* ptr1, T* ptr2)
{
	while (*ptr2)
	{
		if (*ptr1 != *ptr2)
			return false;

		ptr1++;
		ptr2++;
	}

	return true;
}

template<typename T>
static bool WStringCompare(T* ptr1, T* ptr2)
{
	while (*ptr2)
	{
		if (*ptr1++ != *ptr2++)
			return false;
	}

	return !*ptr1;
}

template<typename T>
class zstring
{
private:
	T* buffer;

	// length of the string in bytes
	size_t length;

	// number of characters in the string
	size_t count;

	long long WrapIndex(long long index) const
	{
		if (index < 0)
			return count - index;

		return index;
	}

public:
	zstring(const T* string, ...);

	~zstring()
	{
		if (buffer && buffer != Empty)
			free(buffer);
	}

	static inline const T* Empty = (T*)&emptyChar;

	operator T*& () { return this->buffer; }
	//operator std::string() { return this->buffer; }
	T& operator[] (long long index) { return this->buffer[WrapIndex(index)]; }

	zstring<T>* operator+(zstring<T>& s1)
	{
		auto newBuffer = (T*)malloc(this->length + s1.length);

		memcpy(newBuffer, this->buffer, this->length);
		memcpy(newBuffer + this->length, s1.buffer, s1.length);

		auto newString = new zstring(newBuffer);
		free(newBuffer);

		return newString;
	}

	zstring<T>* operator+(T* s1)
	{
		size_t s1Length = WStringLength(s1);

		auto newBuffer = (T*)malloc(length + s1Length);

		memcpy(newBuffer, buffer, length - 1);
		memcpy(newBuffer + length - 1, s1, s1Length);

		auto newString = new zstring(newBuffer);
		free(newBuffer);

		return newString;
	}

	template<typename Y>
	zstring& operator+=(Y s1)
	{
		T* ptr = (T*)s1;
		size_t len = WStringLength(s1);

		buffer = (T*)realloc(buffer, length + len - 1);
		memcpy(buffer + length - 1, (T*)s1, len);

		return *this;
	}

	bool operator==(zstring& s1) { return Equals(s1); }
	bool operator==(T* s1) { return Equals(s1); }
	bool operator==(const T* s1) { return Equals((T*)s1); }

	bool operator>(zstring& s1) { return length > s1.length; }
	bool operator>(T* s1) { return length > WStringLength(s1); }

	bool operator>=(zstring& s1) { return length >= s1.length; }
	bool operator>=(T* s1) { return length >= WStringLength(s1); }

	bool operator<(zstring& s1) { return length < s1.length; }
	bool operator<(T* s1) { return length < WStringLength(s1); }

	bool operator<=(zstring& s1) { return length <= s1.length; }
	bool operator<=(T* s1) { return length <= WStringLength(s1); }

	inline T* Ptr()
	{
		return buffer;
	}

	zstring* operator <<(size_t amount)
	{
		auto newString = new zstring(*this);

		newString->length -= amount * sizeof(T);
		newString->count -= amount;

		for (size_t i = 0; i < newString->count; i++)
			(*newString)[i] = (*newString)[i + amount];

		newString->buffer = (T*)realloc(newString->buffer, newString->length);

		return newString;
	}

	zstring& operator <<=(size_t amount)
	{
		length -= amount * sizeof(T);
		count -= amount;
		for (size_t i = 0; i < count; i++)
		{
			(*this)[i] = (*this)[i + amount];
		}

		buffer = (T*)realloc(buffer, length);

		return *this;
	}

	// mallocs a new buffer, so it'll have to be free'd at some point
	T* CopyToBuffer()
	{
		auto newBuffer = (T*)malloc(length);
		memcpy(newBuffer, buffer, length);
		return newBuffer;
	}

	void CopyToBuffer(wchar_t* buf, size_t numCharacters)
	{
		memcpy(buf, buffer, numCharacters < count ? numCharacters : count);
	}

	void Replace(zstring& subject, zstring& replacement)
	{
		Replace((T*)subject, (T*)replacement);
	}

	void Replace(const T* subject, const T* replacement)
	{
		Replace((T*)subject, (T*)replacement);
	}

	void Replace(T* subject, T* replacement)
	{
		size_t subjectLength = WStringLength(subject);
		size_t replacementLength = WStringLength(replacement);

		T* ptr = begin();
		T* nd = end();

		auto newBuffer = (T*)malloc(length);
		T* writePtr = newBuffer;
		size_t newLength = length;

		while (ptr < nd)
		{
			if (WStringStartsWith(ptr, subject))
			{
				newLength -= subjectLength;
				newLength += replacementLength;
				writePtr -= (size_t)newBuffer;
				newBuffer = (T*)realloc(newBuffer, newLength);
				writePtr += (size_t)newBuffer;
				memcpy(writePtr, replacement, replacementLength);
				writePtr += replacementLength;
			}
			else
				*writePtr++ = *ptr;

			ptr++;
		}

		free(buffer);

		buffer = newBuffer;
		length = newLength;
		count = newLength / sizeof(T);
	}

	void Replace(T subject, T replacement) const
	{
		for (size_t i = 0; i < count; i++)
		{
			if (buffer[i] == subject)
				buffer[i] = replacement;
		}
	}

	zstring* Substring(long long start)
	{
		return Substring(start, count);
	}

	zstring* Substring(long long start, long long end)
	{
		start = WrapIndex(start);
		end = WrapIndex(end);

		auto buffer = (T*)malloc(((end - start) / sizeof(T)) + 1);

		long long dex = 0;
		for (long long i = start; i < end; i++)
			buffer[dex++] = (*this)[i];

		buffer[dex] = NULL;

		return new zstring(buffer);
	}

	static long long IndexOf(T* haystack, T* needle)
	{
		long long count = (WStringLength(haystack) - sizeof(T)) / sizeof(T);
		for (long long i = 0; i < count; i++)
		{
			if (WStringCompare(&haystack[i], needle))
				return i;
		}

		return -1;
	}

	static long long IndexOf(T* haystack, T needle)
	{
		long long count = (WStringLength(haystack) - sizeof(T)) / sizeof(T);
		for (long long i = 0; i < count; i++)
		{
			if (haystack[i] == needle)
				return i;
		}

		return -1;
	}

	long long IndexOf(T subject)
	{
		for (size_t i = 0; i < count; i++)
		{
			if ((*this)[i] == subject)
				return i;
		}

		return -1;
	}

	long long IndexOf(T* subject)
	{
		for (size_t i = 0; i < count; i++)
		{
			if (WStringCompare(&(*this)[i], subject))
				return i;
		}

		return -1;
	}

	long long IndexOf(zstring& subject)
	{
		return IndexOf((T*)subject);
	}

	static bool Contains(T* a, T* b)
	{
		size_t count = (WStringLength(a) - sizeof(T)) / sizeof(T);
		for (size_t i = 0; i < count; i++)
		{
			if (WStringCompare(&a[i], b))
				return true;
		}

		return false;
	}

	static bool Contains(T* a, const T* b)
	{
		return Contains(a, (T*)b);
	}

	static bool Contains(T* a, T b)
	{
		size_t count = (WStringLength(a) - sizeof(T)) / sizeof(T);
		for (size_t i = 0; i < count; i++)
		{
			if (a[i] == b)
				return true;
		}

		return false;
	}

	bool Contains(T subject)
	{
		for (size_t i = 0; i < count; i++)
		{
			if ((*this)[i] == subject)
				return true;
		}

		return false;
	}

	bool Contains(zstring& subject)
	{
		return Contains((T*)subject);
	}

	bool Contains(T* subject)
	{
		T* end = buffer + length;

		for (T* ptr = buffer; ptr < end; ptr++)
		{
			if (WStringStartsWith(ptr, subject))
				return true;
		}

		return false;
	}

	bool Equals(T* subject)
	{
		return WStringCompare(buffer, subject);
	}

	bool Equals(zstring& subject)
	{
		return WStringCompare(buffer, (wchar_t*)subject);
	}

	bool StartsWith(T subject)
	{
		return *buffer == subject;
	}

	bool StartsWith(const T* subject)
	{
		return StartsWith((T*)subject);
	}

	bool StartsWith(T* subject)
	{
		return WStringStartsWith((T*)*this, subject);
	}

	bool StartsWith(zstring& subject)
	{
		return WStringStartsWith((T*)*this, (T*)subject);
	}

	static bool EndsWith(T* a, T* b)
	{
		size_t bLength = WStringLength(b);
		size_t aLength = WStringLength(a);
		return WStringStartsWith(&a[aLength - bLength], b);
	}

	static bool EndsWith(T* a, const T* b)
	{
		return EndsWith(a, (T*)b);
	}

	bool EndsWith(T subject)
	{
		return buffer[count - 1] == subject;
	}

	bool EndsWith(T* subject)
	{
		size_t subjectLength = WStringLength(subject);
		wprintf(L"%ls, %ls\n", (end() - subjectLength), subject);
		return WStringStartsWith(end() - subjectLength, subject);
	}

	bool EndsWith(const T* subject)
	{
		return EndsWith((T*)subject);
	}

	bool EndsWith(zstring& subject)
	{
		return WStringStartsWith(end() - subject.length, (T*)subject);
	}

	size_t Length() const
	{
		return count;
	}

	T* begin()
	{
		return buffer;
	}

	T* end()
	{
		return buffer + length;
	}
};