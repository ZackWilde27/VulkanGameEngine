#pragma once

#include "miniaudio.h"
#include "engineTypes.h"

// The max number of sounds that can be playing at once
constexpr size_t MAX_SOUNDS = 32;

struct Sound
{
	ma_sound sound;
	float3* position;
	bool live;
	bool is3D;
	bool attachedToThing;
};

class SoundEngine
{
private:
	ma_engine engine;

	ma_uint32 soundCounter;
	Sound sounds[MAX_SOUNDS];

	Camera** observer;

	// Music is separate from sounds so it can never be interrupted by them
	ma_sound music;
	bool musicLive;

	Sound* NextSound();


	bool Setup3DSound(Sound* sound, const char* filename, ma_uint32 flags, float3* position, ma_attenuation_model attenuationModel, float minDistance, float maxDistance);

	void UpdateSoundPosition(Sound* sound);

public:
	SoundEngine(Camera** observer);
	~SoundEngine();

	// To play spatial audio properly, it has to update the positions of the sounds relative to the camera
	// miniaudio has two positioning modes, absolute and relative, so I think there's a built-in way to do this but I don't know how so manually it is
	void PerFrame();

	// Plays an audio file once and destroys it, you can have as many of these playing at once as you want (in theory)
	// The supported formats are .wav, .mp3, and .flac
	void PlaySimple2D(const char* filename);
	// Plays a looping audio file, this one takes up one of 32 slots so if you play too many at once it has a chance of interrupting another sound or being interrupted
	// The supported formats are .wav, .mp3, and .flac
	Sound* PlayLooping2D(const char* filename);
	// Plays an audio file with every parameter available, this one takes up one of 32 slots so if you play too many at once it has a chance of interrupting or being interrupted
	// The supported formats are .wav, .mp3, and .flac
	Sound* PlayComplex2D(const char* filename, ma_uint64 delayMilliseconds=0, ma_uint64 playTimeMilliseconds=0, float volume=1.0, float pitch=1.0, float pan=1.0, ma_bool32 loop=false);

	Sound* PlaySimple3D(const char* filename, float3& position, ma_attenuation_model attenuationModel, float minDistance, float maxDistance);
	Sound* PlayLooping3D(const char* filename, float3& position, ma_attenuation_model attenuationModel, float minDistance, float maxDistance);
	Sound* PlayComplex3D(const char* filename, float3& position, ma_attenuation_model attenuationModel, float minDistance, float maxDistance, ma_uint64 delayMilliseconds=0, ma_uint64 playTimeMilliseconds=0, float volume=1.0, float pitch=1.0, float pan=1.0, ma_bool32 loop=false);

	Sound* AttachSoundToThingSimple(const char* filename, Thing* thing, ma_attenuation_model attenuationModel, float minDistance, float maxDistance);
	Sound* AttachSoundToThingLooping(const char* filename, Thing* thing, ma_attenuation_model attenuationModel, float minDistance, float maxDistance);
	Sound* AttachSoundToThingComplex(const char* filename, Thing* thing, ma_attenuation_model attenuationModel, float minDistance, float maxDistance, ma_uint64 delayMilliseconds=0, ma_uint64 playTimeMilliseconds=0, float volume=1.0, float pitch=1.0, float pan=1.0, ma_bool32 loop=false);

	void StopSound(Sound* sound);

	

	// Stops every sound currently playing
	void StopAllSounds();

	// Music is separate from the sounds so it won't ever be interrupted
	// Unless music is already playing, then it'll interrupt it to play the new one
	void PlayMusic(const char* filename, ma_bool32 loop);
	// This will destroy the sound object. If all you want to do is pause it, use PauseMusic()
	void StopMusic();
	void PauseMusic();
	void ResumeMusic();

	void FadeOutMusic(ma_uint64 fadeTime);

	// Stops everything, sounds and music
	void StopAll();

	inline void IncrementSoundCounter();

	ma_result InitFromFile(const char* file, ma_uint32 flags, ma_sound* pSound);
};
