#include <iostream>
#include <SDL2/SDL.h>
using namespace std;

const int width = 1024;
const int height = 512;

typedef struct {
  bool running;
  uint8_t ram[0xFFF]; // ram
  bool display[64*32]; // screen display res
  uint16_t stack[12]; // subroutine stack
  uint8_t V[0x10]; // registers
  uint16_t I; // I register
  uint16_t PC; // Program Counter
  uint8_t delayTimer;
  uint8_t soundTimer;
  bool keypad[16];
  const char *romName;
} chip_8_t;

bool init_chip_8(chip_8_t *chip_8, const char rom_name[]) {
  const uint32_t entry = 0x200;
  const uint8_t font[] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
  };

  for (int i = 0; i < sizeof font; i++) {
    chip_8->ram[i] = font[i];
  }

  FILE *rom = fopen(rom_name, "rb");
  if (!rom) {
    SDL_Log("ROM FILE %s is invalid\n", rom_name);
    return false;
  }
  fseek(rom, 0, SEEK_END);
  const size_t rom_size = ftell(rom);
  const size_t max_size = sizeof chip_8->ram - entry;
  rewind(rom);

  if (rom_size > max_size) {
    SDL_Log("ROM FILE %s is too large\n", rom_name);
    return false;
  }

  if (fread(&chip_8->ram[entry], 1, rom_size, rom) != rom_size) {
    SDL_Log("Could not read ROM FILE %s into RAM\n", rom_name);
    return false;
  }

  fclose(rom);

  chip_8->running = true;
  chip_8->PC = entry;
  chip_8->I = 0;
  chip_8->delayTimer = 0;
  chip_8->soundTimer = 0;
  chip_8->romName = rom_name;

  return true;
};

void remap_keys(SDL_Scancode key, chip_8_t *chip_8, bool pressed) {
  const uint8_t keys[16] = {
    SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3, SDL_SCANCODE_4,
    SDL_SCANCODE_Q, SDL_SCANCODE_W, SDL_SCANCODE_E, SDL_SCANCODE_R,
    SDL_SCANCODE_A, SDL_SCANCODE_S, SDL_SCANCODE_D, SDL_SCANCODE_F,
    SDL_SCANCODE_Z, SDL_SCANCODE_X, SDL_SCANCODE_C, SDL_SCANCODE_V
  };

  for (int i = 0; i < 16; i++) {
    if (key == keys[i]) {
      chip_8->keypad[i] = pressed;
      break;
    }
  }
}

void final_cleanup(SDL_Renderer* renderer, SDL_Window* window) {
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
}

int main(int argc, char* argv[]) {
  SDL_Init(SDL_INIT_EVERYTHING);
  SDL_Window *window = SDL_CreateWindow("CHIP-8 EMU", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_ALLOW_HIGHDPI);
  if (window == NULL) {
    cout << "Couldn't create window:" << SDL_GetError() << endl;
    return 1;
  }

  SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if (!renderer) {
      cout << "Renderer could not be created! SDL_Error: " << SDL_GetError() << endl;
      return 1;
  }

  SDL_Event windowEvent;

  chip_8_t chip_8;
  const char *rom_name = argv[1];
  if (!init_chip_8(&chip_8, rom_name)) {
      cout << "Failed to initialize CHIP-8" << endl;
      final_cleanup(renderer, window);
      return 1;
  }

  while (chip_8.running) {
    while (SDL_PollEvent(&windowEvent)) {
      if (windowEvent.type == SDL_QUIT){ chip_8.running = false; }      
      else if (windowEvent.type == SDL_KEYDOWN) {
        remap_keys(windowEvent.key.keysym.scancode, &chip_8, true);
      } else if (windowEvent.type == SDL_KEYUP) {
        remap_keys(windowEvent.key.keysym.scancode, &chip_8, false);
      }
      SDL_SetRenderDrawColor(renderer, 173, 216, 230, 255); // Light blue
      SDL_RenderClear(renderer);
      SDL_RenderPresent(renderer);
      SDL_Delay(16); // about 60 fps: 1000ms / 16ms delay
    }
  }

  final_cleanup(renderer, window);
  exit(EXIT_SUCCESS);
}