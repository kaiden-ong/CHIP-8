#include <iostream>
#include <filesystem>
#include <string>
#include <SDL2/SDL.h>
using namespace std;

const int width = 64;
const int height = 32;
const int scale = 1024 / 64;

typedef struct {
  bool running;
  uint8_t ram[0xFFF]; // ram
  bool display[4096]; // screen display res
  uint16_t stack[12]; // subroutine stack
  uint16_t *stack_pointer;
  uint8_t V[16]; // registers
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

  for (int i = 0; i < sizeof(font); i++) {
    chip_8->ram[i] = font[i];
  }

  for (int i = 0; i < 64 * 32; i++) {
    chip_8->display[i] = 0;
  }

  for (int i = 0; i < sizeof(chip_8->keypad); i++) {
    chip_8->keypad[i] = 0;
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

   if (fread(&chip_8->ram[entry], rom_size, 1, rom) != 1) {
        SDL_Log("Could not read Rom file %s into CHIP8 memory\n", 
                rom_name);
        return false;
    }
  fclose(rom);

  chip_8->running = true;
  chip_8->PC = entry;
  chip_8->I = 0;
  chip_8->delayTimer = 0;
  chip_8->soundTimer = 0;
  chip_8->stack_pointer = &chip_8->stack[0];
  chip_8->romName = rom_name;

  return true;
};

void decode_and_execute(uint16_t opcode, chip_8_t *chip_8) {
  uint16_t NNN = opcode & 0x0FFF;
  uint8_t NN = opcode & 0x00FF;
  uint8_t N = opcode & 0x000F;
  uint8_t X = (opcode & 0x0F00) >> 8;
  uint8_t Y = (opcode & 0x00F0) >> 4;

  uint8_t x_coord = chip_8->V[X] % width;
  uint8_t y_coord = chip_8->V[Y] % height;
  const uint8_t original_x = x_coord;
  uint8_t bcd = chip_8->V[X];
  bool carry;

  // printf("Address: 0x%04X, Opcode: 0x%04X Desc: ", chip_8->PC - 2, opcode);

  // // printf("Address: 0x%04x, Opcode: 0x%04x \n", chip_8->PC-2, opcode);
  switch (opcode & 0xF000) {
    case 0x0000:
      switch (opcode & 0x00FF) {
        // case 0x0nnn ignored by modern interpreters
        // 00E0 - CLS
        case 0x00E0:
          for (int i = 0; i < 64 * 32; i++) {
            chip_8->display[i] = 0;
          }
          // printf("Clear screen\n");
          // cout << "Opcode 0x" << hex << opcode << endl;
          break;
        // 00EE - RET
        case 0x00EE:
          chip_8->PC = *--chip_8->stack_pointer;
          // printf("Return from subroutine to address 0x%04X\n", *(chip_8->stack_pointer - 1));
          break;
        default:
          // cout << "Invalid opcode 0x" << hex << opcode << endl;
          break;
      }
      break;
    case 0x1000:
      // 1nnn - JP addr
      chip_8->PC = opcode & 0x0FFF;
      // printf("Jump to address NNN (0x%04X)\n", NNN);
      break;
    case 0x2000:
      // 2nnn - CALL addr
      *chip_8->stack_pointer++ = chip_8->PC;
      chip_8->PC = NNN;
      // printf("Call subroutine at NNN (0x%04X)\n", NNN);
      break;
    case 0x3000:
      // 3xkk - SE Vx, byte
      if (chip_8->V[X] == NN) {
        chip_8->PC += 2;
      }
      // printf("Check if V%X (0x%02X) == NN (0x%02X), skip next instruction if true\n", X, chip_8->V[X], NN);
      break;
    case 0x4000:
      // 4xkk - SNE Vx, byte
      if (chip_8->V[X] != NN) {
        chip_8->PC += 2;
      }
      // printf("Check if V%X (0x%02X) != NN (0x%02X), skip next instruction if true\n", X, chip_8->V[X], NN);
      break;
    case 0x5000:
      // 5xy0 - SE Vx, Vy
      if (chip_8->V[X] == chip_8->V[Y]) {
        chip_8->PC += 2;
      }
      // printf("Check if V%X (0x%02X) == V%X (0x%02X), skip next instruction if true\n", X, chip_8->V[X],  Y, chip_8->V[Y]);
      break;
    case 0x6000:
      // 6xkk - LD Vx, byte
      chip_8->V[X] = NN;
      // printf("Set register V%X = NN (0x%02X)\n", X, NN);
      break;
    case 0x7000:
      // 7xkk - ADD Vx, byte
      // printf("Set register V%X (0x%02X) += NN (0x%02X). Result: 0x%02X\n", X, chip_8->V[X], NN, chip_8->V[X] + NN);
      chip_8->V[X] += NN;
      break;
    case 0x8000:
      switch (N) {
        case 0:
          // 8xy0 - LD Vx, Vy
          chip_8->V[X] = chip_8->V[Y];
          // printf("Set register V%X = V%X (0x%02X)\n", X, Y, chip_8->V[Y]);
          break;
        case 1:
          // 8xy1 - OR Vx, Vy
          chip_8->V[X] |= chip_8->V[Y];
          // printf("Set register V%X (0x%02X) |= V%X (0x%02X); Result: 0x%02X\n", X, chip_8->V[X], Y, chip_8->V[Y], chip_8->V[X] | chip_8->V[Y]);
          break;
        case 2:
        // 8xy2 - AND Vx, Vy
          chip_8->V[X] &= chip_8->V[Y];
          // printf("Set register V%X (0x%02X) &= V%X (0x%02X); Result: 0x%02X\n", X, chip_8->V[X], Y, chip_8->V[Y], chip_8->V[X] & chip_8->V[Y]);
          break;
        case 3:
          // 8xy3 - XOR Vx, Vy
          chip_8->V[X] ^= chip_8->V[Y];
          // printf("Set register V%X (0x%02X) ^= V%X (0x%02X); Result: 0x%02X\n", X, chip_8->V[X], Y, chip_8->V[Y], chip_8->V[X] ^ chip_8->V[Y]);
          break;
        case 4:
          // 8xy4 - ADD Vx, Vy
          carry = ((uint16_t)(chip_8->V[X] + chip_8->V[Y]) > 255);
          chip_8->V[X] += chip_8->V[Y];
          chip_8->V[0xF] = carry;
          // printf("Set register V%X (0x%02X) += V%X (0x%02X), VF = 1 if carry; Result: 0x%02X, VF = %X\n", X, chip_8->V[X], Y, chip_8->V[Y], chip_8->V[X] + chip_8->V[Y], ((uint16_t)(chip_8->V[X] + chip_8->V[Y]) > 255));
          break;
        case 5:
          // 8xy5 - SUB Vx, Vy
          carry = chip_8->V[X] >= chip_8->V[Y];
          chip_8->V[X] -= chip_8->V[Y];
          chip_8->V[0xF] = carry;
          // printf("Set register V%X (0x%02X) -= V%X (0x%02X), VF = 1 if no borrow; Result: 0x%02X, VF = %X\n", X, chip_8->V[X], Y, chip_8->V[Y], chip_8->V[X] - chip_8->V[Y], (chip_8->V[Y] <= chip_8->V[X]));
          break;
        case 6:
          // 8xy6 - SHR Vx {, Vy}
          carry = chip_8->V[X] & 0x1;
          chip_8->V[X] = chip_8->V[Y];
          chip_8->V[X] >>= 1; 
          chip_8->V[0xF] = carry;
          // printf("Set register V%X (0x%02X) >>= 1, VF = shifted off bit (%X); Result: 0x%02X\n", X, chip_8->V[X], chip_8->V[X] & 1, chip_8->V[X] >> 1);
          break;
        case 7:
          // 8xy7 - SUBN Vx, Vy
          carry = chip_8->V[Y] >= chip_8->V[X];
          chip_8->V[X] = chip_8->V[Y] - chip_8->V[X];
          chip_8->V[0xF] = carry;
          // printf("Set register V%X = V%X (0x%02X) - V%X (0x%02X), VF = 1 if no borrow; Result: 0x%02X, VF = %X\n", X, Y, chip_8->V[Y], X, chip_8->V[X], chip_8->V[Y] - chip_8->V[X], (chip_8->V[X] <= chip_8->V[Y]));
          break;
        case 0xE:
        // 8xyE - SHL Vx {, Vy}
          carry = (chip_8->V[X] & 0x80) >> 7;
          chip_8->V[X] = chip_8->V[Y];
          chip_8->V[X] <<= 1;
          chip_8->V[0xF] = carry;
          // printf("Set register V%X (0x%02X) <<= 1, VF = shifted off bit (%X); Result: 0x%02X\n", X, chip_8->V[X], (chip_8->V[X] & 0x80) >> 7, chip_8->V[X] << 1);
          break;
      }
      break;
    case 0x9000:
      // 9xy0 - SNE Vx, Vy
      if (chip_8->V[X] != chip_8->V[Y]) {
        chip_8->PC += 2;
      }
      // printf("Check if V%X (0x%02X) != V%X (0x%02X), skip next instruction if true\n", X, chip_8->V[X],  Y, chip_8->V[Y]);
      break;
    case 0xA000:
      // Annn - LD I, addr
      chip_8->I = NNN;
      // printf("Set I to NNN (0x%04X)\n", NNN);
      break;
    case 0xB000:
      // Bnnn - JP V0, addr
      chip_8->PC = NNN + chip_8->V[0];
      // printf("Set PC to V0 (0x%02X) + NNN (0x%04X); Result PC = 0x%04X\n", chip_8->V[0], NNN, chip_8->V[0] + NNN);
      break;
    case 0xC000:
      // Cxkk - RND Vx, byte
      chip_8->V[X] = (rand() % 256) & NN; 
      // printf("Set V%X = rand() %% 256 & NN (0x%02X)\n", X, NN);
      break;
    case 0xD000:
      // Dxyn - DRW Vx, Vy, nibble
      chip_8->V[0xF] = 0;
      for (uint8_t i = 0; i < N; i++) {
        x_coord = original_x;
        const uint8_t sprite_data = chip_8->ram[chip_8->I + i];
        for (uint8_t j = 7; j != 255; j--) {
          const bool sprite_pixel = (sprite_data & (1 << j));
          bool *display_pixel = &chip_8->display[y_coord * width + x_coord];
          if (sprite_pixel && *display_pixel) {
            chip_8->V[0xF] = 1;
          }
          *display_pixel ^= sprite_pixel;

          if (++x_coord >= width) { break; }
        }
        if (++y_coord >= height) { break; }
      }
      // printf("Draw N (%u) height sprite at coords V%X (0x%02X), V%X (0x%02X) from memory location I (0x%04X). Set VF = 1 if any pixels are turned off.\n", N, X, chip_8->V[X], Y, chip_8->V[Y], chip_8->I);
      break;
    case 0xE000:
      switch (opcode & 0x00FF) {
        case 0x009E:
          // Ex9E - SKP Vx
          if (chip_8->keypad[chip_8->V[X]]) {
            chip_8->PC += 2;
          }
          // printf("Skip next instruction if key in V%X (0x%02X) is pressed; Keypad value: %d\n", X, chip_8->V[X], chip_8->keypad[chip_8->V[X]]);
          break;
        case 0x00A1:
          // ExA1 - SKNP Vx
          if (!chip_8->keypad[chip_8->V[X]]) {
            chip_8->PC += 2;
          }
          // printf("Skip next instruction if key in V%X (0x%02X) is not pressed; Keypad value: %d\n", X, chip_8->V[X], chip_8->keypad[chip_8->V[X]]);
          break;
        default:
          // cout << "Invalid opcode 0x" << hex << opcode << endl;
          break;
      }
      break;
    case 0xF000:
      switch (opcode & 0x00FF) {
        case 0x0007:
          chip_8->V[X] = chip_8->delayTimer;
          // printf("Set V%X = delay timer value (0x%02X)\n", X, chip_8->delayTimer);
          break;
        // case 0x0A:
        case 0x0015:
          chip_8->delayTimer = chip_8->V[X];
          // printf("Set delay timer value = V%X (0x%02X)\n", X, chip_8->V[X]);
          break;
        case 0x0018:
          chip_8->soundTimer = chip_8->V[X];
          // printf("Set sound timer value = V%X (0x%02X)\n", X, chip_8->V[X]);
          break;
        case 0x001E:
          chip_8->I += chip_8->V[X];
          // printf("I (0x%04X) += V%X (0x%02X); Result (I): 0x%04X\n", chip_8->I, X, chip_8->V[X], chip_8->I + chip_8->V[X]);
          break;
        case 0x0029:
          chip_8->I = chip_8->V[X] * 5;
          // printf("Set I to sprite location in memory for character in V%X (0x%02X). Result(VX*5) = (0x%02X)\n", X, chip_8->V[X], chip_8->V[X] * 5);
          break;
        case 0x0033:
          chip_8->ram[chip_8->I+2] = bcd % 10;
          bcd /= 10;
          chip_8->ram[chip_8->I+1] = bcd % 10;
          bcd /= 10;
          chip_8->ram[chip_8->I] = bcd;
          break;
          // printf("Store BCD representation of V%X (0x%02X) at memory from I (0x%04X)\n", X, chip_8->V[X], chip_8->I);
        case 0x0055:
          for (uint8_t i = 0; i <= X; i++) {
            chip_8->ram[chip_8->I++] = chip_8->V[i];
          }
          // printf("Register dump V0-V%X (0x%02X) inclusive at memory from I (0x%04X)\n", X, chip_8->V[X], chip_8->I);
          break;
        case 0x0065:
          for (uint8_t i = 0; i <= X; i++) {
            chip_8->V[i] = chip_8->ram[chip_8->I++];
          }
          // printf("Register load V0-V%X (0x%02X) inclusive at memory from I (0x%04X)\n", X, chip_8->V[X], chip_8->I);
          break;
      }
      break;
    default:
      // printf("Unimplemented Opcode.\n");
      break;
  }
}

void emulate_instructions(chip_8_t *chip_8) {
  uint16_t opcode = chip_8->ram[chip_8->PC] << 8 | chip_8->ram[chip_8->PC + 1];
  chip_8->PC += 2;
  decode_and_execute(opcode, chip_8);
  // cout << chip_8->PC << endl;
} 

void handle_input(chip_8_t *chip_8) {
  SDL_Event windowEvent;
  while (SDL_PollEvent(&windowEvent)) {
    switch (windowEvent.type) {
      case SDL_QUIT:
        chip_8->running = false; 
        break;
      case SDL_KEYDOWN:
        switch (windowEvent.key.keysym.scancode) {
          case SDL_SCANCODE_EQUALS:
           // reset CHIP-8
            init_chip_8(chip_8, chip_8->romName);
            break;
          case SDL_SCANCODE_1: chip_8->keypad[0x1] = true; break;
          case SDL_SCANCODE_2: chip_8->keypad[0x2] = true; break;
          case SDL_SCANCODE_3: chip_8->keypad[0x3] = true; break;
          case SDL_SCANCODE_4: chip_8->keypad[0xC] = true; break;
          case SDL_SCANCODE_Q: chip_8->keypad[0x4] = true; break;
          case SDL_SCANCODE_W: chip_8->keypad[0x5] = true; break;
          case SDL_SCANCODE_E: chip_8->keypad[0x6] = true; break;
          case SDL_SCANCODE_R: chip_8->keypad[0xD] = true; break;
          case SDL_SCANCODE_A: chip_8->keypad[0x7] = true; break;
          case SDL_SCANCODE_S: chip_8->keypad[0x8] = true; break;
          case SDL_SCANCODE_D: chip_8->keypad[0x9] = true; break;
          case SDL_SCANCODE_F: chip_8->keypad[0xE] = true; break;
          case SDL_SCANCODE_Z: chip_8->keypad[0xA] = true; break;
          case SDL_SCANCODE_X: chip_8->keypad[0x0] = true; break;
          case SDL_SCANCODE_C: chip_8->keypad[0xB] = true; break;
          case SDL_SCANCODE_V: chip_8->keypad[0xF] = true; break;
          default: break;
        };
        break;
      case SDL_KEYUP:
        switch (windowEvent.key.keysym.scancode) {
          case SDL_SCANCODE_2: chip_8->keypad[0x2] = false; break;
          case SDL_SCANCODE_3: chip_8->keypad[0x3] = false; break;
          case SDL_SCANCODE_1: chip_8->keypad[0x1] = false; break;
          case SDL_SCANCODE_4: chip_8->keypad[0xC] = false; break;
          case SDL_SCANCODE_Q: chip_8->keypad[0x4] = false; break;
          case SDL_SCANCODE_W: chip_8->keypad[0x5] = false; break;
          case SDL_SCANCODE_E: chip_8->keypad[0x6] = false; break;
          case SDL_SCANCODE_R: chip_8->keypad[0xD] = false; break;
          case SDL_SCANCODE_A: chip_8->keypad[0x7] = false; break;
          case SDL_SCANCODE_S: chip_8->keypad[0x8] = false; break;
          case SDL_SCANCODE_D: chip_8->keypad[0x9] = false; break;
          case SDL_SCANCODE_F: chip_8->keypad[0xE] = false; break;
          case SDL_SCANCODE_Z: chip_8->keypad[0xA] = false; break;
          case SDL_SCANCODE_X: chip_8->keypad[0x0] = false; break;
          case SDL_SCANCODE_C: chip_8->keypad[0xB] = false; break;
          case SDL_SCANCODE_V: chip_8->keypad[0xF] = false; break;
          default: break;
        }
        break;
      default:
        break;
    }
  }
}

void update_screen(SDL_Renderer* renderer, const chip_8_t chip_8) {
  SDL_Rect rect = {0, 0, scale, scale};
  for (uint32_t i = 0; i < sizeof chip_8.display; i++) {
    rect.x = (i % width) * scale;
    rect.y = (i / width) * scale;
    if (chip_8.display[i]) {
      SDL_SetRenderDrawColor(renderer, 0, 0, 0, 1);
      SDL_RenderFillRect(renderer, &rect);
    } else {
      SDL_SetRenderDrawColor(renderer, 173, 216, 230, 255);
      SDL_RenderFillRect(renderer, &rect);
    }
  }
  SDL_RenderPresent(renderer);
}

void update_timers(chip_8_t *chip_8) {
  if (chip_8->delayTimer > 0) {
    chip_8->delayTimer--;
  }
}

void final_cleanup(SDL_Renderer* renderer, SDL_Window* window) {
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
}

int main(int argc, char* argv[]) {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    cout << "SDL_Init Error: " << SDL_GetError() << endl;
    return 1;
  }
  SDL_Window *window = SDL_CreateWindow("CHIP-8 EMU", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width * scale, height * scale, SDL_WINDOW_ALLOW_HIGHDPI);
  if (window == NULL) {
    cout << "Couldn't create window:" << SDL_GetError() << endl;
    SDL_Quit();
    return 1;
  }

  SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if (!renderer) {
      cout << "Renderer could not be created! SDL_Error: " << SDL_GetError() << endl;
      SDL_DestroyWindow(window);
      SDL_Quit();
      return 1;
  }

  chip_8_t chip_8;
  const char *rom_name = argv[1];
  if (!init_chip_8(&chip_8, rom_name)) {
      cout << "Failed to initialize CHIP-8" << endl;
      final_cleanup(renderer, window);
      return 1;
  }

  while (chip_8.running) {
    handle_input(&chip_8);
    SDL_SetRenderDrawColor(renderer, 173, 216, 230, 255); // Light blue
    SDL_RenderClear(renderer);
    for (uint32_t i = 0; i < 600 / 60; i++) {
      emulate_instructions(&chip_8);
    }
    update_screen(renderer, chip_8);
    update_timers(&chip_8);
    // SDL_Delay(1); // about 60 fps: 1000ms / 16ms delay
  }

  final_cleanup(renderer, window);
  exit(EXIT_SUCCESS);
}