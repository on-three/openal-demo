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


// visualization
#define VIZ
#if defined(VIZ)
#include "SDL.h"
#include "SDL_video.h"
// Midi visualization requires a separate parsing lib
#include "MidiFile.h"
#endif

#include <string>
#include <sstream>
#include <vector>

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
bool done = false;

#if defined(VIZ)

SDL_Window *window = nullptr;
SDL_Renderer *renderer = nullptr;
SDL_Texture *texture = nullptr;
SDL_Texture *textureBuffer = nullptr;
int width = 640;
int height = 480;

int createWindow()
{
  SDL_Init(SDL_INIT_VIDEO);              // Initialize SDL2

  // Create an application window with the following settings:
  window = SDL_CreateWindow(
      "An SDL2 window",                  // window title
      SDL_WINDOWPOS_UNDEFINED,           // initial x position
      SDL_WINDOWPOS_UNDEFINED,           // initial y position
      width,                               // width, in pixels
      height,                               // height, in pixels
      SDL_WINDOW_OPENGL                  // flags - see below
  );

  // Check that the window was successfully created
  if (window == NULL) {
      // In the case that the window could not be made...
      fprintf(stderr, "Could not create window: %s\n", SDL_GetError());
      return -1;
  }

  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if (renderer == nullptr)
  {
    fprintf(stderr, "Could not create a renderer: %s", SDL_GetError());
    return -1;
  }

  #if 0
  background = SDL_LoadBMP("hockeyrink.bmp");
  #else
  texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, width, height);
  if(texture == NULL)
  {
    fprintf(stderr, "Texture create error %s\n", SDL_GetError());
  }

  textureBuffer = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, width, height);
  if(textureBuffer == NULL)
  {
    fprintf(stderr, "Rendertexture create error %s\n", SDL_GetError());
  }

  #endif
  return 0;
}

int destroyWindow()
{
  // Close and destroy the window
  SDL_DestroyWindow(window);

  // Clean up
  SDL_Quit();

  return 0;
}

std::vector<int> getTrackHues(smf::MidiFile& midifile)
{
  static int unsigned temp = 10101; //seed 
  std::vector<int> output;

  #if 0
  printf("NumChannels: %d\n", midifile.size());
  #endif

  static std::vector<unsigned int> colors = {
    0xC0C0C0FF, // 	rgb(192, 192, 192) Silver
    0x808080FF, //	rgb(128, 128, 128) gray
    //0x000000FF, //	rgb(0, 0, 0) black
    0xFF0000FF, //	rgb(255, 0, 0) red
    0x800000FF, //	rgb(128, 0, 0) maroon
    0xFFFF00FF, //	rgb(255, 255, 0) yellow
    0x808000FF, //	rgb(128, 128, 0) olive
    0x00FF00FF, //	rgb(0, 255, 0) lime
    0x008000FF, //	rgb(0, 128, 0) green
    0x00FFFFFF, //	rgb(0, 255, 255) aqua
    0x008080FF, //	rgb(0, 128, 128) teal
    0x0000FFFF, //	rgb(0, 0, 255) blue
    0x000080FF, //	rgb(0, 0, 128) navy
    0xFF00FFFF, //	rgb(255, 0, 255)) fuchsia
    0x800080FF //	rgb(128, 0, 128)) purple
  };

  output.resize(midifile.size());
  for (int i = 0; i < midifile.size(); i++)
  {
    #if 0
    temp = temp*(i+i+1);
    temp = (temp^(0xffffff))>>2;
    #else
    int numColors = colors.size();
    temp = colors[i % numColors]; 
    #endif
    output[i] = temp;
  }
   return output;
}

std::vector<int> trackHues;

int base12ToBase7(int pitch) {
   int octave = pitch / 12;
   int chroma = pitch % 12;
   int output = 0;
   switch (chroma) {
      case  0: output = 0; break; // C
      case  1: output = 0; break; // C#
      case  2: output = 1; break; // D
      case  3: output = 2; break; // Eb
      case  4: output = 2; break; // E
      case  5: output = 3; break; // F
      case  6: output = 3; break; // F#
      case  7: output = 4; break; // G
      case  8: output = 4; break; // G#
      case  9: output = 5; break; // A
      case 10: output = 6; break; // Bb
      case 11: output = 6; break; // B
   }
   return output + 7 * octave;
}

void getMinMaxPitch(const smf::MidiFile& midifile, int& minpitch, int& maxpitch) {
   int key = 0;
   for (int i=0; i<midifile.size(); i++) {
      for (int j=0; j<midifile[i].size(); j++) {
         if (midifile[i][j].isNoteOn()) {
            key = midifile[i][j].getP1();
            if ((minpitch < 0) || (minpitch > key)) {
               minpitch = key;
            }
            if ((maxpitch < 0) || (maxpitch < key)) {
               maxpitch = key;
            }
         }
      }
   }

   if (true) { //grandQ) {
      if (minpitch > 40) {
         minpitch = 40;
      }
      if (maxpitch < 80) {
         maxpitch = 80;
      }
   }
   if (true) {
      minpitch = base12ToBase7(minpitch);
      maxpitch = base12ToBase7(maxpitch);
   }
}

int minPitch = 0;
int maxPitch = 0;

int getCurrentNote(smf::MidiFile& midifile, int channel, float elapsedTime)
{
  // inefficient linear search for current note.
  for (int j=0; j< midifile[channel].size(); j++) {
    if (!midifile[channel][j].isNoteOn()) {
      continue;
    }
    /*if (!drumQ) {
      if (midifile[channel][j].getChannel() == 0x09) {
        continue;
      }
    } */
    int tickstart, tickend, tickdur;
    double starttime, endtime, duration;
    int height = 1;
    tickstart  = midifile[channel][j].tick;
    starttime  = midifile[channel][j].seconds;
    if (midifile[channel][j].isLinked()) {
      tickdur  = midifile[channel][j].getTickDuration();
      tickend  = tickstart + tickdur;
      duration = midifile[channel][j].getDurationInSeconds();
      endtime  = starttime + duration;
   } else {
      tickdur = 0;
      tickend = tickstart;
      duration = 0.0;
      endtime = starttime;
   }
   int pitch    = midifile[channel][j].getP1();
   
   if (midifile[channel][j].getChannel() == 9) {
      //pitch = PercussionMap[pitch];
   }
   
   int pitch12  = pitch;
   int pitch7 = base12ToBase7(pitch12);
/*    if (diatonicQ) {
      pitch = base12ToBase7(pitch);
   } */
   int velocity = midifile[channel][j].getP2();
   int _channel  = midifile[channel][j].getChannel();  // 0-offset

    // Is this note currently on? if so return its pitch
    bool on = elapsedTime >= starttime && elapsedTime <= endtime;
    if(on)
    {
      //printf("On: pitch: %d\n", pitch12);
      return pitch7;
    }
  }
  return 0;
}

void drawNote(int currentNote, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{

  const static int noteHeight = 10;
  
  // scale notes according to min/max pitch and screen height
  if(currentNote <= 0) return;
  float scaledPitch = static_cast<float>(currentNote) / static_cast<float>(maxPitch + noteHeight - minPitch);
  //printf("scaledpitch: %f\n", scaledPitch);
  int y = height - height * scaledPitch;

  SDL_Rect drawRect;
  //drawRect.x = width - 400;
  drawRect.x = width - 1;
  drawRect.y = y;
  drawRect.w = 1;
  drawRect.h = noteHeight;
  // Set render color to blue ( rect will be rendered in this color )
  SDL_SetRenderDrawColor( renderer, r, g, b, a );
  SDL_RenderFillRect(renderer, &drawRect);
}

int hasNotes(smf::MidiEventList& eventlist) {
   for (int i=0; i<eventlist.size(); i++) {
      if (eventlist[i].isNoteOn()) {
         if (false) { //drumQ) {
            return 1;
         } else if (eventlist[i].getChannel() != 0x09) {
            return 1;
         }
      }
   }
   return 0;
}

void render(float t, float dt, smf::MidiFile& midifile)
{
  SDL_SetRenderTarget(renderer, textureBuffer);
  
  #if 1
  // clear to black
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderClear(renderer);
  #endif

  SDL_Rect srcrect;
  srcrect.x = 1;
  srcrect.y = 0;
  srcrect.w = width - 1;
  srcrect.h = height;
  SDL_Rect destrect;
  destrect.x = 0;
  destrect.y = 0;
  destrect.w = width - 1;
  destrect.h = height;
  SDL_RenderCopyEx(renderer, texture, &srcrect, &destrect, 0, NULL, SDL_FLIP_NONE);

  // draw the actual notes:
  for (int i=midifile.size()-1; i>=0; i--) {
    if (!hasNotes(midifile[i])) {
        continue;
    }
    int track = i;

    int currentNote = getCurrentNote(midifile, track, t);
    
    if(currentNote > 0)
    {
      int hue = trackHues[track];

      Uint8 _r = hue >> 24;
      Uint8 _g = hue >> 16;
      Uint8 _b = hue >> 8;
      Uint8 _a = 255;

      drawNote(currentNote, _r, _g, _b, _a);
    }

  }

  SDL_SetRenderTarget(renderer, NULL);

  // copy the render buffer back to our background texture
  SDL_SetRenderTarget(renderer, texture);
  
  SDL_RenderCopy(renderer, textureBuffer, NULL, NULL);

  // debug clear to yellow
  #if 0
  SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
  SDL_RenderClear(renderer);
  #endif

  SDL_SetRenderTarget(renderer, NULL);
  
  // Set the color to cornflower blue and clear
  SDL_SetRenderDrawColor(renderer, 100, 149, 237, 255);
  SDL_RenderClear(renderer);

  // draw in our visualization texture which should make the BG 100% not visible
  SDL_RenderCopy(renderer, texture, NULL, NULL);

  // Show the renderer contents
  SDL_RenderPresent(renderer);
}

#endif

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

  int samplesGenerated = 0;

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

    samplesGenerated += WildMidi_GetOutput(midi_ptr, midiSampleBuffer, len);
    
    alBufferData(buffer, format, midiSampleBuffer, len, frequency);
    
    alSourceQueueBuffers(source, 1, &buffer);
    assert(alGetError() == AL_NO_ERROR);
    alGetSourcei(source, AL_BUFFERS_QUEUED, &buffersQueued);
    assert(buffersQueued == buffersWereQueued + 1);
    // make sure it's still playing
    alGetSourcei(source, AL_SOURCE_STATE, &state);
    assert(state == AL_PLAYING);
  }
  // TODO: Exit once we've processed the entire clip.
  //if (samplesGenerated == 0) {
  if(false) {
    //done = true;

#ifdef __EMSCRIPTEN__
    printf("Cancelling emscripten main loop because offset >= size");
    emscripten_cancel_main_loop();
#endif
  }
}

Uint32  current_time = 0; //SDL_GetTicks();
Uint32 last_update_time = 0;
Uint32  delta_time = current_time - last_update_time;
float last_elapsed_time = 0;

smf::MidiFile midifile;

void mainLoop()
{
  Uint32  current_time = SDL_GetTicks();
  Uint32  delta_time = current_time - last_update_time;
  //static Uint32 freq = SDL_GetPerformanceFrequency();
  //float elapsed = static_cast<float>(current_time * 1000) / static_cast<float>(freq);
  float elapsed = static_cast<float>(current_time) / 1000.0f;
  float dt = elapsed - last_elapsed_time;

  #if 0
  printf("current: %d last: %d dt: %d freq:%d elapsed:%f\n", current_time, last_update_time, delta_time, freq, elapsed);
  #endif

  if(delta_time > 0)
  {
    #if 0
    printf("dt: %f\n", dt);
    #endif
    iter();

    #if defined(VIZ)
    render(elapsed, dt, midifile);
    #endif
  }

  #if defined(VIZ)
  // Get the next event
  SDL_Event event;
  if (SDL_PollEvent(&event))
  {
    if (event.type == SDL_QUIT)
    {
      // Break out of the loop on quit
      //break;
      done = true;
      return;
    }
  }
  #endif

  last_update_time = current_time;
  last_elapsed_time = elapsed;

  #if !defined(__EMSCRIPTEN__)
  usleep(16);
  #endif
  
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

  #if defined(VIZ)
  // use midifile lib to parse contents for visualization
  // would be nice if wildmidi had this functionality exposed but it doesn't
  midifile  = smf::MidiFile(midiFileName);
  std::stringstream notes;
  int minpitch = -1;
  int maxpitch = -1;
  //getMinMaxPitch(midifile, minpitch, maxpitch);
  midifile.linkNotePairs();    // first link note-ons to note-offs
  midifile.doTimeAnalysis();   // then create ticks to seconds mapping
  trackHues = getTrackHues(midifile);
  getMinMaxPitch(midifile, minPitch, maxPitch);
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
  emscripten_set_main_loop(mainLoop, 0, 0);
#endif

  #if defined(VIZ)
  createWindow();
  #endif

#if !defined(__EMSCRIPTEN__)
  while (!done) {
    mainLoop();
  }

  #if defined(VIZ)
  destroyWindow();
  #endif

#endif
}
