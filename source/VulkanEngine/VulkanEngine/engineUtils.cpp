#include "engineUtils.h"

// The readFile function will read all of the bytes from the specified file and return them in a byte array managed by std::vector.
std::vector<char> readFile(const std::string& filename)
{
	// We start by opening the file with two flags:
	// - ate: Start reading at the end of the file
	// - binary : Read the file as binary file (avoid text transformations)
	// The advantage of starting to read at the end of the file is that we can use the read position to determine the size of the file and allocate a buffer
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open())
	{
		printf("\'");
		printf(filename.c_str());
		printf("\'");
		printf("\n");
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

bool StringCompare(const char* string1, const char* string2)
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

void StringCopy(char* dest, char* source, size_t len)
{
	while (len--)
		*dest++ = *source++;
}

void StringCopySafe(char* dest, size_t destLen, const char* source)
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

void StrnCopySafe(char* dest, size_t destLen, const char* source, size_t sourceLen)
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

void StringConcatSafe(char* dest, size_t destLen, const char* source)
{
	while (*dest && destLen)
	{
		dest++;
		destLen--;
	}

	StringCopySafe(dest, destLen, source);
}

void StrnConcatSafe(char* dest, size_t destLen, const char* source, size_t sourceLen)
{
	while (*dest && destLen)
	{
		dest++;
		destLen--;
	}

	StrnCopySafe(dest, destLen, source, sourceLen);
}

char* NewString(const char* string)
{
	size_t length = strlen(string) + 1;
	auto newstr = (char*)malloc(length);

	StringCopy(newstr, (char*)string, length);

	return newstr;
}


bool FileExists(const char* filename)
{
	FILE* f = fopen(filename, "r");
	bool exists = f;
	if (exists)
		fclose(f);

	return exists;
}

std::filesystem::file_time_type FileDate(const char* filename)
{
	return std::filesystem::last_write_time(filename);
}