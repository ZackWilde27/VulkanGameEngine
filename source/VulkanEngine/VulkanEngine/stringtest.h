#pragma once
#include "zstring.h"

#define NEWSTRING() s = new zstring("Hello")
#define PRINTTEST(test, result, expected) std::cout << "Failed test #" << testNumber << ": " << #test << "\n" << "Expected: \"" << expected << "\" but got \"" << result << "\"\n"
#define PASSTEST(test) std::cout << "Passed test #" << testNumber << " (" <<  #test << ")\n"; delete s; testNumber++
#define FAILTEST() delete s; return false

#define STRINGTEST(test, expected) NEWSTRING(); test; if (strcmp(*s, expected)) { PRINTTEST(test, *s, expected); FAILTEST(); } PASSTEST(test)
#define STRINGTEST2(name, test, expected) NEWSTRING(); auto name = test; if (name != expected) { PRINTTEST(test, name, expected); FAILTEST(); } PASSTEST(test)
#define STRINGTEST3(test, expected) NEWSTRING(); out = test; if (strcmp(*out, expected)) { PRINTTEST(test, *out, expected); delete out; FAILTEST(); } delete out; PASSTEST(test)

uint32_t testNumber = 0;

bool StringTest()
{
	// Regular Strings
	zstring<char>* s;
	zstring<char>* out;
	testNumber = 0;

	STRINGTEST(*s += " World!", "Hello World!");
	STRINGTEST(*s <<= 1, "ello");
	STRINGTEST3(s->Substring(2), "llo");
	STRINGTEST3(s->Substring(2, 4), "ll");
	STRINGTEST(s->Replace('e', 'a'), "Hallo");
	std::cout << "Hello?\n";
	STRINGTEST(s->Replace("ello", "alogen"), "Halogen");

	std::cout << "Hello?\n";
	/*
	NEWSTRING();
	char* b = s->CopyToBuffer();
	if (strcmp(*s, b))
	{
		PRINTTEST(s->CopyToBuffer(), b, *s);
		free(b);
		FAILTEST();
	}
	PASSTEST(s->CopyToBuffer());
	*/

	STRINGTEST2(l, s->Length(), 6);

	STRINGTEST2(c1, s->Contains((char*)"ell"), true);
	STRINGTEST2(c2, s->Contains((char*)"No"), false);

	STRINGTEST2(s1, s->StartsWith("Hel"), true);
	STRINGTEST2(s2, s->StartsWith("No"), false);

	std::cout << "No Errors!\n";
	return true;
}