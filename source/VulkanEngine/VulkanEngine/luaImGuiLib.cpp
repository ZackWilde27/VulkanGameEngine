#include "luaImGuiLib.h"
#include "luaUtils.h"

#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <imgui_impl_glfw.h>

static int LuaFN_ImGuiBegin(lua_State* L)
{
	bool wasClosed;
	lua_pushboolean(L, ImGui::Begin(lua_tostring(L, 1), &wasClosed, (lua_gettop(L) == 1) ? 0 : lua_tointeger(L, 2)));
	lua_pushboolean(L, wasClosed);
	return 2;
}

static int LuaFN_ImGuiEnd(lua_State* L)
{
	ImGui::End();
	return 0;
}

static int LuaFN_ImGuiText(lua_State* L)
{
	ImGui::Text(lua_tostring(L, 1));
	return 0;
}

static int LuaFN_ImGuiFloat2(lua_State* L)
{
	LuaData(vec, 2, float2);
	lua_pushboolean(L, ImGui::DragFloat2(lua_tostring(L, 1), (float*)vec, 0.1f));
	return 1;
}

static int LuaFN_ImGuiFloat3(lua_State* L)
{
	LuaData(vec, 2, float3);
	lua_pushboolean(L, ImGui::DragFloat3(lua_tostring(L, 1), (float*)vec, 0.1f));
	return 1;
}

static int LuaFN_ImGuiFloat4(lua_State* L)
{
	LuaData(vec, 2, float4);
	lua_pushboolean(L, ImGui::DragFloat4(lua_tostring(L, 1), (float*)vec, 0.1f));
	return 1;
}

static int LuaFN_ImGuiButton(lua_State* L)
{
	if (lua_gettop(L) == 2)
	{
		LuaData(vec, 2, float2);
		lua_pushboolean(L, ImGui::Button(lua_tostring(L, 1), { vec->x, vec->y }));
	}
	else
		lua_pushboolean(L, ImGui::Button(lua_tostring(L, 1)));

	return 1;
}

static int LuaFN_ImGuiCheckbox(lua_State* L)
{
	bool checked = lua_toboolean(L, 2);

	lua_pushboolean(L, ImGui::Checkbox(lua_tostring(L, 1), &checked));
	lua_pushboolean(L, checked);

	return 2;
}

static int LuaFN_ImGuiDrawTextAt(lua_State* L)
{
	const char* text = lua_tostring(L, 1);
	LuaData(vec, 2, float2);

	ImGui::Begin("text", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMouseInputs | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	{
		ImGui::SetWindowFontScale(lua_tonumber(L, 3));

		ImVec2 scale = ImGui::CalcTextSize(text);
		ImVec2 windowSize = { scale.x * 2, scale.y * 2 };
		ImVec2 pos = { vec->x - (windowSize.x * 0.5f), vec->y - (windowSize.y * 0.5f) };
		ImGui::SetWindowPos(pos);
		ImGui::SetWindowSize(windowSize);

		float offsetX = (windowSize.x - scale.x) * 0.5f;
		float offsetY = (windowSize.y - scale.y) * 0.5f;
		if (offsetX > 0)
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);

		ImGui::Text(text);
	}
	ImGui::End();

	return 0;
}

void Lua_AddImGuiLib(lua_State* L)
{
	lua_createtable(L, 0, 8);

	AddLuaGlobalEnum(ImGuiWindowFlags_None);
	AddLuaGlobalEnum(ImGuiWindowFlags_NoTitleBar);
	AddLuaGlobalEnum(ImGuiWindowFlags_NoResize);
	AddLuaGlobalEnum(ImGuiWindowFlags_NoMove);
	AddLuaGlobalEnum(ImGuiWindowFlags_NoScrollbar);
	AddLuaGlobalEnum(ImGuiWindowFlags_NoScrollWithMouse);
	AddLuaGlobalEnum(ImGuiWindowFlags_NoCollapse);
	AddLuaGlobalEnum(ImGuiWindowFlags_AlwaysAutoResize);
	AddLuaGlobalEnum(ImGuiWindowFlags_NoBackground);
	AddLuaGlobalEnum(ImGuiWindowFlags_NoSavedSettings);
	AddLuaGlobalEnum(ImGuiWindowFlags_NoMouseInputs);
	AddLuaGlobalEnum(ImGuiWindowFlags_MenuBar);
	AddLuaGlobalEnum(ImGuiWindowFlags_HorizontalScrollbar);
	AddLuaGlobalEnum(ImGuiWindowFlags_NoFocusOnAppearing);
	AddLuaGlobalEnum(ImGuiWindowFlags_NoBringToFrontOnFocus);
	AddLuaGlobalEnum(ImGuiWindowFlags_AlwaysVerticalScrollbar);
	AddLuaGlobalEnum(ImGuiWindowFlags_AlwaysHorizontalScrollbar);
	AddLuaGlobalEnum(ImGuiWindowFlags_NoNavInputs);
	AddLuaGlobalEnum(ImGuiWindowFlags_NoNavFocus);
	AddLuaGlobalEnum(ImGuiWindowFlags_UnsavedDocument);
	AddLuaGlobalEnum(ImGuiWindowFlags_NoNav);
	AddLuaGlobalEnum(ImGuiWindowFlags_NoDecoration);
	AddLuaGlobalEnum(ImGuiWindowFlags_NoInputs);

	lua_pushcclosure(L, LuaFN_ImGuiBegin, 0);
	lua_setfield(L, -2, "Begin");

	lua_pushcclosure(L, LuaFN_ImGuiEnd, 0);
	lua_setfield(L, -2, "End");

	lua_pushcclosure(L, LuaFN_ImGuiText, 0);
	lua_setfield(L, -2, "Text");

	lua_pushcclosure(L, LuaFN_ImGuiFloat3, 0);
	lua_setfield(L, -2, "DragFloat3");

	lua_pushcclosure(L, LuaFN_ImGuiFloat4, 0);
	lua_setfield(L, -2, "DragFloat4");

	lua_pushcclosure(L, LuaFN_ImGuiButton, 0);
	lua_setfield(L, -2, "Button");

	lua_pushcclosure(L, LuaFN_ImGuiCheckbox, 0);
	lua_setfield(L, -2, "Checkbox");

	lua_pushcclosure(L, LuaFN_ImGuiDrawTextAt, 0);
	lua_setfield(L, -2, "DrawTextAt");

	lua_setglobal(L, "ImGui");
}