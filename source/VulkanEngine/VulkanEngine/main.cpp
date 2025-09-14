#include "engine.h"
#include <iostream>

// PrintF can be changed for situations where the console is not visible
void PrintF(const char* message, ...)
{
	va_list v;
	va_start(v, message);
	vprintf(message, v);
	va_end(v);
}

int main()
{
	LastGenEngine* lge;
#ifdef _DEBUG
	try
	{
#endif
		lge = new LastGenEngine();

		glfwSetInputMode(lge->glWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		glfwSetInputMode(lge->glWindow, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

		lge->Run();
		delete lge;
#ifdef _DEBUG
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}
#endif

	return EXIT_SUCCESS;
}