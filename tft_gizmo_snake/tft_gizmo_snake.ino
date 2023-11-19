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

#if 0
// when the counter reaches the target, we will do an event.
struct game_event_s {
  int counter;
  int target;
};
//struct game_event_s pill_event;
#endif

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
  bool do_walk = true;

  switch (player_dir) {
  case dir_east:
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

  if (do_walk) {
    if (player_cell.x == pill_cell.x &&
        player_cell.y == pill_cell.y) {
      pill_cell.x = -1;
      pill_cell.y = -1;
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
}

void draw_segment(struct game_cell_s *worm_cell, uint16_t color) {
  int x, y, radius;

  if (x < 0 || x >= CELLS_X ||
      y < 0 || y >= CELLS_Y) {
      return; // off-screen location, don't draw
  }
  radius = CELL_PIXELS/2;
  x = worm_cell->x * CELL_PIXELS + radius;
  y = worm_cell->y * CELL_PIXELS + radius;
  tft.fillCircle(x, y, radius, color);
}

#define SCORE_WIDTH  70
#define SCORE_HEIGHT 28
void draw_game(void) {
  // draw score
  // background
  tft.fillRect(0, 0, SCORE_WIDTH, SCORE_HEIGHT, ST77XX_BLACK);
  // number
  our_drawnum (worm_cell_count, ST77XX_GREEN);

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

bool is_in_score(struct game_cell_s *test_cell) {
  int x, y;

  x = (test_cell->x+1) * CELL_PIXELS;
  y = (test_cell->y+1) * CELL_PIXELS;
  if (x <= SCORE_WIDTH &&
      y <= SCORE_HEIGHT) {
        return true;
  }
  return false;
}

void draw_banner (uint16_t color) {
  our_drawtext("Welcome to", -1, color);
  our_drawtext("Wormy!", 0, color);
  delay(1000);
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
  draw_banner (ST77XX_MAGENTA);
  draw_banner (ST77XX_GREEN);
  delay(2000);
  tft.fillScreen(ST77XX_BLACK);
#if 0
  delay(500);

  // large block of text
  tft.fillScreen(ST77XX_BLACK);
  testdrawtext("Lorem ipsum dolor sit amet, consectetur adipiscing elit. Curabitur adipiscing ante sed nibh tincidunt feugiat. Maecenas enim massa, fringilla sed malesuada et, malesuada sit amet turpis. Sed porttitor neque ut ante pretium vitae malesuada nunc bibendum. Nullam aliquet ultrices massa eu hendrerit. Ut sed nisi lorem. In vestibulum purus a tortor imperdiet posuere. ", ST77XX_WHITE);
  delay(1000);

  // tft print function!
  tftPrintTest();
  delay(4000);
#endif
  // a single pixel
  //tft.drawPixel(tft.width()/2, tft.height()/2, ST77XX_GREEN);
#if 0
  delay(500);

  // line draw test
  testlines(ST77XX_YELLOW);
  delay(500);

  // optimized lines
  testfastlines(ST77XX_RED, ST77XX_BLUE);
  delay(500);

  testdrawrects(ST77XX_GREEN);
  delay(500);

  testfillrects(ST77XX_YELLOW, ST77XX_MAGENTA);
  delay(500);

  tft.fillScreen(ST77XX_BLACK);
  testfillcircles(10, ST77XX_BLUE);
  testdrawcircles(10, ST77XX_WHITE);
  delay(500);

  testroundrects();
  delay(500);

  testtriangles();
  delay(500);

  mediabuttons();
  delay(500);

  Serial.println("done");
  delay(1000);
#endif
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

void testlines(uint16_t color) {
  tft.fillScreen(ST77XX_BLACK);
  for (int16_t x=0; x < tft.width(); x+=6) {
    tft.drawLine(0, 0, x, tft.height()-1, color);
    delay(0);
  }
  for (int16_t y=0; y < tft.height(); y+=6) {
    tft.drawLine(0, 0, tft.width()-1, y, color);
    delay(0);
  }

  tft.fillScreen(ST77XX_BLACK);
  for (int16_t x=0; x < tft.width(); x+=6) {
    tft.drawLine(tft.width()-1, 0, x, tft.height()-1, color);
    delay(0);
  }
  for (int16_t y=0; y < tft.height(); y+=6) {
    tft.drawLine(tft.width()-1, 0, 0, y, color);
    delay(0);
  }

  tft.fillScreen(ST77XX_BLACK);
  for (int16_t x=0; x < tft.width(); x+=6) {
    tft.drawLine(0, tft.height()-1, x, 0, color);
    delay(0);
  }
  for (int16_t y=0; y < tft.height(); y+=6) {
    tft.drawLine(0, tft.height()-1, tft.width()-1, y, color);
    delay(0);
  }

  tft.fillScreen(ST77XX_BLACK);
  for (int16_t x=0; x < tft.width(); x+=6) {
    tft.drawLine(tft.width()-1, tft.height()-1, x, 0, color);
    delay(0);
  }
  for (int16_t y=0; y < tft.height(); y+=6) {
    tft.drawLine(tft.width()-1, tft.height()-1, 0, y, color);
    delay(0);
  }
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

void testfastlines(uint16_t color1, uint16_t color2) {
  tft.fillScreen(ST77XX_BLACK);
  for (int16_t y=0; y < tft.height(); y+=5) {
    tft.drawFastHLine(0, y, tft.width(), color1);
  }
  for (int16_t x=0; x < tft.width(); x+=5) {
    tft.drawFastVLine(x, 0, tft.height(), color2);
  }
}

void testdrawrects(uint16_t color) {
  tft.fillScreen(ST77XX_BLACK);
  for (int16_t x=0; x < tft.width(); x+=6) {
    tft.drawRect(tft.width()/2 -x/2, tft.height()/2 -x/2 , x, x, color);
  }
}

void testfillrects(uint16_t color1, uint16_t color2) {
  tft.fillScreen(ST77XX_BLACK);
  for (int16_t x=tft.width()-1; x > 6; x-=6) {
    tft.fillRect(tft.width()/2 -x/2, tft.height()/2 -x/2 , x, x, color1);
    tft.drawRect(tft.width()/2 -x/2, tft.height()/2 -x/2 , x, x, color2);
  }
}

void testfillcircles(uint8_t radius, uint16_t color) {
  for (int16_t x=radius; x < tft.width(); x+=radius*2) {
    for (int16_t y=radius; y < tft.height(); y+=radius*2) {
      tft.fillCircle(x, y, radius, color);
    }
  }
}

void testdrawcircles(uint8_t radius, uint16_t color) {
  for (int16_t x=0; x < tft.width()+radius; x+=radius*2) {
    for (int16_t y=0; y < tft.height()+radius; y+=radius*2) {
      tft.drawCircle(x, y, radius, color);
    }
  }
}

void testtriangles() {
  tft.fillScreen(ST77XX_BLACK);
  uint16_t color = 0xF800;
  int t;
  int w = tft.width()/2;
  int x = tft.height()-1;
  int y = 0;
  int z = tft.width();
  for(t = 0 ; t <= 15; t++) {
    tft.drawTriangle(w, y, y, x, z, x, color);
    x-=4;
    y+=4;
    z-=4;
    color+=100;
  }
}

void testroundrects() {
  tft.fillScreen(ST77XX_BLACK);
  uint16_t color = 100;
  int i;
  int t;
  for(t = 0 ; t <= 4; t+=1) {
    int x = 0;
    int y = 0;
    int w = tft.width()-2;
    int h = tft.height()-2;
    for(i = 0 ; i <= 16; i+=1) {
      tft.drawRoundRect(x, y, w, h, 5, color);
      x+=2;
      y+=3;
      w-=4;
      h-=6;
      color+=1100;
    }
    color+=100;
  }
}

void tftPrintTest() {
  tft.setTextWrap(false);
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(0, 30);
  tft.setTextColor(ST77XX_RED);
  tft.setTextSize(1);
  tft.println("Hello World!");
  tft.setTextColor(ST77XX_YELLOW);
  tft.setTextSize(2);
  tft.println("Hello World!");
  tft.setTextColor(ST77XX_GREEN);
  tft.setTextSize(3);
  tft.println("Hello World!");
  tft.setTextColor(ST77XX_BLUE);
  tft.setTextSize(4);
  tft.print(1234.567);
  delay(1500);
  tft.setCursor(0, 0);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(0);
  tft.println("Hello World!");
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_GREEN);
  tft.print(p, 6);
  tft.println(" Want pi?");
  tft.println(" ");
  tft.print(8675309, HEX); // print 8,675,309 out in HEX!
  tft.println(" Print HEX!");
  tft.println(" ");
  tft.setTextColor(ST77XX_WHITE);
  tft.println("Sketch has been");
  tft.println("running for: ");
  tft.setTextColor(ST77XX_MAGENTA);
  tft.print(millis() / 1000);
  tft.setTextColor(ST77XX_WHITE);
  tft.print(" seconds.");
}

void mediabuttons() {
  // play
  tft.fillScreen(ST77XX_BLACK);
  tft.fillRoundRect(25, 10, 78, 60, 8, ST77XX_WHITE);
  tft.fillTriangle(42, 20, 42, 60, 90, 40, ST77XX_RED);
  delay(500);
  // pause
  tft.fillRoundRect(25, 90, 78, 60, 8, ST77XX_WHITE);
  tft.fillRoundRect(39, 98, 20, 45, 5, ST77XX_GREEN);
  tft.fillRoundRect(69, 98, 20, 45, 5, ST77XX_GREEN);
  delay(500);
  // play color
  tft.fillTriangle(42, 20, 42, 60, 90, 40, ST77XX_BLUE);
  delay(50);
  // pause color
  tft.fillRoundRect(39, 98, 20, 45, 5, ST77XX_RED);
  tft.fillRoundRect(69, 98, 20, 45, 5, ST77XX_RED);
  // play color
  tft.fillTriangle(42, 20, 42, 60, 90, 40, ST77XX_GREEN);
}
