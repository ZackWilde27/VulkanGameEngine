#pragma once

#include "engineUtils.h"

struct INIItem
{
	const char* name;
	void* ptr;
	char type;
};

std::vector<INIItem> iniItems;

constexpr size_t INI_BUFFER_SIZE = 256;

char leftBuffer[INI_BUFFER_SIZE];
char rightBuffer[INI_BUFFER_SIZE];

// To me it makes a lot more sense for a string compare to return true if they are the same
#define stringcmp(x, y) !strcmp(x, y)

VkFormat FormatToString(char* string)
{
	if (stringcmp(string, "HDR"))
		return VK_FORMAT_R16G16B16A16_SFLOAT;

	return VK_FORMAT_R8G8B8A8_SRGB;
}

inline void INI_Float(const char* name, float* ptr)
{
	iniItems.push_back({ name, ptr, 'f' });
}

inline void INI_Double(const char* name, double* ptr)
{
	iniItems.push_back({ name, ptr, 'd' });
}

inline void INI_Int(const char* name, int* ptr)
{
	iniItems.push_back({ name, ptr, 'i' });
}

inline void INI_VkFormat(const char* name, VkFormat* ptr)
{
	iniItems.push_back({ name, ptr, 'v' });
}

inline void INI_Bool(const char* name, bool* ptr)
{
	iniItems.push_back({ name, ptr, 'b' });
}

inline void INI_UInt32(const char* name, uint32_t* ptr)
{
	iniItems.push_back({ name, ptr, 'I' });
}

inline void INI_ULongLong(const char* name, unsigned long long* ptr)
{
	iniItems.push_back({ name,  ptr, 'L' });
}

inline void INI_SIZET(const char* name, size_t* ptr)
{
	iniItems.push_back({ name, ptr, 'S' });
}

inline void INI_String(const char* name, char** ptr)
{
	iniItems.push_back({ name, ptr, 's' });
}

void DereferenceAbitrary(void* ptr, char type)
{
	char* temp;
	size_t length;

	switch (type)
	{
		case 'd':
			*(double*)ptr = strtod(rightBuffer, &temp);
			break;

		case 'f':
			*(float*)ptr = strtof(rightBuffer, &temp);
			break;

		case 'i':
			*(int*)ptr = strtol(rightBuffer, &temp, 10);
			break;

		case 'I':
			*(uint32_t*)ptr = strtoul(rightBuffer, &temp, 10);
			break;

		case 'v':
			*(VkFormat*)ptr = FormatToString(rightBuffer);
			break;

		case 'b':
			*(bool*)ptr = !strcmp(rightBuffer, "true");
			break;

		case 'L':
			*(unsigned long long*)ptr = strtoull(rightBuffer, &temp, 10);
			break;

		case 'S':
			*(size_t*)ptr = strtoull(rightBuffer, &temp, 10);
			break;

		case 's':
			length = strlen(rightBuffer);
			*(char**)ptr = (char*)malloc(length + 1);
			if (*(char**)ptr)
			{
				StringCopy(*(char**)ptr, rightBuffer, length);
				(*(char**)ptr)[length] = NULL;
			}
			break;
	}
}

void InterpretBuffers()
{
	for (size_t i = 0; i < iniItems.size(); i++)
	{
		if (stringcmp(iniItems[i].name, leftBuffer))
		{
			DereferenceAbitrary(iniItems[i].ptr, iniItems[i].type);
			break;
		}
	}
}

void ReadINIFile(const char* filename)
{
	std::vector<char> bytes = readFile(filename);

	char* ptr = bytes.data();
	char* endPtr = bytes.data() + bytes.size();

	ZEROMEM(leftBuffer, INI_BUFFER_SIZE);
	ZEROMEM(rightBuffer, INI_BUFFER_SIZE);
	char* bufferPtr = leftBuffer;
	while (ptr < endPtr)
	{
		switch (*ptr)
		{
		case '\t':
		case ' ':
			break;

		case '\n':
			InterpretBuffers();

			ZEROMEM(leftBuffer, INI_BUFFER_SIZE);
			ZEROMEM(rightBuffer, INI_BUFFER_SIZE);
			bufferPtr = leftBuffer;
			break;

		case '=':
			bufferPtr = rightBuffer;
			break;

		default:
			*bufferPtr++ = *ptr;
			break;
		}
		ptr++;
	}
	InterpretBuffers();

	iniItems.clear();
}