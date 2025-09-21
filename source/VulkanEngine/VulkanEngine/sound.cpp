#include "sound.h"
#include <iostream>

SoundEngine::SoundEngine(Camera** activeCamera)
{
	soundCounter = 0;
	musicLive = false;
	this->observer = activeCamera;

	memset(&music, 0, sizeof(ma_sound));
	memset(&sounds, 0, sizeof(Sound) * MAX_SOUNDS);

	if (ma_engine_init(NULL, &engine))
		throw std::runtime_error("Failed to init sound engine!");
}

SoundEngine::~SoundEngine()
{
	StopAll();
	ma_engine_uninit(&engine);
}

void SoundEngine::PlaySimple2D(const char* filename)
{
	ma_engine_play_sound(&engine, filename, NULL);
}

Sound* SoundEngine::PlayLooping2D(const char* filename)
{
	Sound* next = NextSound();

	next->is3D = false;

	if (ma_sound_init_from_file(&engine, filename, MA_SOUND_FLAG_NO_SPATIALIZATION | MA_SOUND_FLAG_NO_PITCH | MA_SOUND_FLAG_LOOPING, NULL, NULL, &next->sound) == MA_SUCCESS)
	{
		next->live = true;
		ma_sound_start(&next->sound);
	}

	return next;
}

static inline void SetupComplexSound(ma_sound* sound, ma_uint64 delayMilliseconds, ma_uint64 playTimeMilliseconds, float volume, float pitch, float pan, ma_bool32 loop)
{
	ma_sound_set_volume(sound, volume);
	ma_sound_set_pitch(sound, pitch);
	ma_sound_set_start_time_in_milliseconds(sound, delayMilliseconds);

	if (playTimeMilliseconds)
		ma_sound_set_stop_time_in_milliseconds(sound, delayMilliseconds + playTimeMilliseconds);

	ma_sound_set_pan_mode(sound, ma_pan_mode_pan);
	ma_sound_set_pan(sound, pan);

	ma_sound_set_looping(sound, loop);

	ma_sound_start(sound);
}

Sound* SoundEngine::PlayComplex2D(const char* filename, ma_uint64 delayMilliseconds, ma_uint64 playTimeMilliseconds, float volume, float pitch, float pan, ma_bool32 loop)
{
	Sound* next = NextSound();

	next->is3D = false;

	if (ma_sound_init_from_file(&engine, filename, MA_SOUND_FLAG_NO_SPATIALIZATION, NULL, NULL, &next->sound) == MA_SUCCESS)
	{
		next->live = true;
		SetupComplexSound(&next->sound, delayMilliseconds, playTimeMilliseconds, volume, pitch, pan, loop);
	}

	return next;
}

void SoundEngine::StopSound(Sound* sound)
{
	if (sound->live)
	{
		if (sound->is3D && !sound->attachedToThing)
		{
			free(sound->position);
			sound->position = NULL;
		}

		ma_sound_stop(&sound->sound);
		ma_sound_uninit(&sound->sound);
		sound->live = false;
	}
}

inline void SoundEngine::IncrementSoundCounter()
{
	++soundCounter %= MAX_SOUNDS;
}

Sound* SoundEngine::NextSound()
{
	IncrementSoundCounter();
	ma_uint32 oldSoundCounter = soundCounter;

	// When the next sound is live and playing, it'll go through the list trying to find one that is not playing
	// If the rest of them are also playing, only then will it stop the sound pre-maturely and replace it
	// This system makes it so it'll hopefully only stop the oldest sounds, if it ever has to
	while (sounds[soundCounter].live && ma_sound_is_playing(&sounds[soundCounter].sound))
	{
		IncrementSoundCounter();

		if (soundCounter == oldSoundCounter)
			break;
	}

	Sound* next = &sounds[soundCounter];

	StopSound(next);

	return next;
}

void SoundEngine::UpdateSoundPosition(Sound* sound)
{
	float4 relativePosition = (*observer)->viewMatrix * float4(*sound->position, 1);
	ma_sound_set_position(&sound->sound, relativePosition.x, relativePosition.y, relativePosition.z);
}

bool SoundEngine::Setup3DSound(Sound* sound, const char* filename,ma_uint32 flags, float3* position, ma_attenuation_model attenuationModel, float minDistance, float maxDistance)
{
	sound->position = position;

	bool succeeded = ma_sound_init_from_file(&engine, filename, flags, NULL, NULL, &sound->sound) == MA_SUCCESS;
	if (succeeded)
	{	
		sound->live = true;
		ma_sound_set_spatialization_enabled(&sound->sound, MA_TRUE);
		ma_sound_set_attenuation_model(&sound->sound, attenuationModel);
		ma_sound_set_min_distance(&sound->sound, minDistance);
		ma_sound_set_max_distance(&sound->sound, maxDistance);
		UpdateSoundPosition(sound);
		ma_sound_set_positioning(&sound->sound, ma_positioning_absolute);
	}

	return succeeded;
}

Sound* SoundEngine::PlaySimple3D(const char* filename, float3& position, ma_attenuation_model attenuationModel, float minDistance, float maxDistance)
{
	Sound* next = NextSound();

	next->is3D = true;
	next->attachedToThing = false;

	auto pos = NEW(float3);
	*pos = position;

	if (Setup3DSound(next, filename, MA_SOUND_FLAG_NO_PITCH, pos, attenuationModel, minDistance, maxDistance))
		ma_sound_start(&next->sound);

	return next;
}

Sound* SoundEngine::PlayLooping3D(const char* filename, float3& position, ma_attenuation_model attenuationModel, float minDistance, float maxDistance)
{
	Sound* next = NextSound();

	next->is3D = true;
	next->attachedToThing = false;

	auto pos = NEW(float3);
	*pos = position;

	if (Setup3DSound(next, filename, MA_SOUND_FLAG_NO_PITCH | MA_SOUND_FLAG_LOOPING, pos, attenuationModel, minDistance, maxDistance))
		ma_sound_start(&next->sound);

	return next;
}

Sound* SoundEngine::PlayComplex3D(const char* filename, float3& position, ma_attenuation_model attenuationModel, float minDistance, float maxDistance, ma_uint64 delayMilliseconds, ma_uint64 playTimeMilliseconds, float volume, float pitch, float pan, ma_bool32 loop)
{
	Sound* next = NextSound();

	next->is3D = true;
	next->attachedToThing = false;

	auto pos = NEW(float3);
	*pos = position;

	if (Setup3DSound(next, filename, 0, pos, attenuationModel, minDistance, maxDistance))
		SetupComplexSound(&next->sound, delayMilliseconds, playTimeMilliseconds, volume, pitch, pan, loop);

	return next;
}

Sound* SoundEngine::AttachSoundToThingSimple(const char* filename, Thing* thing, ma_attenuation_model attenuationModel, float minDistance, float maxDistance)
{
	Sound* next = NextSound();

	next->is3D = true;
	next->attachedToThing = true;

	if (Setup3DSound(next, filename, MA_SOUND_FLAG_NO_PITCH, &thing->position, attenuationModel, minDistance, maxDistance))
		ma_sound_start(&next->sound);

	return next;
}

Sound* SoundEngine::AttachSoundToThingLooping(const char* filename, Thing* thing, ma_attenuation_model attenuationModel, float minDistance, float maxDistance)
{
	Sound* next = NextSound();

	next->is3D = true;
	next->attachedToThing = true;

	if (Setup3DSound(next, filename, MA_SOUND_FLAG_NO_PITCH | MA_SOUND_FLAG_LOOPING, &thing->position, attenuationModel, minDistance, maxDistance))
		ma_sound_start(&next->sound);

	return next;
}

Sound* SoundEngine::AttachSoundToThingComplex(const char* filename, Thing* thing, ma_attenuation_model attenuationModel, float minDistance, float maxDistance, ma_uint64 delayMilliseconds, ma_uint64 playTimeMilliseconds, float volume, float pitch, float pan, ma_bool32 loop)
{
	Sound* next = NextSound();

	next->is3D = true;
	next->attachedToThing = true;

	if (Setup3DSound(next, filename, 0, &thing->position, attenuationModel, minDistance, maxDistance))
		SetupComplexSound(&next->sound, delayMilliseconds, playTimeMilliseconds, volume, pitch, pan, loop);

	return next;
}

void SoundEngine::FadeOutMusic(ma_uint64 fadeTime)
{
	ma_sound_stop_with_fade_in_milliseconds(&music, fadeTime);
}

void SoundEngine::StopAllSounds()
{
	for (size_t i = 0; i < MAX_SOUNDS; i++)
		StopSound(&sounds[i]);
}

void SoundEngine::PlayMusic(const char* filename, ma_bool32 loop)
{
	StopMusic();

	if (ma_sound_init_from_file(&engine, filename, 0, NULL, NULL, &music) == MA_SUCCESS)
	{
		musicLive = true;
		ma_sound_set_looping(&music, loop);
		ma_sound_start(&music);
	}
}

void SoundEngine::StopMusic()
{
	if (musicLive)
	{
		if (ma_sound_is_playing(&music))
			PauseMusic();

		ma_sound_uninit(&music);
	}

	musicLive = false;
}

void SoundEngine::PauseMusic()
{
	if (musicLive)
		ma_sound_stop(&music);
}

void SoundEngine::ResumeMusic()
{
	if (musicLive)
		ma_sound_start(&music);
}

void SoundEngine::StopAll()
{
	StopAllSounds();
	StopMusic();
}

void SoundEngine::PerFrame()
{
	for (size_t i = 0; i < MAX_SOUNDS; i++)
	{
		if (sounds[i].live)
		{
			if (sounds[i].is3D)
				UpdateSoundPosition(&sounds[i]);

			if (!ma_sound_is_playing(&sounds[i].sound))
				StopSound(&sounds[i]);
		}
	}
}

ma_result SoundEngine::InitFromFile(const char* file, ma_uint32 flags, ma_sound* pSound)
{
	return ma_sound_init_from_file(&engine, file, 0, NULL, NULL, pSound);
}