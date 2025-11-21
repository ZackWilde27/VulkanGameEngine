#include "engineUtils.h"
#include <cstring>
#include <iostream>


#ifndef _WIN32

#include "zstring.h"
std::vector<char> readFile(const char* filename)
{
	// We start by opening the file with two flags:
	// - ate: Start reading at the end of the file
	// - binary : Read the file as binary file (avoid text transformations)
	// The advantage of starting to read at the end of the file is that we can use the read position to determine the size of the file and allocate a buffer
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open())
	{
		std::cout << filename << "\n";
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

std::vector<char> readFile(const wchar_t* filename)
{
    zstring converted("%ls", filename);
    return readFile((char*)*converted);
}

bool FileExists(const char* filename)
{
	std::ifstream file(filename, std::ios_base::in);

	bool exists = file.is_open();
	if (exists)
		file.close();

	return exists;
}

bool FileExists(const wchar_t* filename)
{
    zstring converted("%ls", filename);

    return FileExists((char*)*converted);
}

#endif
