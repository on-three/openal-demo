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

void render(float t, float dt)
{
  // Set the color to cornflower blue and clear
  SDL_SetRenderDrawColor(renderer, 100, 149, 237, 255);
  SDL_RenderClear(renderer);

  SDL_Rect srcrect;
  srcrect.x = 1;
  srcrect.y = 0;
  srcrect.w = width - 1;
  srcrect.h = height;

  SDL_Rect destrect;
  destrect.x = 0;
  destrect.y = 0;
  destrect.w = width;// - 1;
  destrect.h = height;

  // copy the current background texture back to the render texture (but with 1 pixel left offset)
  /*   int SDL_RenderCopyEx(SDL_Renderer*          renderer,
                     SDL_Texture*           texture,
                     const SDL_Rect*        srcrect,
                     const SDL_Rect*        dstrect,
                     const double           angle,
                     const SDL_Point*       center,
                     const SDL_RendererFlip flip) */
	SDL_SetRenderTarget(renderer, textureBuffer);
	//SDL_RenderClear(renderer);
	//SDL_RenderCopy(renderer, texture, NULL, NULL);
  SDL_RenderCopyEx(renderer, texture, &srcrect, &destrect, 0.0, NULL, SDL_FLIP_NONE);


  // TODO: draw in new notes along the right edge
  #if 1
  static int i = 0;
  SDL_Rect drawRect;
  drawRect.x = 300;
  drawRect.y = 200 + i % 100;
  drawRect.w = 100;
  drawRect.h = 100;
  // Set render color to blue ( rect will be rendered in this color )
  SDL_SetRenderDrawColor( renderer, 0, 0, 255, 255 );
  SDL_RenderFillRect(renderer, &drawRect);
  i++;
  #endif

  //Detach the texture
	SDL_SetRenderTarget(renderer, NULL);

  // copy the render buffer back to our background texture
  SDL_SetRenderTarget(renderer, texture);
  SDL_RenderCopy(renderer, textureBuffer, NULL, NULL);
  SDL_SetRenderTarget(renderer, NULL);

  // Set the color to cornflower blue and clear
  SDL_SetRenderDrawColor(renderer, 100, 149, 237, 255);
  //SDL_RenderClear(renderer);

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

void mainLoop()
{
  iter();

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
  // TODO: feed with total elapsed time and delta time
  render(0.0f, 0.0f);
  #endif

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
  smf::MidiFile midifile(midiFileName);
  std::stringstream notes;
  int minpitch = -1;
  int maxpitch = -1;
  //getMinMaxPitch(midifile, minpitch, maxpitch);
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
