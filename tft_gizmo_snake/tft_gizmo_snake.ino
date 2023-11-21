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
//#define DEBUG_ACCEL
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

//#define CELL_PIXELS 10
#define CELL_PIXELS 24

#define CELLS_X (PIXELS_X/CELL_PIXELS)
#define CELLS_Y (PIXELS_Y/CELL_PIXELS)
//#define MAX_SEGMENTS 100
#define MAX_SEGMENTS 999

#define COS30 0.866 // cos (30 degrees)
#define SIN30 0.6   // sin (30 degrees)
#define COS60 0.6   // cos (60 degrees)
#define SIN60 0.866 // sin (60 degrees)

enum color_e {
  color_backgr = ST77XX_BLACK,
  
  color_intro1 = ST77XX_WHITE,
  color_intro2 = ST77XX_MAGENTA,
  color_intro3 = ST77XX_GREEN,

  color_worm = ST77XX_GREEN, // aka the snake
  color_pill = ST77XX_YELLOW,
  color_score = ST77XX_GREEN,
  color_count,
};

enum direction_e {
  dir_north = 0,
  dir_east,
  dir_south,
  dir_west,
  direction_count,
};

enum cell_img_e {
  mouth_north = dir_north,
  mouth_east = dir_east,
  mouth_south = dir_south,
  mouth_west = dir_west,
  body_north,
  body_east,
  body_south,
  body_west,
  tail_north,
  tail_east,
  tail_south,
  tail_west,
  img_pill,
  img_erase,
  cell_img_count,
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

#define ACCEL_FLAT 2.2
// between ACCEL_FLAT and -ACCEL_FLAT, don't change the worm's direction.
// >ACCEL_FLAT, point worm positive direction (right or down)
// <-ACCEL_FLAT, point worm negative direction (left or up)
#define ACCEL_JUMP 2.2
// if x or y changes by more than ACCEL_JUMP,
// consider that a command to move in that + or - x or y direction.
// (overrides absolute tilt above, to be more responsive.)
float x_accel;
float y_accel;
float z_accel; // unused
float x_old;
float y_old;
float x_delta; // from flat
float y_delta;
float x_jump; // since last iteration
float y_jump;

// game state
struct game_cell_s {
  int x, y;
  int img;
};
struct game_cell_s player_cell;
int player_dir; // -1 to turn left, +1 to turn right: so use int for this math, not enum
struct game_cell_s pill_cell; // pill the worm can eat go grow longer at this x,y

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
    Serial.print(",");
    Serial.print(worm_cell->img);
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
  //worm_cell_count = 15; // worm starts moderately long
  worm_cell_count = 4; // worm starts short
  //worm_cell_count = 1; // worm starts stubby

  // set where head of worm will be
  player_cell.x = CELLS_X/2;
  player_cell.y = CELLS_Y/2;
  player_cell.img = dir_east;

  // add worm's starting segments
  for (cell_idx = 0; cell_idx < worm_cell_count; cell_idx++) {
    worm_cells[cell_idx].x = player_cell.x-cell_idx-1;
    worm_cells[cell_idx].y = player_cell.y;
    if (cell_idx == 0) {
      worm_cells[cell_idx].img = mouth_east;
    } else if (cell_idx == worm_cell_count-1) {
      worm_cells[cell_idx].img = tail_east;
    } else {
      worm_cells[cell_idx].img = body_east;
    }
  }
  // not trying to erase a tail right now
  tail_cell.x = -1;
  tail_cell.y = -1;
  // no pill on screen yet
  pill_cell.x = -1;
  pill_cell.y = -1;
//  pill_event.counter = 0;
//  pill_event.target = 100;
  print_worm();
}

void walk_player(void) {
  int cell_idx;
  static bool did_walk = false;
  bool         do_walk = true;

  switch (worm_cells[0].img) {
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
  player_cell.img = worm_cells[0].img;

  if (did_walk && !do_walk) {
    start_sound (sound_stop);
  }
  if (do_walk && !did_walk) {
    start_sound (sound_go);
  }

  if (do_walk) {
    if (player_cell.x == pill_cell.x &&
        player_cell.y == pill_cell.y) {
      pill_cell.x   = -1;
      pill_cell.y   = -1;
      pill_cell.img = -1;
      start_sound (sound_pill);
      if (worm_cell_count < MAX_SEGMENTS) {
        worm_cell_count++;
      }
    }

    tail_cell.x   = worm_cells[worm_cell_count-1].x;
    tail_cell.y   = worm_cells[worm_cell_count-1].y;
    tail_cell.img = img_erase;
    for (cell_idx = worm_cell_count-1; cell_idx >= 1; cell_idx--) {
      worm_cells[cell_idx].x   = worm_cells[cell_idx-1].x;
      worm_cells[cell_idx].y   = worm_cells[cell_idx-1].y;
      if (cell_idx == 1) {
        worm_cells[cell_idx].img = worm_cells[cell_idx-1].img + direction_count; // change FROM: mouth TO: body
      } else if (cell_idx == worm_cell_count-1) {
        worm_cells[cell_idx].img = worm_cells[cell_idx-1].img + direction_count; // change FROM: body TO: tail
      } else {
        worm_cells[cell_idx].img = worm_cells[cell_idx-1].img;
      }
    }
    worm_cells[0].x   = player_cell.x;
    worm_cells[0].y   = player_cell.y;
    worm_cells[0].img = player_cell.img;
  } else { // don't try to erase tail anymore: not moving.
    tail_cell.x   = -1;
    tail_cell.y   = -1;
    tail_cell.img = -1;
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

void draw_cell(struct game_cell_s *the_cell) {
  struct game_cell_s center; // center of the cell, in pixel x, y
  // points for mouth cells, in pixels
  struct game_cell_s upper_lip;
  struct game_cell_s lower_lip;
  // points for body cells, in pixels
  struct game_cell_s upper_left;
  struct game_cell_s lower_right;
  struct game_cell_s body1;
  struct game_cell_s body2;
  int radius;

  if (the_cell->x < 0 || the_cell->x >= CELLS_X ||
      the_cell->y < 0 || the_cell->y >= CELLS_Y ||
      is_in_score (the_cell)) {
      return; // off-screen location, don't draw
  }

  radius = CELL_PIXELS/2;
  center.x = the_cell->x * CELL_PIXELS + radius;
  center.y = the_cell->y * CELL_PIXELS + radius;
  upper_left.x = the_cell->x * CELL_PIXELS;
  upper_left.y = the_cell->y * CELL_PIXELS;
  lower_right.x = ((the_cell->x+1) * CELL_PIXELS)-1;
  lower_right.y = ((the_cell->y+1) * CELL_PIXELS)-1;

  switch (the_cell->img) {
  case img_pill:
    tft.fillCircle(center.x, center.y, radius, color_pill);
    break;
  case mouth_east:
    upper_lip.x = center.x + radius;
    upper_lip.y = center.y - radius;
    lower_lip.x = center.x + radius;
    lower_lip.y = center.y + radius;
    tft.fillCircle(center.x, center.y, radius, color_worm);
    tft.fillTriangle(center.x, center.y, upper_lip.x, upper_lip.y,
                                        lower_lip.x, lower_lip.y, color_backgr);
    break;
  case mouth_west:
    upper_lip.x = center.x - radius;
    upper_lip.y = center.y - radius;
    lower_lip.x = center.x - radius;
    lower_lip.y = center.y + radius;
    tft.fillCircle(center.x, center.y, radius, color_worm);
    tft.fillTriangle(center.x, center.y, upper_lip.x, upper_lip.y,
                                        lower_lip.x, lower_lip.y, color_backgr);
    break;
  case mouth_north:
    upper_lip.x = center.x + radius;
    upper_lip.y = center.y - radius;
    lower_lip.x = center.x - radius;
    lower_lip.y = center.y - radius;
    tft.fillCircle(center.x, center.y, radius, color_worm);
    tft.fillTriangle(center.x, center.y, upper_lip.x, upper_lip.y,
                                        lower_lip.x, lower_lip.y, color_backgr);
    break;
  case mouth_south:
    upper_lip.x = center.x + radius;
    upper_lip.y = center.y + radius;
    lower_lip.x = center.x - radius;
    lower_lip.y = center.y + radius;
    tft.fillCircle(center.x, center.y, radius, color_worm);
    tft.fillTriangle(center.x, center.y, upper_lip.x, upper_lip.y,
                                        lower_lip.x, lower_lip.y, color_backgr);
    break;
  case body_east:
    body1.x = lower_right.x - 3*radius/5;
    body1.y = center.y-CELL_PIXELS/5;
    body2.x = upper_left.x + 2*radius/5;
    body2.y = center.y+CELL_PIXELS/5;
    tft.fillCircle(body1.x, body1.y, 3*radius/5, color_worm);
    tft.fillCircle(body2.x, body2.y, 2*radius/5, color_worm);
    break;
  case body_west:
    body1.x = upper_left.x + 3*radius/5;
    body1.y = center.y-CELL_PIXELS/5;
    body2.x = lower_right.x - 2*radius/5;
    body2.y = center.y+CELL_PIXELS/5;
    tft.fillCircle(body1.x, body1.y, 3*radius/5, color_worm);
    tft.fillCircle(body2.x, body2.y, 2*radius/5, color_worm);
    break;
  case body_north:
    body1.x = center.x - CELL_PIXELS/5;
    body1.y = upper_left.y + 3*radius/5;
    body2.x = center.x + CELL_PIXELS/5;
    body2.y = lower_right.y - 2*radius/5;
    tft.fillCircle(body1.x, body1.y, 3*radius/5, color_worm);
    tft.fillCircle(body2.x, body2.y, 2*radius/5, color_worm);
    break;
  case body_south:
    body1.x = center.x - CELL_PIXELS/5;
    body1.y = lower_right.y - 3*radius/5;
    body2.x = center.x + CELL_PIXELS/5;
    body2.y = upper_left.y + 2*radius/5;
    tft.fillCircle(body1.x, body1.y, 3*radius/5, color_worm);
    tft.fillCircle(body2.x, body2.y, 2*radius/5, color_worm);
    break;
  case tail_east:
    body1.x = lower_right.x - 3*radius/5;
    body1.y = center.y+1;
    body2.x = upper_left.x + 2*radius/5;
    body2.y = center.y-1;
    tft.fillCircle(body1.x, body1.y, 3*radius/5, color_worm);
    tft.fillCircle(body2.x, body2.y, 2*radius/5, color_worm);
    break;
  case tail_west:
    body1.x = upper_left.x + 3*radius/5;
    body1.y = center.y+1;
    body2.x = lower_right.x - 2*radius/5;
    body2.y = center.y-1;
    tft.fillCircle(body1.x, body1.y, 3*radius/5, color_worm);
    tft.fillCircle(body2.x, body2.y, 2*radius/5, color_worm);
    break;
  case tail_north:
    body1.x = center.x-1;
    body1.y = upper_left.y + 3*radius/5;
    body2.x = center.x+1;
    body2.y = lower_right.y - 2*radius/5;
    tft.fillCircle(body1.x, body1.y, 3*radius/5, color_worm);
    tft.fillCircle(body2.x, body2.y, 2*radius/5, color_worm);
    break;
  case tail_south:
    body1.x = center.x-1;
    body1.y = lower_right.y - 3*radius/5;
    body2.x = center.x+1;
    body2.y = upper_left.y + 2*radius/5;
    tft.fillCircle(body1.x, body1.y, 3*radius/5, color_worm);
    tft.fillCircle(body2.x, body2.y, 2*radius/5, color_worm);
    break;
  default:
    break;
  }
}

void erase_cell(struct game_cell_s *the_cell) {
  int x, y;

  if (the_cell->x < 0 || the_cell->x >= CELLS_X ||
      the_cell->y < 0 || the_cell->y >= CELLS_Y ||
      is_in_score (the_cell)) {
      return; // off-screen location, don't draw
  }
  x = the_cell->x * CELL_PIXELS;
  y = the_cell->y * CELL_PIXELS;
  tft.fillRect(x, y, CELL_PIXELS+1, CELL_PIXELS+1, color_backgr);
}

void draw_game(void) {
  // draw score
  static int score_old = -1;
  static game_cell_s head_old = { -1, -1, -1 };
  static game_cell_s pill_old = { -1, -1, -1 };

  if (score_old != worm_cell_count) { // score changed; redraw it
    // background
    tft.fillRect(0, 0, SCORE_WIDTH, SCORE_HEIGHT, color_backgr);
    // number
    our_drawnum (worm_cell_count, color_score);

    score_old = worm_cell_count; // save the new score
  }

  if (head_old.x   != worm_cells[0].x ||
      head_old.y   != worm_cells[0].y ||
      head_old.img != worm_cells[0].img) {
      // worm head
      draw_cell (&worm_cells[0]);
      // worm body behind head
      if (worm_cell_count > 1) {
        erase_cell (&worm_cells[1]); // hide old head image
        draw_cell (&worm_cells[1]);
      }
      if (worm_cell_count > 2) {
        erase_cell (&worm_cells[worm_cell_count-1]); // end of worm
        draw_cell (&worm_cells[worm_cell_count-1]);
      }
      // erase old worm tail
      erase_cell (&tail_cell);

      head_old.x   = worm_cells[0].x;
      head_old.y   = worm_cells[0].y;
      head_old.img = worm_cells[0].img;
  }

  // pill!
  if (pill_old.x   != pill_cell.x ||
      pill_old.y   != pill_cell.y) {
    draw_cell (&pill_cell);
    pill_old.x = pill_cell.x;
    pill_old.y = pill_cell.y;
  }
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
  tft.fillScreen(color_backgr);
  time = millis() - time;

  Serial.println(time, DEC);
  draw_banner (color_intro1);
  start_sound (sound_intro1);

  draw_banner (color_intro2);
  start_sound (sound_intro2);

  draw_banner (color_intro3);
  start_sound (sound_intro3);
  delay(2000);
  tft.fillScreen(color_backgr);
  start_game();
  draw_game();
}

void loop() {
  float x_move;
  float y_move;
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

  x_accel = CircuitPlayground.motionX();
  y_accel = CircuitPlayground.motionY();
  z_accel = CircuitPlayground.motionZ();
#ifdef DEBUG_ACCEL
  Serial.print("X Accel: ");
  Serial.print(x_accel);
  Serial.print(", Y Accel: ");
  Serial.print(y_accel);
  Serial.print(", Z Accel: ");
  Serial.println(z_accel);
#endif

  if (left_btn && !left_old) {
    worm_cells[0].img = worm_cells[0].img-1;
    if (worm_cells[0].img < 0) {
      worm_cells[0].img = direction_count-1; // kept turning left from first direction; now looking in last direction.
    }
  }
  if (right_btn && !right_old) {
    worm_cells[0].img = worm_cells[0].img+1;
    if (worm_cells[0].img >= direction_count) {
      worm_cells[0].img = 0; // kept turning right from last direction, now looking back in first direction again.
    }
  }

  x_delta = 0;
  y_delta = 0;
  // find if we are tilted, but not moving in that direction
  if (x_accel > ACCEL_FLAT && worm_cells[0].img != dir_east) {
      x_delta = x_accel;
  }
  if (x_accel < -ACCEL_FLAT && worm_cells[0].img != dir_west) {
      x_delta = x_accel;
  }
  if (y_accel > ACCEL_FLAT && worm_cells[0].img != dir_south) {
      y_delta = y_accel;
  }
  if (y_accel < -ACCEL_FLAT && worm_cells[0].img != dir_north) {
      y_delta = y_accel;
  }
  // Note: This causes the effect if we are diagonal,
  // we will keep switching between x and y directions in a 'staircase' effect.
  //  (Kind of cool, actually.)

  // find if the amount of tilt changed quickly
  // Note: if you speed up the main loop, this response will change.
  x_jump = (x_accel - x_old);
  if (fabs(x_jump) < ACCEL_JUMP) {
    x_jump = 0;
  }
  y_jump = (y_accel - y_old);
  if (fabs(y_jump) < ACCEL_JUMP) {
    y_jump = 0;
  }

  // let strong tilt in one direction override
  //      minor tilt in another direction
  x_move = 0;
  y_move = 0;
  if (fabs(x_delta) > fabs(y_delta)) {
    x_move = x_delta;
  } else if (fabs(y_delta) > fabs(x_delta)) {
    y_move = y_delta;
  }

  // let fast increases in tilt override
  //   absolute tilt value
  if (fabs(x_jump) > fabs(y_jump)) {
    x_move = x_jump;
    y_move = 0;
  } else if (fabs(y_jump) > fabs(x_jump)) {
    x_move = 0;
    y_move = y_jump;
  }

  // face the direction that was tilted the fastest,
  //  or if none tilted fast, the way we are most tilted.
  if (x_move > 0) {
    worm_cells[0].img = dir_east;
  }
  if (x_move < 0) {
    worm_cells[0].img = dir_west;
  }
  if (y_move > 0) {
    worm_cells[0].img = dir_south;
  }
  if (y_move < 0) {
    worm_cells[0].img = dir_north;
  }
  // Note: z_accel currently unused

#ifdef DEBUG_MOVING
  Serial.print("player_cell.x: ");
  Serial.print(player_cell.x);
  Serial.print(", player_cell.y: ");
  Serial.println(player_cell.y);
#endif
  right_old = right_btn;
  left_old  =  left_btn;
  x_old = x_accel;
  y_old = y_accel;

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
        pill_cell.x   = random(0, CELLS_X);
        pill_cell.y   = random(0, CELLS_Y);
        pill_cell.img = img_pill;
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
void our_drawtext(const char *text, int line, uint16_t color) {
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