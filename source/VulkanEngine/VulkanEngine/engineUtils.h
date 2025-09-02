#pragma once

#include <vector>
#include <fstream>
#include <filesystem>


std::vector<char> readFile(const std::string& filename);
bool StringCompare(const char* string1, const char* string2);
void CopySubstring(char* destination, const char* source, size_t num);
void StringCopy(char* dest, char* source, size_t len);

// Reimplementations of the safe versions of standard library functions for non-windows machines
void StringCopySafe(char* dest, size_t destLen, const char* source);
void StringConcatSafe(char* dest, size_t destLen, const char* source);
void StrnCopySafe(char* dest, size_t destLen, const char* source, size_t sourceLen);
void StrnConcatSafe(char* dest, size_t destLen, const char* source, size_t sourceLen);

bool FileExists(const char* filename);
std::filesystem::file_time_type FileDate(const char* filename);