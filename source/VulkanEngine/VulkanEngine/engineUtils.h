#pragma once

#include <vector>
#include <fstream>
#include <filesystem>


std::vector<char> readFile(const std::string& filename);

// Unlike strcmp, this one does not include the terminator so it actually checks if string1 starts with string2
bool StringCompare(const char* string1, const char* string2);

// Copies len number of characters from the source to the destination, does not automatically terminate the string
void StringCopy(char* dest, char* source, size_t len);

// Reimplementations of the safe versions of standard library functions for non-windows machines

// Copies as much of the source string as it can fit and automatically adds the terminator
void StringCopySafe(char* dest, size_t destLen, const char* source);
// Seeks to the terminator of the string and copies as much of the source string as it can fit, automatically adds the terminator
void StringConcatSafe(char* dest, size_t destLen, const char* source);

// Same as StringCopySafe but adds a parameter for source length, just like strncpy
void StrnCopySafe(char* dest, size_t destLen, const char* source, size_t sourceLen);
// Same as StringConcatSafe, but adds a parameter for source length, just like strncat
void StrnConcatSafe(char* dest, size_t destLen, const char* source, size_t sourceLen);

bool FileExists(const char* filename);
std::filesystem::file_time_type FileDate(const char* filename);