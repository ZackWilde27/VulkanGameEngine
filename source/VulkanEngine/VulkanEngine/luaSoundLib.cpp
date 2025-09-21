#include "luaSoundLib.h"
#include "luaUtils.h"
#include "luafunctions.h"

#define GetSound auto sound = (SoundEngine*)lua_touserdata(L, lua_upvalueindex(1))

#define AddSoundFunction(func, name) 	lua_pushlightuserdata(L, sound); \
															lua_pushcclosure(L, func, 1); \
															lua_setfield(L, -2, name)

#define AddMAFunction(func, name)	lua_pushcclosure(L, func, 0); \
														lua_setfield(L, -2, name)

static int LuaFN_SoundStop(lua_State* L)
{
	GetSound;
	auto s = (Sound*)lua_touserdata(L, lua_upvalueindex(2));

	sound->StopSound(s);

	return 0;
}

static int LuaFN_SoundPause(lua_State* L)
{
	auto s = (Sound*)lua_touserdata(L, lua_upvalueindex(1));

	ma_sound_stop(&s->sound);

	return 0;
}

static int LuaFN_SoundResume(lua_State* L)
{
	auto s = (Sound*)lua_touserdata(L, lua_upvalueindex(1));

	ma_sound_start(&s->sound);

	return 0;
}

static int LuaFN_SoundFadeOut(lua_State* L)
{
	auto s = (Sound*)lua_touserdata(L, lua_upvalueindex(1));
	
	ma_sound_stop_with_fade_in_milliseconds(&s->sound, lua_tointeger(L, 1));

	return 0;
}

static void Lua_PushSound(lua_State* L, Sound* sound, SoundEngine* engine)
{
	lua_createtable(L, 0, 5);
	lua_pushlightuserdata(L, sound);
	lua_setfield(L, -2, "data");

	lua_pushlightuserdata(L, engine);
	lua_pushlightuserdata(L, sound);
	lua_pushcclosure(L, LuaFN_SoundStop, 2);
	lua_setfield(L, -2, "Stop");

	lua_pushlightuserdata(L, sound);
	lua_pushcclosure(L, LuaFN_SoundPause, 1);
	lua_setfield(L, -2, "Pause");

	lua_pushlightuserdata(L, sound);
	lua_pushcclosure(L, LuaFN_SoundResume, 1);
	lua_setfield(L, -2, "Resume");

	lua_pushlightuserdata(L, sound);
	lua_pushcclosure(L, LuaFN_SoundFadeOut, 1);
	lua_setfield(L, -2, "FadeOut");
}

static int LuaFN_PlaySoundSimple2D(lua_State* L)
{
	GetSound;

	sound->PlaySimple2D(lua_tostring(L, 1));

	return 0;
}

static int LuaFN_PlaySoundLooping2D(lua_State* L)
{
	GetSound;

	Lua_PushSound(L, sound->PlayLooping2D(lua_tostring(L, 1)), sound);

	return 1;
}

static int LuaFN_PlaySoundComplex2D(lua_State* L)
{
	GetSound;

	Lua_PushSound(L, sound->PlayComplex2D(lua_tostring(L, 1), lua_tointeger(L, 2), lua_tointeger(L, 3), lua_tonumber(L, 4), lua_tonumber(L, 5), lua_tonumber(L, 6), lua_toboolean(L, 7)), sound);

	return 1;
}

static int LuaFN_PlaySoundSimple3D(lua_State* L)
{
	GetSound;

	LuaData(position, 2, float3);

	// something is strange about the exponential attenuation model
	// the max distance doesn't seem to matter at all, and if the min distance is 0 you can't hear anything no matter where the camera is
	Lua_PushSound(L, sound->PlaySimple3D(lua_tostring(L, 1), *position, (ma_attenuation_model)lua_tointeger(L, 3), lua_tonumber(L, 4), lua_tonumber(L, 5)), sound);

	return 1;
}

static int LuaFN_PlaySoundLooping3D(lua_State* L)
{
	GetSound;

	LuaData(position, 2, float3);

	Lua_PushSound(L, sound->PlayLooping3D(lua_tostring(L, 1), *position, (ma_attenuation_model)lua_tointeger(L, 3), lua_tonumber(L, 4), lua_tonumber(L, 5)), sound);

	return 1;
}

static int LuaFN_PlaySoundComplex3D(lua_State* L)
{
	GetSound;

	LuaData(position, 2, float3);

	Lua_PushSound(L, sound->PlayComplex3D(lua_tostring(L, 1), *position, (ma_attenuation_model)lua_tointeger(L, 3), lua_tonumber(L, 4), lua_tonumber(L, 5), lua_tointeger(L, 6), lua_tointeger(L, 7), lua_tonumber(L, 8), lua_tonumber(L, 9), lua_toboolean(L, 10)), sound);

	return 1;
}

static int LuaFN_AttachSoundToThingSimple(lua_State* L)
{
	GetSound;

	LuaData(thing, 2, Thing);

	if (lua_isnil(L, 3))
		printf("\nsound.AttachToThingSimple(): Attenuation model was nil!\nThe attenuation model can be one of the following:\n- ma_attenuation_model_none\n- ma_attenuation_model_linear\n- ma_attenuation_model_exponential\n- ma_attenuation_model_inverse\n\n");

	Lua_PushSound(L, sound->AttachSoundToThingSimple(lua_tostring(L, 1), thing, (ma_attenuation_model)lua_tointeger(L, 3), lua_tonumber(L, 4), lua_tonumber(L, 5)), sound);

	return 1;
}

static int LuaFN_AttachSoundToThingLooping(lua_State* L)
{
	GetSound;

	LuaData(thing, 2, Thing);

	Lua_PushSound(L, sound->AttachSoundToThingLooping(lua_tostring(L, 1), thing, (ma_attenuation_model)lua_tointeger(L, 3), lua_tonumber(L, 4), lua_tonumber(L, 5)), sound);

	return 1;
}

static int LuaFN_AttachSoundToThingComplex(lua_State* L)
{
	GetSound;

	LuaData(thing, 2, Thing);

	Lua_PushSound(L, sound->AttachSoundToThingComplex(lua_tostring(L, 1), thing, (ma_attenuation_model)lua_tointeger(L, 3), lua_tonumber(L, 4), lua_tonumber(L, 5), lua_tointeger(L, 6), lua_tointeger(L, 7), lua_tonumber(L, 8), lua_tonumber(L, 9), lua_toboolean(L, 10)), sound);

	return 1;
}

static int LuaFN_PlayMusic(lua_State* L)
{
	printf("Playing music...\n");

	GetSound;

	sound->PlayMusic(lua_tostring(L, 1), lua_toboolean(L, 2));

	return 0;
}

static int LuaFN_StopMusic(lua_State* L)
{
	GetSound;

	sound->StopMusic();

	return 0;
}

static int LuaFN_PauseMusic(lua_State* L)
{
	GetSound;

	sound->PauseMusic();

	return 0;
}

static int LuaFN_ResumeMusic(lua_State* L)
{
	GetSound;

	sound->PauseMusic();

	return 0;
}

static int LuaFN_FadeOutMusic(lua_State* L)
{
	GetSound;

	sound->FadeOutMusic(lua_tointeger(L, 1));

	return 0;
}

static int LuaFN_StopAll(lua_State* L)
{
	GetSound;

	sound->StopAll();

	return 0;
}

static int LuaFN_StopAllSounds(lua_State* L)
{
	GetSound;

	sound->StopAllSounds();

	return 0;
}

static int LuaFN_ma_sound_gc(lua_State* L)
{
	LuaData(sound, 1, ma_sound);

	ma_sound_stop(sound);
	ma_sound_uninit(sound);

	return 0;
}

static void Lua_Push_ma_sound_idx(lua_State* L, int index)
{
	lua_createtable(L, 0, 1);

	lua_rotate(L, index, -1);
	lua_setfield(L, -2, "data");


	lua_createtable(L, 0, 1);

	lua_pushcclosure(L, LuaFN_ma_sound_gc, 0);
	lua_setfield(L, -2, "__gc");

	lua_setmetatable(L, -2);
}

static int LuaFN_ma_sound_start(lua_State* L)
{
	LuaData(sound, 1, ma_sound);

	lua_pushinteger(L, ma_sound_start(sound));

	return 1;
}

static int LuaFN_ma_sound_stop(lua_State* L)
{
	LuaData(sound, 1, ma_sound);

	lua_pushinteger(L, ma_sound_stop(sound));

	return 1;
}

static int LuaFN_ma_sound_init_from_file(lua_State* L)
{
	GetSound;

	auto newSound = Lua_New(ma_sound);

	lua_pushinteger(L, sound->InitFromFile(lua_tostring(L, 1), lua_tointeger(L, 2), newSound));
	Lua_Push_ma_sound_idx(L, 3);

	return 2;
}

static int LuaFN_ma_sound_is_playing(lua_State* L)
{
	LuaData(sound, 1, ma_sound);

	lua_pushboolean(L, ma_sound_is_playing(sound));

	return 1;
}

static int LuaFN_ma_sound_is_looping(lua_State* L)
{
	LuaData(sound, 1, ma_sound);

	lua_pushboolean(L, ma_sound_is_looping(sound));

	return 1;
}

static int LuaFN_ma_sound_is_spatialization_enabled(lua_State* L)
{
	LuaData(sound, 1, ma_sound);

	lua_pushboolean(L, ma_sound_is_spatialization_enabled(sound));

	return 1;
}

static int LuaFN_ma_sound_set_attenuation_model(lua_State* L)
{
	LuaData(sound, 1, ma_sound);

	ma_sound_set_attenuation_model(sound, (ma_attenuation_model)lua_tointeger(L, 2));

	return 0;
}

static int LuaFN_ma_sound_set_cone(lua_State* L)
{
	LuaData(sound, 1, ma_sound);

	ma_sound_set_cone(sound, lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4));

	return 0;
}

static int LuaFN_ma_sound_set_direction(lua_State* L)
{
	LuaData(sound, 1, ma_sound);
	LuaData(dir, 2, float3);

	ma_sound_set_direction(sound, dir->x, dir->y, dir->z);

	return 0;
}

static int LuaFN_ma_sound_set_directional_attenuation_factor(lua_State* L)
{
	LuaData(sound, 1, ma_sound);

	ma_sound_set_directional_attenuation_factor(sound, lua_tonumber(L, 2));

	return 0;
}

static int LuaFN_ma_sound_set_doppler_factor(lua_State* L)
{
	LuaData(sound, 1, ma_sound);

	ma_sound_set_doppler_factor(sound, lua_tonumber(L, 2));

	return 0;
}

static int LuaFN_ma_sound_set_fade_in_milliseconds(lua_State* L)
{
	LuaData(sound, 1, ma_sound);

	ma_sound_set_fade_in_milliseconds(sound, lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tointeger(L, 4));

	return 0;
}

static int LuaFN_ma_sound_set_fade_start_in_milliseconds(lua_State* L)
{
	LuaData(sound, 1, ma_sound);

	ma_sound_set_fade_start_in_milliseconds(sound, lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tointeger(L, 4), lua_tointeger(L, 5));

	return 0;
}

static int LuaFN_ma_sound_set_looping(lua_State* L)
{
	LuaData(sound, 1, ma_sound);

	ma_sound_set_looping(sound, lua_toboolean(L, 2));

	return 0;
}

static int LuaFN_ma_sound_set_max_distance(lua_State* L)
{
	LuaData(sound, 1, ma_sound);

	ma_sound_set_max_distance(sound, lua_tonumber(L, 2));

	return 0;
}

static int LuaFN_ma_sound_set_max_gain(lua_State* L)
{
	LuaData(sound, 1, ma_sound);

	ma_sound_set_max_gain(sound, lua_tonumber(L, 2));

	return 0;
}

static int LuaFN_ma_sound_set_pan(lua_State* L)
{
	LuaData(sound, 1, ma_sound);

	ma_sound_set_pan(sound, lua_tonumber(L, 2));

	return 0;
}

static int LuaFN_ma_sound_set_pan_mode(lua_State* L)
{
	LuaData(sound, 1, ma_sound);

	ma_sound_set_pan_mode(sound, (ma_pan_mode)lua_tointeger(L, 2));

	return 0;
}

static int LuaFN_ma_sound_set_pinned_listener_index(lua_State* L)
{
	LuaData(sound, 1, ma_sound);

	ma_sound_set_pinned_listener_index(sound, lua_tointeger(L, 2));

	return 0;
}

static int LuaFN_ma_sound_set_pitch(lua_State* L)
{
	LuaData(sound, 1, ma_sound);

	ma_sound_set_pitch(sound, lua_tonumber(L, 2));

	return 0;
}

static int LuaFN_ma_sound_set_position(lua_State* L)
{
	LuaData(sound, 1, ma_sound);
	LuaData(position, 2, float3);

	ma_sound_set_position(sound, position->x, position->y, position->z);

	return 0;
}

static int LuaFN_ma_sound_set_positioning(lua_State* L)
{
	LuaData(sound, 1, ma_sound);

	ma_sound_set_positioning(sound, (ma_positioning)lua_tointeger(L, 2));

	return 0;
}

static int LuaFN_ma_sound_set_rolloff(lua_State* L)
{
	LuaData(sound, 1, ma_sound);

	ma_sound_set_rolloff(sound, lua_tonumber(L, 2));

	return 0;
}

static int LuaFN_ma_sound_set_spatialization_enabled(lua_State* L)
{
	LuaData(sound, 1, ma_sound);

	ma_sound_set_spatialization_enabled(sound, lua_toboolean(L, 2));

	return 0;
}

static int LuaFN_ma_sound_set_start_time_in_milliseconds(lua_State* L)
{
	LuaData(sound, 1, ma_sound);

	ma_sound_set_start_time_in_milliseconds(sound, lua_tointeger(L, 2));

	return 0;
}

static int LuaFN_ma_sound_set_stop_time_in_milliseconds(lua_State* L)
{
	LuaData(sound, 1, ma_sound);

	ma_sound_set_stop_time_in_milliseconds(sound, lua_tointeger(L, 2));

	return 0;
}

static int LuaFN_ma_sound_set_stop_time_with_fade_in_milliseconds(lua_State* L)
{
	LuaData(sound, 1, ma_sound);

	ma_sound_set_stop_time_with_fade_in_milliseconds(sound, lua_tointeger(L, 2), lua_tointeger(L, 3));

	return 0;
}

static int LuaFN_ma_sound_set_velocity(lua_State* L)
{
	LuaData(sound, 1, ma_sound);
	LuaData(vel, 2, float3);

	ma_sound_set_velocity(sound, vel->x, vel->y, vel->z);

	return 0;
}

static int LuaFN_ma_sound_set_volume(lua_State* L)
{
	LuaData(sound, 1, ma_sound);

	ma_sound_set_volume(sound, lua_tonumber(L, 2));

	return 0;
}

static int LuaFN_ma_sound_get_attenuation_model(lua_State* L)
{
	LuaData(sound, 1, ma_sound);

	lua_pushinteger(L, ma_sound_get_attenuation_model(sound));

	return 1;
}

static int LuaFN_ma_sound_get_cone(lua_State* L)
{
	LuaData(sound, 1, ma_sound);

	float outerAngle, innerAngle, outerGain;
	ma_sound_get_cone(sound, &innerAngle, &outerAngle, &outerGain);

	lua_pushnumber(L, innerAngle);
	lua_pushnumber(L, outerAngle);
	lua_pushnumber(L, outerGain);

	return 3;
}

static int LuaFN_ma_sound_get_direction(lua_State* L)
{
	LuaData(sound, 1, ma_sound);
	auto dir = ma_sound_get_direction(sound);

	auto vec = (float3*)lua_newuserdata(L, sizeof(float3));
	*vec = float3(dir.x, dir.y, dir.z);

	Lua_PushFloat3_idx(L, -1);
	return 1;
}

static int LuaFN_ma_sound_get_directional_attenuation_factor(lua_State* L)
{
	LuaData(sound, 1, ma_sound);

	lua_pushnumber(L, ma_sound_get_directional_attenuation_factor(sound));

	return 1;
}

static int LuaFN_ma_sound_get_doppler_factor(lua_State* L)
{
	LuaData(sound, 1, ma_sound);

	lua_pushnumber(L, ma_sound_get_doppler_factor(sound));

	return 1;
}

static int LuaFN_ma_sound_get_max_distance(lua_State* L)
{
	LuaData(sound, 1, ma_sound);

	lua_pushnumber(L, ma_sound_get_max_distance(sound));

	return 1;
}

static int LuaFN_ma_sound_get_max_gain(lua_State* L)
{
	LuaData(sound, 1, ma_sound);

	lua_pushnumber(L, ma_sound_get_max_gain(sound));

	return 1;
}

static int LuaFN_ma_sound_get_pan(lua_State* L)
{
	LuaData(sound, 1, ma_sound);

	lua_pushnumber(L, ma_sound_get_pan(sound));

	return 1;
}

static int LuaFN_ma_sound_get_pan_mode(lua_State* L)
{
	LuaData(sound, 1, ma_sound);

	lua_pushinteger(L, ma_sound_get_pan_mode(sound));

	return 1;
}

static int LuaFN_ma_sound_get_pinned_listener_index(lua_State* L)
{
	LuaData(sound, 1, ma_sound);

	lua_pushinteger(L, ma_sound_get_pinned_listener_index(sound));

	return 1;
}

static int LuaFN_ma_sound_get_pitch(lua_State* L)
{
	LuaData(sound, 1, ma_sound);

	lua_pushnumber(L, ma_sound_get_pitch(sound));

	return 1;
}

static int LuaFN_ma_sound_get_position(lua_State* L)
{
	LuaData(sound, 1, ma_sound);

	ma_vec3f pos = ma_sound_get_position(sound);

	auto vec = (float3*)lua_newuserdata(L, sizeof(float3));
	*vec = float3(pos.x, pos.y, pos.z);

	Lua_PushFloat3_idx(L, -1);
	return 1;
}

static int LuaFN_ma_sound_get_positioning(lua_State* L)
{
	LuaData(sound, 1, ma_sound);

	lua_pushinteger(L, ma_sound_get_positioning(sound));

	return 1;
}

static int LuaFN_ma_sound_get_rolloff(lua_State* L)
{
	LuaData(sound, 1, ma_sound);

	lua_pushnumber(L, ma_sound_get_rolloff(sound));

	return 1;
}

static int LuaFN_ma_sound_get_velocity(lua_State* L)
{
	LuaData(sound, 1, ma_sound);

	ma_vec3f vel = ma_sound_get_velocity(sound);

	auto vec = (float3*)lua_newuserdata(L, sizeof(float3));
	*vec = float3(vel.x, vel.y, vel.z);

	Lua_PushFloat3_idx(L, -1);
	return 1;
}

static int LuaFN_ma_sound_get_volume(lua_State* L)
{
	LuaData(sound, 1, ma_sound);

	lua_pushnumber(L, ma_sound_get_volume(sound));

	return 1;
}

void Lua_AddSoundLib(lua_State* L, SoundEngine* sound)
{
	// First is my wrapper for miniaudio, to make simple sound effects easier
	AddLuaGlobalEnum(ma_attenuation_model_none);
	AddLuaGlobalEnum(ma_attenuation_model_linear);
	AddLuaGlobalEnum(ma_attenuation_model_exponential);
	AddLuaGlobalEnum(ma_attenuation_model_inverse);

	lua_createtable(L, 0, 16);

	AddSoundFunction(LuaFN_PlaySoundSimple2D, "PlaySimple2D");
	AddSoundFunction(LuaFN_PlaySoundLooping2D, "PlayLooping2D");
	AddSoundFunction(LuaFN_PlaySoundComplex2D, "PlayComplex2D");

	AddSoundFunction(LuaFN_PlaySoundSimple3D, "PlaySimple3D");
	AddSoundFunction(LuaFN_PlaySoundLooping3D, "PlayLooping3D");
	AddSoundFunction(LuaFN_PlaySoundComplex3D, "PlayComplex3D");

	AddSoundFunction(LuaFN_AttachSoundToThingSimple, "AttachToThingSimple");
	AddSoundFunction(LuaFN_AttachSoundToThingLooping, "AttachToThingLooping");
	AddSoundFunction(LuaFN_AttachSoundToThingComplex, "AttachToThingComplex");

	AddSoundFunction(LuaFN_StopAllSounds, "StopAllSounds");

	AddSoundFunction(LuaFN_PlayMusic, "PlayMusic");
	AddSoundFunction(LuaFN_StopMusic, "StopMusic");
	AddSoundFunction(LuaFN_PauseMusic, "PauseMusic");
	AddSoundFunction(LuaFN_ResumeMusic, "ResumeMusic");
	AddSoundFunction(LuaFN_FadeOutMusic, "FadeOutMusic");

	AddSoundFunction(LuaFN_StopAll, "StopAll");

	lua_setglobal(L, "sound");

	// Then comes a literal port of miniaudio if you want more control
	AddLuaGlobalEnum(MA_SUCCESS);
	AddLuaGlobalEnum(MA_ERROR);
	AddLuaGlobalEnum(MA_INVALID_ARGS);
	AddLuaGlobalEnum(MA_INVALID_OPERATION);
	AddLuaGlobalEnum(MA_OUT_OF_MEMORY);
	AddLuaGlobalEnum(MA_OUT_OF_RANGE);
	AddLuaGlobalEnum(MA_ACCESS_DENIED);
	AddLuaGlobalEnum(MA_DOES_NOT_EXIST);
	AddLuaGlobalEnum(MA_ALREADY_EXISTS);
	AddLuaGlobalEnum(MA_TOO_MANY_OPEN_FILES);
	AddLuaGlobalEnum(MA_INVALID_FILE);
	AddLuaGlobalEnum(MA_TOO_BIG);
	AddLuaGlobalEnum(MA_PATH_TOO_LONG);
	AddLuaGlobalEnum(MA_NAME_TOO_LONG);
	AddLuaGlobalEnum(MA_NOT_DIRECTORY);
	AddLuaGlobalEnum(MA_IS_DIRECTORY);
	AddLuaGlobalEnum(MA_DIRECTORY_NOT_EMPTY);
	AddLuaGlobalEnum(MA_AT_END);
	AddLuaGlobalEnum(MA_NO_SPACE);
	AddLuaGlobalEnum(MA_BUSY);
	AddLuaGlobalEnum(MA_IO_ERROR);
	AddLuaGlobalEnum(MA_INTERRUPT);
	AddLuaGlobalEnum(MA_UNAVAILABLE);
	AddLuaGlobalEnum(MA_ALREADY_IN_USE);
	AddLuaGlobalEnum(MA_BAD_ADDRESS);
	AddLuaGlobalEnum(MA_BAD_SEEK);
	AddLuaGlobalEnum(MA_BAD_PIPE);
	AddLuaGlobalEnum(MA_DEADLOCK);
	AddLuaGlobalEnum(MA_TOO_MANY_LINKS);
	AddLuaGlobalEnum(MA_NOT_IMPLEMENTED);
	AddLuaGlobalEnum(MA_NO_MESSAGE);
	AddLuaGlobalEnum(MA_BAD_MESSAGE);
	AddLuaGlobalEnum(MA_NO_DATA_AVAILABLE);
	AddLuaGlobalEnum(MA_INVALID_DATA);
	AddLuaGlobalEnum(MA_TIMEOUT);
	AddLuaGlobalEnum(MA_NO_NETWORK);
	AddLuaGlobalEnum(MA_NOT_UNIQUE);
	AddLuaGlobalEnum(MA_NOT_SOCKET);
	AddLuaGlobalEnum(MA_NO_ADDRESS);
	AddLuaGlobalEnum(MA_BAD_PROTOCOL);
	AddLuaGlobalEnum(MA_PROTOCOL_UNAVAILABLE);
	AddLuaGlobalEnum(MA_PROTOCOL_NOT_SUPPORTED);
	AddLuaGlobalEnum(MA_PROTOCOL_FAMILY_NOT_SUPPORTED);
	AddLuaGlobalEnum(MA_ADDRESS_FAMILY_NOT_SUPPORTED);
	AddLuaGlobalEnum(MA_SOCKET_NOT_SUPPORTED);
	AddLuaGlobalEnum(MA_CONNECTION_RESET);
	AddLuaGlobalEnum(MA_ALREADY_CONNECTED);
	AddLuaGlobalEnum(MA_NOT_CONNECTED);
	AddLuaGlobalEnum(MA_CONNECTION_REFUSED);
	AddLuaGlobalEnum(MA_NO_HOST);
	AddLuaGlobalEnum(MA_IN_PROGRESS);
	AddLuaGlobalEnum(MA_CANCELLED);
	AddLuaGlobalEnum(MA_MEMORY_ALREADY_MAPPED);
	AddLuaGlobalEnum(MA_CRC_MISMATCH);
	AddLuaGlobalEnum(MA_FORMAT_NOT_SUPPORTED);
	AddLuaGlobalEnum(MA_DEVICE_TYPE_NOT_SUPPORTED);
	AddLuaGlobalEnum(MA_SHARE_MODE_NOT_SUPPORTED);
	AddLuaGlobalEnum(MA_NO_BACKEND);
	AddLuaGlobalEnum(MA_NO_DEVICE);
	AddLuaGlobalEnum(MA_API_NOT_FOUND);
	AddLuaGlobalEnum(MA_INVALID_DEVICE_CONFIG);
	AddLuaGlobalEnum(MA_LOOP);
	AddLuaGlobalEnum(MA_BACKEND_NOT_ENABLED);
	AddLuaGlobalEnum(MA_DEVICE_NOT_INITIALIZED);
	AddLuaGlobalEnum(MA_DEVICE_ALREADY_INITIALIZED);
	AddLuaGlobalEnum(MA_DEVICE_NOT_STARTED);
	AddLuaGlobalEnum(MA_DEVICE_NOT_STOPPED);
	AddLuaGlobalEnum(MA_FAILED_TO_INIT_BACKEND);
	AddLuaGlobalEnum(MA_FAILED_TO_OPEN_BACKEND_DEVICE);
	AddLuaGlobalEnum(MA_FAILED_TO_START_BACKEND_DEVICE);
	AddLuaGlobalEnum(MA_FAILED_TO_STOP_BACKEND_DEVICE);

	AddLuaGlobalEnum(ma_pan_mode_balance);
	AddLuaGlobalEnum(ma_pan_mode_pan);

	AddLuaGlobalEnum(ma_positioning_absolute);
	AddLuaGlobalEnum(ma_positioning_relative);

	lua_createtable(L, 0, 45);

	AddSoundFunction(LuaFN_ma_sound_init_from_file, "sound_init_from_file");

	AddMAFunction(LuaFN_ma_sound_start, "sound_start");
	AddMAFunction(LuaFN_ma_sound_stop, "sound_stop");

	AddMAFunction(LuaFN_ma_sound_is_playing, "sound_is_playing");
	AddMAFunction(LuaFN_ma_sound_is_looping, "sound_is_looping");
	AddMAFunction(LuaFN_ma_sound_is_spatialization_enabled, "sound_is_spatialization_enabled");

	AddMAFunction(LuaFN_ma_sound_set_attenuation_model, "sound_set_attenuation_model");
	AddMAFunction(LuaFN_ma_sound_set_cone, "sound_set_cone");
	AddMAFunction(LuaFN_ma_sound_set_direction, "sound_set_direction");
	AddMAFunction(LuaFN_ma_sound_set_directional_attenuation_factor, "sound_set_directional_attenuation_factor");
	AddMAFunction(LuaFN_ma_sound_set_doppler_factor, "sound_set_doppler_factor");
	AddMAFunction(LuaFN_ma_sound_set_fade_in_milliseconds, "sound_set_fade_in_milliseconds");
	AddMAFunction(LuaFN_ma_sound_set_fade_start_in_milliseconds, "sound_set_fade_start_in_milliseconds");
	AddMAFunction(LuaFN_ma_sound_set_looping, "sound_set_looping");
	AddMAFunction(LuaFN_ma_sound_set_max_distance, "sound_set_max_distance");
	AddMAFunction(LuaFN_ma_sound_set_max_gain, "sound_set_max_gain");
	AddMAFunction(LuaFN_ma_sound_set_pan, "sound_set_pan");
	AddMAFunction(LuaFN_ma_sound_set_pan_mode, "sound_set_pan_mode");
	AddMAFunction(LuaFN_ma_sound_set_pinned_listener_index, "sound_set_pinned_listener_index");
	AddMAFunction(LuaFN_ma_sound_set_pitch, "sound_set_pitch");
	AddMAFunction(LuaFN_ma_sound_set_position, "sound_set_position");
	AddMAFunction(LuaFN_ma_sound_set_positioning, "sound_set_positioning");
	AddMAFunction(LuaFN_ma_sound_set_rolloff, "sound_set_rolloff");
	AddMAFunction(LuaFN_ma_sound_set_spatialization_enabled, "sound_set_spatialization_enabled");
	AddMAFunction(LuaFN_ma_sound_set_start_time_in_milliseconds, "sound_set_start_time_in_milliseconds");
	AddMAFunction(LuaFN_ma_sound_set_stop_time_in_milliseconds, "sound_set_stop_time_in_milliseconds");
	AddMAFunction(LuaFN_ma_sound_set_stop_time_with_fade_in_milliseconds, "sound_set_stop_time_with_fade_in_milliseconds");
	AddMAFunction(LuaFN_ma_sound_set_velocity, "sound_set_velocity");
	AddMAFunction(LuaFN_ma_sound_set_volume, "sound_set_volume");

	AddMAFunction(LuaFN_ma_sound_get_attenuation_model, "sound_get_attenuation_model");
	AddMAFunction(LuaFN_ma_sound_get_cone, "sound_get_cone");
	AddMAFunction(LuaFN_ma_sound_get_direction, "sound_get_direction");
	AddMAFunction(LuaFN_ma_sound_get_directional_attenuation_factor, "sound_get_directional_attenuation_factor");
	AddMAFunction(LuaFN_ma_sound_get_doppler_factor, "sound_get_doppler_factor");
	AddMAFunction(LuaFN_ma_sound_get_max_distance, "sound_get_max_distance");
	AddMAFunction(LuaFN_ma_sound_get_max_gain, "sound_get_max_gain");
	AddMAFunction(LuaFN_ma_sound_get_pan, "sound_get_pan");
	AddMAFunction(LuaFN_ma_sound_get_pan_mode, "sound_get_pan_mode");
	AddMAFunction(LuaFN_ma_sound_get_pinned_listener_index, "sound_get_pinned_listener_index");
	AddMAFunction(LuaFN_ma_sound_get_pitch, "sound_get_pitch");
	AddMAFunction(LuaFN_ma_sound_get_position, "sound_get_position");
	AddMAFunction(LuaFN_ma_sound_get_positioning, "sound_get_positioning");
	AddMAFunction(LuaFN_ma_sound_get_rolloff, "sound_get_rolloff");
	AddMAFunction(LuaFN_ma_sound_get_velocity, "sound_get_velocity");
	AddMAFunction(LuaFN_ma_sound_get_volume, "sound_get_volume");

	lua_setglobal(L, "ma");
}