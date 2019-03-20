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
#define BUFFER_SIZE 1470*10

// Play a midi generated on the fly via wildmidi
// The quality of wildmidi generations seems better than timidity
#define USE_WILDMIDI
#if defined(USE_WILDMIDI)
#include "wildmidi_lib.h"
#endif

#include <string>


ALCdevice* device = NULL;
ALCcontext* context = NULL;
// Audio source state.
unsigned char* data = NULL;
unsigned int size = 0;
unsigned int offset = 0;
unsigned int channels = 0;
unsigned int frequency = 0;
unsigned int bits = 0;
ALenum format = 0;
ALuint source = 0;
void iter() {
  ALuint buffer = 0;
  ALint buffersProcessed = 0;
  ALint buffersWereQueued = 0;
  ALint buffersQueued = 0;
  ALint state;
  alGetSourcei(source, AL_BUFFERS_PROCESSED, &buffersProcessed);
  while (offset < size && buffersProcessed--) {
    // unqueue the old buffer and validate the queue length
    alGetSourcei(source, AL_BUFFERS_QUEUED, &buffersWereQueued);
    alSourceUnqueueBuffers(source, 1, &buffer);
    assert(alGetError() == AL_NO_ERROR);
    int len = size - offset;
    if (len > BUFFER_SIZE) {
      len = BUFFER_SIZE;
    }
    alGetSourcei(source, AL_BUFFERS_QUEUED, &buffersQueued);
    assert(buffersQueued == buffersWereQueued - 1);
    // queue the new buffer and validate the queue length
    buffersWereQueued = buffersQueued;
    alBufferData(buffer, format, &data[offset], len, frequency);
    alSourceQueueBuffers(source, 1, &buffer);
    assert(alGetError() == AL_NO_ERROR);
    alGetSourcei(source, AL_BUFFERS_QUEUED, &buffersQueued);
    assert(buffersQueued == buffersWereQueued + 1);
    // make sure it's still playing
    alGetSourcei(source, AL_SOURCE_STATE, &state);
    assert(state == AL_PLAYING);
    offset += len;
  }
  // Exit once we've processed the entire clip.
  if (offset >= size) {
#ifdef __EMSCRIPTEN__
    int result = 0;
    //REPORT_RESULT();
#endif
    exit(0);
  }
}
int main(int argc, char* argv[]) {
  //
  // Setup the AL context.
  //
  device = alcOpenDevice(NULL);
  context = alcCreateContext(device, NULL);
  alcMakeContextCurrent(context);
  //
  // Read in the audio sample.
  //

  FILE* fp = fopen("assets/Bburg1_2.mid.wav", "rb");

  fseek(fp, 0, SEEK_END);
  size = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  data = (unsigned char*)malloc(size);
  fread(data, size, 1, fp);
  fclose(fp);
  offset = 12; // ignore the RIFF header
  offset += 8; // ignore the fmt header
  offset += 2; // ignore the format type
  channels = data[offset + 1] << 8;
  channels |= data[offset];
  offset += 2;
  printf("Channels: %u\n", channels);
  frequency = data[offset + 3] << 24;
  frequency |= data[offset + 2] << 16;
  frequency |= data[offset + 1] << 8;
  frequency |= data[offset];
  offset += 4;
  printf("Frequency: %u\n", frequency);
  offset += 6; // ignore block size and bps
  bits = data[offset + 1] << 8;
  bits |= data[offset];
  offset += 2;
  printf("Bits: %u\n", bits);
  format = 0;
  if (bits == 8) {
    if (channels == 1) {
      format = AL_FORMAT_MONO8;
    } else if (channels == 2) {
      format = AL_FORMAT_STEREO8;
    }
  } else if (bits == 16) {
    if (channels == 1) {
      format = AL_FORMAT_MONO16;
    } else if (channels == 2) {
      format = AL_FORMAT_STEREO16;
    }
  }
  offset += 8; // ignore the data chunk

  #if defined(USE_WILDMIDI)
  std::string config_file("assets/linux-wildmidi.cfg");
  long libraryver = WildMidi_GetVersion();
  unsigned int rate = 32072;
  unsigned mixer_options = WM_MO_REVERB | WM_MO_ENHANCED_RESAMPLING;
  printf("Initializing libWildMidi %ld.%ld.%ld\n\n",
                      (libraryver>>16) & 255,
                      (libraryver>> 8) & 255,
                      (libraryver    ) & 255);
  if (WildMidi_Init(config_file.c_str(), rate, mixer_options) == -1) {
      fprintf(stderr, "%s\n", WildMidi_GetError());
      WildMidi_ClearError();
      return (1);
  }

  uint8_t master_volume = 100;
  WildMidi_MasterVolume(master_volume);

  std::string midiFileName("assets/Bburg1_2.mid");

  // open our midi file
  printf("Playing %s\n", midiFileName.c_str());

  void *midi_ptr = NULL;
  char * ret_err = NULL;
  midi_ptr = WildMidi_Open(midiFileName.c_str());
  if (midi_ptr == NULL) {
    ret_err = WildMidi_GetError();
    printf(" Skipping: %s\n",ret_err);
    // TODO: bail
  }

  #endif


  //
  // Seed the buffers with some initial data.
  //
  ALuint buffers[NUM_BUFFERS];
  alGenBuffers(NUM_BUFFERS, buffers);
  alGenSources(1, &source);
  ALint numBuffers = 0;
  while (numBuffers < NUM_BUFFERS && offset < size) {
    int len = size - offset;
    if (len > BUFFER_SIZE) {
      len = BUFFER_SIZE;
    }
    alBufferData(buffers[numBuffers], format, &data[offset], len, frequency);
    alSourceQueueBuffers(source, 1, &buffers[numBuffers]);
    assert(alGetError() == AL_NO_ERROR);
    offset += len;
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
