#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

#define BTN_UP   2
#define BTN_DOWN 3
#define BTN_OK   4

#define MAX_OBSTACLES 3   

int obstacleCols[MAX_OBSTACLES];
int obstacleRows[MAX_OBSTACLES];
bool obstacleActive[MAX_OBSTACLES];


LiquidCrystal_I2C lcd(0x27, 16, 2);

// ---------------- STATES ----------------
enum State {
  STATE_INTRO,
  STATE_MENU,
  STATE_GAME,
  STATE_GAME_OVER,
  STATE_HIGH_SCORE
};
byte playerChar[] = {
  B01000,
  B11101,
  B11110,
  B01111,
  B01111,
  B11110,
  B11101,
  B01000
};
byte obstacleChar[8] = {
  B00011,
  B00110,
  B01100,
  B11000,
  B11000,
  B01100,
  B00110,
  B00011
};

State currentState = STATE_INTRO;

// ---------------- MENU ----------------
int menuIndex = 0;
int lastMenuIndex = -1;

// ---------------- GAME ----------------
int playerRow = 0;
const int playerCol = 1;


int score = 0;
int highScore = 0;

int prevPlayerRow = 0;

bool introDrawn = false;
bool gameOverDrawn = false;

int nextObstacleRow = 0;   

unsigned long lastSpawnTime = 0;
// ---------------- TIMING ----------------
unsigned long lastMoveTime = 0;
unsigned long gameSpeed = 900;
unsigned long stateStartTime = 0;

int prevObstacleCols[MAX_OBSTACLES];
int prevObstacleRows[MAX_OBSTACLES];


// ---------------- SETUP ----------------
void setup() {
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_OK, INPUT_PULLUP);

  lcd.init();
  lcd.backlight();
  lcd.createChar(0, playerChar);
  lcd.createChar(1, obstacleChar);

  highScore = readHighScore();
  stateStartTime = millis();
}

// ---------------- LOOP ----------------
void loop() {

  readButtons();

  switch (currentState) {

    case STATE_INTRO:
      runIntro();
      break;

    case STATE_MENU:
      runMenu();
      break;

    case STATE_GAME:
      runGame();
      break;

    case STATE_GAME_OVER:
      runGameOver();
      break;

    case STATE_HIGH_SCORE:
      runHighScore();
      break;
  }
}

// ---------------- BUTTON HANDLING ----------------
void readButtons() {

  if (currentState == STATE_MENU) {

    if (digitalRead(BTN_UP) == LOW) {
      menuIndex--;
      delay(180);
    }

    if (digitalRead(BTN_DOWN) == LOW) {
      menuIndex++;
      delay(180);
    }

    if (digitalRead(BTN_OK) == LOW) {
      if (menuIndex == 0) startGame();
      if (menuIndex == 1) {
        lcd.clear();
        stateStartTime = millis();
        currentState = STATE_HIGH_SCORE;
      }
      delay(200);
    }
  }

  else if (currentState == STATE_GAME) {

    if (digitalRead(BTN_UP) == LOW) playerRow = 0;
    if (digitalRead(BTN_DOWN) == LOW) playerRow = 1;
  }

  else if (currentState == STATE_GAME_OVER) {
    if (digitalRead(BTN_OK) == LOW) {
      lcd.clear();
      gameOverDrawn = false;   
      lastMenuIndex = -1;      
      currentState = STATE_MENU;
      delay(200);
    }
  }

}

// ---------------- INTRO ----------------
void runIntro() {

  if (millis() - stateStartTime < 2000) {

    if (!introDrawn) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(" Racing Game ");
      introDrawn = true;
    }

  }
  else if (millis() - stateStartTime < 4000) {

    if (introDrawn) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(" Created by ");
      lcd.setCursor(0, 1);
      lcd.print(" GUNA ");
      introDrawn = false;
    }

  }
  else {
    lcd.clear();
    introDrawn = false;
    currentState = STATE_MENU;
  }
}


// ---------------- MENU ----------------
void runMenu() {

  if (menuIndex < 0) menuIndex = 0;
  if (menuIndex > 1) menuIndex = 1;

  if (menuIndex != lastMenuIndex) {

    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print(menuIndex == 0 ? "> Start Game" : "  Start Game");

    lcd.setCursor(0, 1);
    lcd.print(menuIndex == 1 ? "> High Score" : "  High Score");

    lastMenuIndex = menuIndex;
  }
}

// ---------------- START GAME ----------------
void startGame() {

  lcd.clear();
  score = 0;
  gameSpeed = 900;

  playerRow = 0;

  for (int i = 0; i < MAX_OBSTACLES; i++) {
    obstacleActive[i] = false;
    obstacleCols[i] = 15;
    obstacleRows[i] = 0;

    prevObstacleCols[i] = obstacleCols[i];
    prevObstacleRows[i] = obstacleRows[i];
  }


  nextObstacleRow = 0;
  lastMoveTime = millis();
  currentState = STATE_GAME;
}


// ---------------- GAME LOGIC ----------------
void runGame() {

  if (millis() - lastMoveTime > gameSpeed) {

  for (int i = 0; i < MAX_OBSTACLES; i++) {

    if (obstacleActive[i]) {
      obstacleCols[i]--;

      // Obstacle passed successfully
      if (obstacleCols[i] < 0) {
        obstacleActive[i] = false;
        score++;

        // Speed up gradually
        if (score % 3 == 0 && gameSpeed > 200) {
          gameSpeed -= 40;
        }
      }
    }
  }

  lastMoveTime = millis();
  }


  // COLLISION
  for (int i = 0; i < MAX_OBSTACLES; i++) {
  if (obstacleActive[i] &&
      obstacleCols[i] == playerCol &&
      obstacleRows[i] == playerRow) {
    endGame();
    return;
  }
  }

  


  drawGame();
  
  unsigned long spawnInterval = 1200;

if (millis() - lastSpawnTime > spawnInterval) {

  // ---- FAIRNESS CHECK FIRST ----
  for (int i = 0; i < MAX_OBSTACLES; i++) {
    if (obstacleActive[i] && obstacleCols[i] > 12) {
      return;  // too close, wait next frame
    }
  }

  // ---- FIND FREE SLOT ----
  for (int i = 0; i < MAX_OBSTACLES; i++) {
    if (!obstacleActive[i]) {

      obstacleActive[i] = true;
      obstacleCols[i] = 15;
      obstacleRows[i] = nextObstacleRow;

      nextObstacleRow = 1 - nextObstacleRow;
      lastSpawnTime = millis();
      break;
    }
  }
}


}

// ---------------- DRAW GAME ----------------
void drawGame() {

  // ---- ERASE PREVIOUS PLAYER ----
  lcd.setCursor(playerCol, prevPlayerRow);
  lcd.print(" ");

  // ---- ERASE PREVIOUS OBSTACLES ----
  for (int i = 0; i < MAX_OBSTACLES; i++) {
    lcd.setCursor(prevObstacleCols[i], prevObstacleRows[i]);
    lcd.print(" ");
  }

  // ---- DRAW PLAYER ----
  lcd.setCursor(playerCol, playerRow);
  lcd.write(byte(0));

  // ---- DRAW OBSTACLES ----
  for (int i = 0; i < MAX_OBSTACLES; i++) {
    if (obstacleActive[i]) {
      lcd.setCursor(obstacleCols[i], obstacleRows[i]);
      lcd.write(byte(1));
    }
  }

  // ---- DRAW SCORE ----
  lcd.setCursor(12, 0);
  lcd.print("S:");
  lcd.print(score);
  lcd.print(" ");

  // ---- SAVE CURRENT POSITIONS ----
  prevPlayerRow = playerRow;

  for (int i = 0; i < MAX_OBSTACLES; i++) {
    prevObstacleCols[i] = obstacleCols[i];
    prevObstacleRows[i] = obstacleRows[i];
  }
}




// ---------------- GAME OVER ----------------
void endGame() {

  if (score > highScore) {
  highScore = score;
  if (highScore > 255) highScore = 255; // clamp
    EEPROM.write(0, highScore);
  }


  lcd.clear();
  gameOverDrawn = false;
  stateStartTime = millis();
  currentState = STATE_GAME_OVER;
}


void runGameOver() {

  if (!gameOverDrawn) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Game Over");
    lcd.setCursor(0, 1);
    lcd.print("Score: ");
    lcd.print(score);
    gameOverDrawn = true;
  }

  // After 5 sec, show "Press OK"
  if (millis() - stateStartTime > 5000) {
    lcd.setCursor(0, 0);
    lcd.print("Press OK      ");
    lcd.setCursor(0, 1);
    lcd.print("for Menu      ");
  }
}


// ---------------- HIGH SCORE ----------------
void runHighScore() {

  static bool highScoreDrawn = false;

  if (!highScoreDrawn) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("High Score");
    lcd.setCursor(0, 1);
    lcd.print(highScore);
    highScoreDrawn = true;
    stateStartTime = millis();
  }

  // Auto return after 3 seconds
  if (millis() - stateStartTime > 3000) {
    lcd.clear();
    highScoreDrawn = false;
    lastMenuIndex = -1;   // force menu redraw
    currentState = STATE_MENU;
  }
}


int readHighScore() {
  int value = EEPROM.read(0);
  if (value == 255) {   // EEPROM empty
    value = 0;
    EEPROM.write(0, 0);
  }
  return value;
}


