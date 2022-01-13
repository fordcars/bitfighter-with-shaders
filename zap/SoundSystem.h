//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef SOUNDSYSTEM_H_
#define SOUNDSYSTEM_H_

#include "SoundSystemEnums.h"
#include "Timer.h"

#ifdef ZAP_DEDICATED
#  define BF_NO_AUDIO
#endif

#ifndef BF_NO_AUDIO
#  include <alc.h>
#  include <al.h>
#  include <AL/alure.h>
#else
   class alureStream;
#endif

#include "tnlTypes.h"
#include "tnlVector.h"

#include <string>


// forward declarations
typedef unsigned int ALuint;
typedef float ALfloat;

namespace TNL {
   template <class T> class RefPtr;
   class ByteBuffer;
   typedef RefPtr<ByteBuffer> ByteBufferPtr;
};

using namespace TNL;
using namespace std;

namespace Zap {

// Forward declarations
class Point;
class SoundEffect;
typedef RefPtr<SoundEffect> SFXHandle;


enum MusicCommand {
   MusicCommandNone,
   MusicCommandStop,     // Instantly stop music
   MusicCommandPlay,     // Play/resume music (no fading)
   MusicCommandPause,    // Pause music
   MusicCommandFadeIn,   // Start and fade in music
   MusicCommandFadeOut,  // Fade out and stop music
};

enum MusicState {
   MusicStateNone,
   MusicStateFadingIn,
   MusicStateFadingOut,
   MusicStatePlaying,
   MusicStateStopped,
   MusicStatePaused,
   // Interim states
   MusicStateLoading,
   MusicStateStopping,
   MusicStatePausing,
   MusicStateResuming,
};

struct MusicData
{
   MusicLocation currentLocation;   // Music location (in menus, in game, etc..)
   MusicLocation previousLocation;
   MusicCommand command;            // Command to target a different music state
   MusicState state;                // Current music state
   ALfloat volume;
   ALuint source;
   alureStream* stream;
};


class SoundSystem
{
private:
   static const S32 NumMusicStreamBuffers = 3;
   static const S32 MusicChunkSize = 250000;
   static const S32 NumVoiceChatBuffers = 32;
   static const S32 NumSamples = 16;

   // Sound Effect functions
   static void updateGain(SFXHandle& effect, F32 sfxVolLevel, F32 voiceVolLevel);
   static void playOnSource(SFXHandle& effect, F32 sfxVol, F32 voiceVol);

   static void music_end_callback(void* userData, ALuint source);
   static void menu_music_end_callback(void* userData, ALuint source);

   static MusicData mMusicData;

   static string mMusicDir;
   static string mMenuMusicFile;
   static string mCreditsMusicFile;

   static bool mMenuMusicValid;
   static bool mGameMusicValid;
   static bool mCreditsMusicValid;

   static Vector<string> mGameMusicList;
   static S32 mCurrentlyPlayingIndex;

   static Timer mMusicFadeTimer;
   static const U32 MusicFadeOutDelay = 500;
   static const U32 MusicFadeInDelay = 1000;

   static bool musicSystemValid();

public:
   SoundSystem();
   virtual ~SoundSystem();

   // General functions
   static void init(const string &sfxDir, const string &musicDir, float musicVol);
   static void shutdown();
   static void setListenerParams(const Point &position, const Point &velocity);
   static void processAudio(U32 timeDelta, F32 sfxVol, F32 musicVol, F32 voiceVol, MusicLocation);  // Client version
   static void processAudio(F32 sfxVol);                                                            // Server version

   // Sound Effect functions
   static void processSoundEffects(F32 sfxVol, F32 voiceVol);
   static SFXHandle playSoundEffect(U32 profileIndex, F32 gain = 1.0f);
   static SFXHandle playSoundEffect(U32 profileIndex, const Point &position);
   static SFXHandle playSoundEffect(U32 profileIndex, const Point &position, const Point &velocity, F32 gain = 1.0f);
   static void playSoundEffect(const SFXHandle &effect);
   static SFXHandle playRecordedBuffer(ByteBufferPtr p, F32 gain);
   static void stopSoundEffect(SFXHandle &effect);
   static void unqueueBuffers(S32 sourceIndex);
   static void setMovementParams(SFXHandle& effect, const Point &position, const Point &velocity);
   static void updateMovementParams(SFXHandle& effect);

   // Voice Chat functions
   static void processVoiceChat();
   static void queueVoiceChatBuffer(const SFXHandle &effect, ByteBufferPtr p);
   static bool startRecording();
   static void captureSamples(ByteBufferPtr sampleBuffer);
   static void stopRecording();

   // Music functions
   static void processMusic(U32 timeDelta, F32 musicVol, MusicLocation musicLocation);
   static void playMusic();
   static void stopMusic();
   static void pauseMusic();
   static void resumeMusic();
   static void fadeInMusic();
   static void fadeOutMusic();
   static void playNextTrack();
   static void playPrevTrack();

   static bool isMusicPlaying();
};

}

#endif /* SOUNDSYSTEM_H_ */
