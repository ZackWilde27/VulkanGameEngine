#include "zstring.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <cstring>
#include <cstdarg>
#include <wchar.h>
#endif

#ifndef _WIN32
template<>
#endif
zstring<char>::zstring(const char* string, ...)
{
	if (!string)
	{
		buffer = (char*)zstring::Empty;
		length = 0;
		count = 0;
		return;
	}

	buffer = (char*)malloc(MAX_STRING_LENGTH);
	memset(buffer, 0, MAX_STRING_LENGTH);

	if (buffer != NULL)
	{
		va_list v;
		va_start(v, string);
		vsprintf(buffer, string, v);

		length = WStringLength(buffer);
		count = length - 1;

		auto ptr = (char*)realloc(buffer, length);
		if (ptr)
			buffer = ptr;

		va_end(v);
	}
}

#ifndef _WIN32
template<>
#endif
zstring<wchar_t>::zstring(const wchar_t* string, ...)
{
	if (!string)
	{
		buffer = (wchar_t*)zstring::Empty;
		length = 0;
		count = 0;
		return;
	}

	length = MAX_STRING_LENGTH * 2;

	buffer = (wchar_t*)malloc(length);
	memset(buffer, 0, length);

	if (buffer != NULL)
	{
		va_list v;
		va_start(v, string);
#ifdef _WIN32
		wvsprintf(buffer, string, v);
#else
        vswprintf(buffer, length, string, v);
#endif

		length = WStringLength(buffer);
		count = (length - 2) >> 1;

		auto ptr = (wchar_t*)realloc(buffer, length);
		if (ptr)
			buffer = ptr;

		va_end(v);
	}
}
