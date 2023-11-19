/**************************************************************************
  This is a library for several Adafruit displays based on ST77* drivers.

  Works with the Adafruit TFT Gizmo
    ----> http://www.adafruit.com/products/4367

  Check out the links above for our tutorials and wiring diagrams.

  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.
  MIT license, all text above must be included in any redistribution
 **************************************************************************/

#include <Adafruit_CircuitPlayground.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
#include <SPI.h>
#include "pitches.h"

//#define DEBUG_BUTTONS
//#define DEBUG_MOVING
//#define DEBUG_SEGMENTS

// Because of the limited number of pins available on the Circuit Playground Boards
// Software SPI is used
#define TFT_CS        0
#define TFT_RST       -1 // Or set to -1 and connect to Arduino RESET pin
#define TFT_DC        1
#define TFT_BACKLIGHT PIN_A3 // Display backlight pin

// You will need to use Adafruit's CircuitPlayground Express Board Definition
// for Gizmos rather than the Arduino version since there are additional SPI
// ports exposed.
#if (SPI_INTERFACES_COUNT == 1)
  SPIClass* spi = &SPI;
#else
  SPIClass* spi = &SPI1;
#endif

// OPTION 1 (recommended) is to use the HARDWARE SPI pins, which are unique
// to each board and not reassignable.
Adafruit_ST7789 tft = Adafruit_ST7789(spi, TFT_CS, TFT_DC, TFT_RST);

// OPTION 2 lets you interface the display using ANY TWO or THREE PINS,
// tradeoff being that performance is not as fast as hardware SPI above.
//#define TFT_MOSI      PIN_WIRE_SDA  // Data out
//#define TFT_SCLK      PIN_WIRE_SCL  // Clock out
//Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

#define PIXELS_X 240
#define PIXELS_Y 240

#define CELL_PIXELS 10

#define CELLS_X (PIXELS_X/CELL_PIXELS)
#define CELLS_Y (PIXELS_Y/CELL_PIXELS)
//#define MAX_SEGMENTS 100
#define MAX_SEGMENTS 999

enum direction_e {
  dir_north = 0,
  dir_east,
  dir_south,
  dir_west,
  direction_count,
};

struct speaker_note_s {
  int tone_Hz; // =1 = end of sequence
  int note_length; // 4=4th note, 8=8th note
};

#if 0 // shave and a haircut
struct speaker_note_s notes_intro[] = {
  { NOTE_C4, 4, },
  { NOTE_G3, 8, },
  { NOTE_G3, 8, },
  { NOTE_A3, 4, },
  { NOTE_G3, 4, },
  { 0,       4, }, // silent
  { NOTE_B3, 4, },
  { NOTE_C4, 4, },
  { -1,     -1, },
};
#endif
struct speaker_note_s notes_intro1[] = {
  { NOTE_C4, 4, },
  { NOTE_G3, 8, },
  { NOTE_G3, 8, },
  { -1,     -1, },
};

struct speaker_note_s notes_intro2[] = {
  { NOTE_A3, 4, },
  { NOTE_G3, 4, },
  { 0,       4, }, // silent
  { -1,     -1, },
};

struct speaker_note_s notes_intro3[] = {
  { NOTE_B3, 4, },
  { NOTE_C4, 4, },
  { -1,     -1, },
};

struct speaker_note_s notes_pill[] = {
  { 5500,   10, },
  { -1,     -1, },
};

struct speaker_note_s notes_stop[] = {
  { 500,    10, },
  { -1,     -1, },
};

struct speaker_note_s notes_go[] = {
  { 2000,    10, },
  { 3500,    10, },
  { -1,     -1, },
};

enum game_sound_e {
  sound_intro1 = 0,
  sound_intro2,
  sound_intro3,
  sound_pill,
  sound_go,
  sound_stop,
  sound_die, // TBD
  game_sound_count,
};

struct sound_notes_s {
  int                    which_sound; // -1 = end of list
  struct speaker_note_s *sound_notes;
};

struct sound_notes_s game_sounds[] {
  { sound_intro1, notes_intro1, },
  { sound_intro2, notes_intro2, },
  { sound_intro3, notes_intro3, },
  { sound_pill,  notes_pill, },
  { sound_go,    notes_go, },
  { sound_stop,  notes_stop, },
  { -1,  NULL, },
};

// when the counter reaches the target, we will do an event.
struct game_event_s {
  int counter;
  int target;
};

float p = 3.1415926;

// input button state
bool left_btn;
bool right_btn;
bool left_old;
bool right_old;

// game state
struct game_cell_s {
  int x, y;
};
struct game_cell_s player_cell;
int player_dir; // -1 to turn left, +1 to turn right: so use int for this math, not enum
struct game_cell_s pill_cell; // pill the worm can eat go grow longer at this x,y
struct game_cell_s missed_cell; // player did not get the pill. hide it.

struct game_cell_s tail_cell;
// do the worm `-_/~
struct game_cell_s worm_cells[MAX_SEGMENTS];
int worm_cell_count;
//int worm_delay;

//struct game_event_s pill_event;

// print ":(x, y) " coordinates of worm_cell
// Helper for print_worm()
void print_cell(struct game_cell_s *worm_cell) {
    Serial.print(":(");
    Serial.print(worm_cell->x);
    Serial.print(",");
    Serial.print(worm_cell->y);
    Serial.print(") ");
}

// Debug output of tail_cell, worm_cells[] x, y coordinates
void print_worm(void) {
#ifdef DEBUG_SEGMENTS
  int cell_idx;

  Serial.print("tail_cell");
  print_cell(&tail_cell);
  Serial.print(" ");
  Serial.print(worm_cell_count);
  Serial.print(" worm_cells: ");
  for (cell_idx = 0; cell_idx < worm_cell_count; cell_idx++) {
    Serial.print(cell_idx);
    print_cell(&worm_cells[cell_idx]);
  }
  Serial.println();
#endif
}

void start_sound(int which_sound) {
  int sound_idx;
  bool more_sounds = true;
  bool found_sound = false;
  int note_idx;
  bool more_notes = true;
  int tone_Hz;
  int note_length;
  int note_ms;
  int pause_ms;

  CircuitPlayground.speaker.enable(false);
  if (CircuitPlayground.slideSwitch()) {
    // Sound is off
    return;
  }
  // Sound is on
  CircuitPlayground.speaker.enable(true);
  sound_idx = 0;
  do {
    if (game_sounds[sound_idx].which_sound < 0) { // end of list
      more_sounds = false;
      continue;
    }
    if (game_sounds[sound_idx].which_sound == which_sound) {
      found_sound = true;
      continue;
    }
    sound_idx++;
  } while (more_sounds && !found_sound);
  if (!found_sound) {
    return;
  }

  note_idx = 0;
  do {
    tone_Hz     = game_sounds[sound_idx].sound_notes[note_idx].tone_Hz;
    note_length = game_sounds[sound_idx].sound_notes[note_idx].note_length;

    if (tone_Hz < 0) {
      more_notes = false;
      continue;
    }

    if (CircuitPlayground.slideSwitch()) {
      // Sound is off
      return;
    }

    note_ms = 1000 / note_length;
    CircuitPlayground.playTone(tone_Hz, note_ms);

    //pause_ms = note_ms * 1.30;
    //delay(pause_ms);

    note_idx++;
  } while (more_notes);
}

// initialize player and world state to starting conditions
void start_game(void) {
  int cell_idx;
  //worm_cell_count = 99; // worm starts long
  //worm_cell_count = 4; // worm starts short
  worm_cell_count = 1; // worm starts stubby

  // set where head of worm will be
  player_cell.x = CELLS_X/2;
  player_cell.y = CELLS_Y/2;
  player_dir = dir_east;

  // add worm's starting segments
  for (cell_idx = 0; cell_idx < worm_cell_count; cell_idx++) {
    worm_cells[cell_idx].x = player_cell.x-cell_idx-1;
    worm_cells[cell_idx].y = player_cell.y;
  }
  // not trying to erase a tail right now
  tail_cell.x = -1;
  tail_cell.y = -1;
  // no pill on screen yet
  pill_cell.x = -1;
  pill_cell.y = -1;
  missed_cell.x = -1;
  missed_cell.y = -1;
//  pill_event.counter = 0;
//  pill_event.target = 100;
  print_worm();
}

void walk_player(void) {
  int cell_idx;
  static bool did_walk = false;
  bool         do_walk = true;

  switch (player_dir) {
  case dir_east:
    //is_in_score
    player_cell.x++;
    break;
  case dir_west:
    player_cell.x--;
    break;
  case dir_north:
    player_cell.y--;
    break;
  case dir_south:
    player_cell.y++;
    break;
  default: // should not occur
    break;
  }

  // stay in cell bounds
  if (player_cell.x < 0) {
    player_cell.x = 0;
    do_walk = false; // no change to cells
  }
  if (player_cell.y < 0) {
    player_cell.y = 0;
    do_walk = false;
  }
  if (player_cell.x >= CELLS_X) {
    player_cell.x = CELLS_X-1;
    do_walk = false;
  }
  if (player_cell.y >= CELLS_Y) {
    player_cell.y = CELLS_Y-1;
    do_walk = false;
  }

  if (did_walk && !do_walk) {
    start_sound (sound_stop);
  }
  if (do_walk && !did_walk) {
    start_sound (sound_go);
  }

  if (do_walk) {
    if (player_cell.x == pill_cell.x &&
        player_cell.y == pill_cell.y) {
      pill_cell.x = -1;
      pill_cell.y = -1;
      start_sound (sound_pill);
      if (worm_cell_count < MAX_SEGMENTS) {
        worm_cell_count++;
      }
    }

    tail_cell.x = worm_cells[worm_cell_count-1].x;
    tail_cell.y = worm_cells[worm_cell_count-1].y;
    for (cell_idx = worm_cell_count-1; cell_idx >= 1; cell_idx--) {
      worm_cells[cell_idx].x = worm_cells[cell_idx-1].x;
      worm_cells[cell_idx].y = worm_cells[cell_idx-1].y;
    }
    worm_cells[0].x = player_cell.x;
    worm_cells[0].y = player_cell.y;
  } else { // don't try to erase tail anymore: not moving.
    tail_cell.x = -1;
    tail_cell.y = -1;
  }
  print_worm();

  did_walk = do_walk;
}

#define SCORE_WIDTH  70
#define SCORE_HEIGHT 28
bool is_in_score(struct game_cell_s *test_cell) {
  int x, y;

  x = (test_cell->x+1) * CELL_PIXELS;
  y = (test_cell->y  ) * CELL_PIXELS;
  if (x <= SCORE_WIDTH &&
      y <= SCORE_HEIGHT) {
        return true;
  }
  return false;
}

void draw_segment(struct game_cell_s *draw_cell, uint16_t color) {
  int x, y, radius;

  if (draw_cell->x < 0 || draw_cell->x >= CELLS_X ||
      draw_cell->y < 0 || draw_cell->y >= CELLS_Y ||
      is_in_score (draw_cell)) {
      return; // off-screen location, don't draw
  }
  radius = CELL_PIXELS/2;
  x = draw_cell->x * CELL_PIXELS + radius;
  y = draw_cell->y * CELL_PIXELS + radius;
  tft.fillCircle(x, y, radius, color);
}

void draw_game(void) {
  // draw score
  static int score_old = -1;

  if (score_old != worm_cell_count) { // score changed; redraw it
    // background
    tft.fillRect(0, 0, SCORE_WIDTH, SCORE_HEIGHT, ST77XX_BLACK);
    // number
    our_drawnum (worm_cell_count, ST77XX_GREEN);

    score_old = worm_cell_count; // save the new score
  }

  // worm head
  draw_segment (&worm_cells[0], ST77XX_GREEN);
  // erase old worm tail
  draw_segment (&tail_cell,     ST77XX_BLACK);

  // pill!
  draw_segment (&pill_cell,     ST77XX_YELLOW);

  // erase old pill
  draw_segment (&missed_cell,   ST77XX_BLACK);
  missed_cell.x = -1;
  missed_cell.y = -1;
}

void draw_banner (uint16_t color) {
  our_drawtext("Welcome to", -1, color);
  our_drawtext("Wormy!", 0, color);
}

void setup(void) {
  Serial.begin(9600);
  Serial.print(F("Hello! Welcome to wormy serial port!"));
  CircuitPlayground.begin();

  pinMode(TFT_BACKLIGHT, OUTPUT);
  digitalWrite(TFT_BACKLIGHT, HIGH); // Backlight on

  tft.init(PIXELS_X, PIXELS_Y);                // Init ST7789 240x240
  tft.setRotation(2);

  Serial.println(F("Initialized"));

  uint16_t time = millis();
  tft.fillScreen(ST77XX_BLACK);
  time = millis() - time;

  Serial.println(time, DEC);
  draw_banner (ST77XX_WHITE);
  start_sound (sound_intro1);
  draw_banner (ST77XX_MAGENTA);
  start_sound (sound_intro2);
  draw_banner (ST77XX_GREEN);
  start_sound (sound_intro3);
  delay(2000);
  tft.fillScreen(ST77XX_BLACK);
  start_game();
  draw_game();
}

void loop() {
  // we are looking at the screen side. so swap right and left.
  right_btn = CircuitPlayground.leftButton();
  left_btn = CircuitPlayground.rightButton();

#ifdef DEBUG_BUTTONS
  Serial.print("Left Button: ");
  if (left_btn) {
    Serial.print("DOWN");
  } else {
    Serial.print("  UP");
  }
  Serial.print("   Right Button: ");
  if (right_btn) {
    Serial.print("DOWN");
  } else {
    Serial.print("  UP");    
  }
  Serial.println();
#endif
  if (left_btn && !left_old) {
    player_dir = player_dir-1;
    if (player_dir < 0) {
      player_dir = direction_count-1; // kept turning left from first direction; now looking in last direction.
    }
  }
  if (right_btn && !right_old) {
    player_dir = player_dir+1;
    if (player_dir >= direction_count) {
      player_dir = 0; // kept turning right from last direction, now looking back in first direction again.
    }
  }
#ifdef DEBUG_MOVING
  Serial.print("player_cell.x: ");
  Serial.print(player_cell.x);
  Serial.print(", player_cell.y: ");
  Serial.println(player_cell.y);
#endif
  right_old = right_btn;
  left_old = left_btn;

  walk_player();

  if (pill_cell.x == -1 &&
      pill_cell.y == -1) {
    //pill_event.counter++;
    //if (pill_event.counter >= pill_event.target) {
      bool pill_ok;
      int cell_idx;

      //pill_event.counter = 0;
      // place a new pill, but not on the worm.
      do {
        if (pill_cell.x != -1 ||
            pill_cell.y != -1) {
            missed_cell.x = pill_cell.x;
            missed_cell.y = pill_cell.y;
        }
        pill_cell.x = random(0, CELLS_X);
        pill_cell.y = random(0, CELLS_Y);
        pill_ok = true;
        if (is_in_score(&pill_cell)) {
          pill_ok = false;
        }
        for (cell_idx = 0; cell_idx < worm_cell_count; cell_idx++) {
          if (worm_cells[cell_idx].x == pill_cell.x &&
              worm_cells[cell_idx].y == pill_cell.y) {
            pill_ok = false;
            break;
          }
        }
      } while (!pill_ok);
    //}
  } // end if pill_cell.x == -1, .y == -1
  draw_game();

  delay(100);
}

#define LINE_PIXELS 20
void our_drawtext(char *text, int line, uint16_t color) {
  //int char_count;

  //char_count = strlen(text);
  tft.setTextSize(3);
  tft.setCursor(10, PIXELS_Y/2 + line*24);
  tft.setTextColor(color);
  tft.setTextWrap(true);
  tft.print(text);
}

void our_drawnum(int num, uint16_t color) {
  tft.setTextSize(4);
  tft.setCursor(0, 0);
  tft.setTextColor(color);
  tft.setTextWrap(true);
  tft.print(num);
}