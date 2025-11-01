#pragma once

#include <vector>
#include <fstream>
#include <iostream>
#include <filesystem>

// The readFile function will read all of the bytes from the specified file and return them in a byte array managed by std::vector.
template<typename T>
std::vector<char> readFile(const T* filename)
{
	// We start by opening the file with two flags:
	// - ate: Start reading at the end of the file
	// - binary : Read the file as binary file (avoid text transformations)
	// The advantage of starting to read at the end of the file is that we can use the read position to determine the size of the file and allocate a buffer
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open())
	{
		std::wcout << filename << L"\n";
		throw std::runtime_error("failed to open file!");
	}

	size_t fileSize = (size_t)file.tellg();
	std::vector<char> buffer(fileSize);

	// After that, we can seek back to the beginning of the file and read all of the bytes at once
	file.seekg(0);
	file.read(buffer.data(), fileSize);

	file.close();

	return buffer;
}

template <typename T>
bool StringCompare(const T* string1, const T* string2)
{
	if (*string2)
	{
		while (*string2)
		{
			if (*string1++ != *string2++)
				return false;
		}
		return true;
	}

	return !(*string1);
}

template <typename T>
void StringCopy(T* dest, T* source, size_t len)
{
	while (len--)
		*dest++ = *source++;
}

template <typename T>
void StringCopySafe(T* dest, size_t destLen, const T* source)
{
	if (destLen)
	{
		// destLen - 1 to fit in the terminator
		destLen--;

		while (*source && destLen--)
			*dest++ = *source++;

		*dest = NULL;
	}
}

template <typename T>
void StrnCopySafe(T* dest, size_t destLen, const T* source, size_t sourceLen)
{
	if (destLen && sourceLen)
	{
		// destLen - 1 to fit in the terminator
		destLen--;

		while (sourceLen-- && destLen--)
			*dest++ = *source++;

		*dest = NULL;
	}
}

template <typename T>
void StringConcatSafe(T* dest, size_t destLen, const T* source)
{
	while (*dest && destLen)
	{
		dest++;
		destLen--;
	}

	StringCopySafe(dest, destLen, source);
}

template <typename T>
void StrnConcatSafe(T* dest, size_t destLen, const T* source, size_t sourceLen)
{
	while (*dest && destLen)
	{
		dest++;
		destLen--;
	}

	StrnCopySafe(dest, destLen, source, sourceLen);
}

template<typename T>
bool FileExists(const T* filename)
{
	std::ifstream file(filename, std::ios_base::in);

	bool exists = file.is_open();
	if (exists)
		file.close();

	return exists;
}

template <typename T>
std::filesystem::file_time_type FileDate(const T* filename)
{
	return std::filesystem::last_write_time(filename);
}