#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <AL/al.h>
#include <AL/alc.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
#include <unistd.h> // for usleep
#endif
#define NUM_BUFFERS 4
#define BUFFER_SIZE 16384

// Play a midi generated on the fly via wildmidi
// The quality of wildmidi generations seems better than timidity
#include "wildmidi_lib.h"

#include <string>

static const std::string defaultMidiFilename = "assets/bburg14a.mid";
static const std::string wildmidiConfigFilename = 
#if defined(__EMSCRIPTEN__)
  "assets/wildmidi.cfg";
#else
  "assets/linux-wildmidi.cfg";
#endif

midi *midi_ptr = NULL;
int8_t midiSampleBuffer[BUFFER_SIZE];

unsigned int wildMidiFillBuffer(midi* midi_ptr, int8_t* output_buffer, uint32_t num_samples)
{
  int res = WildMidi_GetOutput(midi_ptr, output_buffer, num_samples);
  if (res <= 0)
  {
    fprintf(stderr, "Could not read samples frm wildmidi.\n");
  }
  return res;
}

ALCdevice* device = NULL;
ALCcontext* context = NULL;
// Audio source state.
unsigned char* data = NULL;
unsigned int channels = 2;
unsigned int frequency = 44100;
unsigned int bits = 16;
ALenum format = AL_FORMAT_STEREO16;
ALuint source = 0;

/*
* main sample loop
*/
void iter()
{

  ALuint buffer = 0;
  ALint buffersProcessed = 0;
  ALint buffersWereQueued = 0;
  ALint buffersQueued = 0;
  ALint state;
  alGetSourcei(source, AL_BUFFERS_PROCESSED, &buffersProcessed);
  while (buffersProcessed--) {

    // unqueue the old buffer and validate the queue length
    alGetSourcei(source, AL_BUFFERS_QUEUED, &buffersWereQueued);
    alSourceUnqueueBuffers(source, 1, &buffer);
    assert(alGetError() == AL_NO_ERROR);
    int len = BUFFER_SIZE;

    alGetSourcei(source, AL_BUFFERS_QUEUED, &buffersQueued);
    assert(buffersQueued == buffersWereQueued - 1);
    // queue the new buffer and validate the queue length
    buffersWereQueued = buffersQueued;

    WildMidi_GetOutput(midi_ptr, midiSampleBuffer, len);
    
    alBufferData(buffer, format, midiSampleBuffer, len, frequency);
    
    alSourceQueueBuffers(source, 1, &buffer);
    assert(alGetError() == AL_NO_ERROR);
    alGetSourcei(source, AL_BUFFERS_QUEUED, &buffersQueued);
    assert(buffersQueued == buffersWereQueued + 1);
    // make sure it's still playing
    alGetSourcei(source, AL_SOURCE_STATE, &state);
    assert(state == AL_PLAYING);
  }
  // Exit once we've processed the entire clip.
  if (false) {
#ifdef __EMSCRIPTEN__
    printf("Cancelling emscripten main loop because offset >= size");
    emscripten_cancel_main_loop();
#endif
    exit(0);
  }
}
int main(int argc, char* argv[]) {

  // first argument is midi file to play
  // if no args use default
  std::string midiFileName = defaultMidiFilename;
  if(argc > 1)
  {
    midiFileName = argv[1];
  }
  printf("Playing midi file: %s\n", midiFileName.c_str());

  #if 0
  // use midifile lib to parse contents for visualization
  // would be nice if wildmidi had this functionality exposed but it doesn't
  MidiFile midifile(options.getArg(1));
  stringstream notes;
  int minpitch = -1;
  int maxpitch = -1;
  getMinMaxPitch(midifile, minpitch, maxpitch);
  #endif

  //
  // Setup the AL context.
  //
  device = alcOpenDevice(NULL);
  context = alcCreateContext(device, NULL);
  alcMakeContextCurrent(context);
 
  long libraryver = WildMidi_GetVersion();
  unsigned int rate = frequency; //44100;//32072;
  unsigned mixer_options = WM_MO_REVERB | WM_MO_ENHANCED_RESAMPLING;
  //mixer_options = WM_MO_STRIPSILENCE;
  printf("Initializing libWildMidi %ld.%ld.%ld\n\n",
                      (libraryver>>16) & 255,
                      (libraryver>> 8) & 255,
                      (libraryver    ) & 255);
  if (WildMidi_Init(wildmidiConfigFilename.c_str(), rate, mixer_options) == -1) {
      fprintf(stderr, "%s\n", WildMidi_GetError());
      WildMidi_ClearError();
      return (1);
  }

  uint8_t master_volume = 100;
  WildMidi_MasterVolume(master_volume);

  // open our midi file
  printf("Playing %s\n", midiFileName.c_str());


  char * ret_err = NULL;
  midi_ptr = WildMidi_Open(midiFileName.c_str());
  if (midi_ptr == NULL) {
    ret_err = WildMidi_GetError();
    printf(" Skipping: %s\n",ret_err);
    // TODO: bail
  }

  // show some midi info
    //struct _WM_Info *wm_info;
  struct _WM_Info *wm_info;
  wm_info = WildMidi_GetInfo(midi_ptr);

  uint32_t apr_mins = wm_info->approx_total_samples / (rate * 60);
  uint32_t apr_secs = (wm_info->approx_total_samples % (rate * 60)) / rate;

  mixer_options = wm_info->mixer_options;
  char modes[5];
  modes[0] = (mixer_options & WM_MO_LOG_VOLUME)? 'l' : ' ';
  modes[1] = (mixer_options & WM_MO_REVERB)? 'r' : ' ';
  modes[2] = (mixer_options & WM_MO_ENHANCED_RESAMPLING)? 'e' : ' ';
  modes[3] = ' ';
  modes[4] = '\0';

  printf("[Approx %2um %2us Total]\n", apr_mins, apr_secs);

  //
  // Seed the buffers with some initial data.
  //
  ALuint buffers[NUM_BUFFERS];
  alGenBuffers(NUM_BUFFERS, buffers);
  alGenSources(1, &source);
  ALint numBuffers = 0;
  while (numBuffers < NUM_BUFFERS) {
    int len = BUFFER_SIZE;

    WildMidi_GetOutput(midi_ptr, midiSampleBuffer, len);
    alBufferData(buffers[numBuffers], format, midiSampleBuffer, len, frequency);
    
    alSourceQueueBuffers(source, 1, &buffers[numBuffers]);
    assert(alGetError() == AL_NO_ERROR);
    numBuffers++;
  }
  //
  // Start playing the source.
  //
  alSourcePlay(source);
  ALint state;
  alGetSourcei(source, AL_SOURCE_STATE, &state);
  assert(state == AL_PLAYING);
  alGetSourcei(source, AL_BUFFERS_QUEUED, &numBuffers);
  assert(numBuffers == NUM_BUFFERS);
  //
  // Cycle and refill the buffers until we're done.
  //
#ifdef __EMSCRIPTEN__
  emscripten_set_main_loop(iter, 0, 0);
#else
  while (1) {
    iter();
    usleep(16);
  }
#endif
}
