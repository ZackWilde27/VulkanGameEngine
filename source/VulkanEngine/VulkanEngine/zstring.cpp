#include "zstring.h"

#ifdef _WIN32
#include <windows.h>
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
		wvsprintf(buffer, string, v);

		length = WStringLength(buffer);
		count = (length - 2) >> 1;

		auto ptr = (wchar_t*)realloc(buffer, length);
		if (ptr)
			buffer = ptr;

		va_end(v);
	}
}