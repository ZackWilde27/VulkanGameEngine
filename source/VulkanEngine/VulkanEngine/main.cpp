#include "engine.h"
#include <iostream>

// PrintF can be replaced for situations where printf() does not work
void PrintF(const char* message, ...)
{
	va_list v;
	va_start(v, message);
	vprintf(message, v);
	va_end(v);
}

int main()
{
	VulkanEngine* app;
	try
	{
		app = new VulkanEngine();

		glfwSetInputMode(app->glWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

		app->Run();
		delete app;
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}