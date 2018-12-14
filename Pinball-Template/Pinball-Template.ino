#include <LedControl.h>
#include <EEPROM.h>
#include <Button.h>
#include <Servo.h>
#include "sounds.h"


/*
Pinball - Starting Template

*/

//Setup constant pin values

#define SCORE_1_PIN  2

//if using the coin slot, this is the pin that will be used.
#define COIN_SLOT 3

//this is the pin that is used to detect the return at the bottom of the machine.
#define RETURN_PIN    A1 

#define POP_1_SOLENIOD A2

#define BUMPER_PIN 12

#define LED_DISPLAY_1_DIN 4
#define LED_DISPLAY_1_CLK 5
#define LED_DISPLAY_1_LOAD 6

#define LED_DISPLAY_2_DIN 7
#define LED_DISPLAY_2_CLK 9
#define LED_DISPLAY_2_LOAD 8

//reset pin - this is the reset button.
#define RESET_PIN A4

#define SERVO_PWM 10

//use a PWM pin for the buzzer
#define BUZZER_PIN 11

//these are states that our machine can be in.
#define ATTRACT_MODE 0
#define PLAY_MODE 1
#define GAME_OVER 2
#define SCORE_SOUND_MODE 3

#define BALLS_GIVEN_PER_GAME 3

unsigned long currentMillis = 0;

LedControl highScoreLED = LedControl(LED_DISPLAY_1_DIN,LED_DISPLAY_1_CLK,LED_DISPLAY_1_LOAD,1); 

LedControl scoreLED = LedControl(LED_DISPLAY_2_DIN,LED_DISPLAY_2_CLK,LED_DISPLAY_2_LOAD,1); 

Button resetButton = Button(RESET_PIN,PULLUP);

volatile unsigned long highScore;

volatile unsigned long prevScoreMillis = 0;

//store which part of the game we are up to.
volatile int gameMode = ATTRACT_MODE; 

volatile unsigned long currentScore = 0;

volatile int lostBallCount = 0;

//we need to keep track of the time between ball counts,
//otherwise if the ball stays on the sensor for too long, it will count extra
//ball losses.

long previousBallLostMillis = 0;  

long previousScoreUpdate = 0;

long lostBallInterval = 2000; //2 secs?

Servo gateServo;

#ifdef USE_COIN_SLOT


#else

 //we will have the return plugged into the interrupt pin, so we refine the pins below

  #define COIN_SLOT A1

  //this is the pin that is used to detect the return at the bottom of the machine.
  #define RETURN_PIN    3

#endif

void setup() {
  
  Serial.begin(9600);
  //setup the LED's
  highScoreLED.shutdown(0, false);  // turns on display
  highScoreLED.setIntensity(0, 15); // 15 = brightest

  gateServo.attach(SERVO_PWM);

  for(int index=0;index<scoreLED.getDeviceCount();index++) {
        scoreLED.shutdown(index,false); 
  } 

  for(int index=0;index<highScoreLED.getDeviceCount();index++) {
        highScoreLED.shutdown(index,false); 
  } 

  scoreLED.setIntensity(0, 15); // 15 = brightest
  highScoreLED.setIntensity(0,15);

  //WriteScore(0);
  //load high score
  highScore = ReadScore();

  setupGame();
  Serial.println("Setup Done");
}

void loop() 
{
  

  switch(gameMode){

        case ATTRACT_MODE:
            showLEDAttraction();
            break;
        case PLAY_MODE:
            writePlayOn7Segment();
            delay(1000);
            gameLoop();
            break;
        case GAME_OVER:
            gameOver();
            break;

  } 



}


void gameLoop(){
  lostBallCount = 0;

  int servoPositionCounter = 0;
  unsigned long prevServoMillis = 0;
 

  int scoreIteration = 0;

  long highestScore = ReadScore();

  scoreLED.clearDisplay(0);
  highScoreLED.clearDisplay(0);

  while(lostBallCount < BALLS_GIVEN_PER_GAME){

    unsigned long currentMillis = millis();

    //check inputs, add to score
    
    if(gameMode == SCORE_SOUND_MODE){
        Serial.println("Scoring");
        if(currentMillis - prevScoreMillis > 50) {  //50ms delay between scoring ops
          prevScoreMillis = currentMillis;
          currentScore += 100;
          ScoreBeep();
          gameMode = PLAY_MODE;
        }
        
    }
   
    //make noises
    if(currentMillis - previousScoreUpdate > 1000) {  //update the display every second
      previousScoreUpdate = currentMillis;  
      scoreIteration++;

      //every one second, switch between the high score and balls remaining.
      if(scoreIteration % 2 == 0){
        //print high score
        printHighScore(ReadScore());
      }else{
        //print balls remaining.
        writeBallCountOn7Segment(BALLS_GIVEN_PER_GAME - lostBallCount);
      }

      //change to game over state when done
      printScore(currentScore);

      
    }

    
   
    //check to see if the ball has returned (loss).
    if(digitalRead(RETURN_PIN) == LOW){

 
      if(currentMillis - previousBallLostMillis > lostBallInterval) {
          // save the last time you lost the ball 
          previousBallLostMillis = currentMillis;   
 
          lostBallCount++;
          //make a sound
          BallLossBeep();

          if(lostBallCount == BALLS_GIVEN_PER_GAME){
           gameOver();
          }
      }
    }

    //only increment the servo after 20ms or so
    if(currentMillis - prevServoMillis > 20) {
      prevServoMillis = currentMillis;   
      servoPositionCounter++;
      if(servoPositionCounter > 180){
        servoPositionCounter = 0;
      }
    }

    //the servo will try to move back and forth.
    gateServo.write(servoPositionCounter);
  }
}

void setupGame()
{
  //make sure all of our pins are setup correctly.
  pinMode(SCORE_1_PIN, INPUT_PULLUP);
  pinMode(COIN_SLOT, INPUT);
  pinMode(BUMPER_PIN, INPUT_PULLUP);
  pinMode(RETURN_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);

  pinMode(POP_1_SOLENIOD, OUTPUT);
  

  //attach interrupts
  attachInterrupt(digitalPinToInterrupt(SCORE_1_PIN), ScoreSlot1, CHANGE);
  #ifdef USE_COIN_SLOT
  attachInterrupt(digitalPinToInterrupt(COIN_SLOT), CoinSlot, RISING);
  #else
  //we need to plug the return into the coin slot PCB JST header for this to work
  attachInterrupt(digitalPinToInterrupt(COIN_SLOT), ReturnWatch, RISING);
  #endif

}

void ReturnWatch()
{
  if(currentMillis - previousBallLostMillis > lostBallInterval) 
  {
          // save the last time you lost the ball 
          previousBallLostMillis = currentMillis;   
 
          lostBallCount++;
          //make a sound
          BallLossBeep();

          if(lostBallCount == BALLS_GIVEN_PER_GAME){
           gameOver();
          }
  }
  
}


void showLEDAttraction(){

  while(gameMode == ATTRACT_MODE)
  {
    //always be checking for the coin slot, then
    //we can transition to the game play mode.
  
    if(resetButton.isPressed()){
  
        //check to see how long the button has been pressed, clear the high score.
        
    }
    writeArduinoOn7Segment();
    //StarWarsFirstSection();
    //StarWarsSecondSection();
    Play_HB();
    
    //scrollDigits();
    //check to see if a coin has been added to the slot?
    if(digitalRead(COIN_SLOT) == HIGH){
      gameMode = PLAY_MODE;
      
    }

  }

}

void gameOver(){

  //show something to invite player back
  //write high score if needed.

  if(currentScore > ReadScore()){
    Serial.println("Writing Score");
    WriteScore(currentScore);

    //let the player know they won the high score.
    Play_Titanic();

  }else{
    delay(2000);
  }

  currentScore = 0;
  //change state back to attract mode.
  gameMode = ATTRACT_MODE;
}

void ScoreSlot1() {

  
  if(gameMode == PLAY_MODE){
    //increment score by chaning game modes
    gameMode = SCORE_SOUND_MODE; //change the game mode so that our loop will play a sound.
  }
  
  
}

void CoinSlot(){
  if(gameMode == PLAY_MODE){
    //they put more money into the coin slot .. give them extra balls to play with.
    lostBallCount = 0;
  }
  gameMode = PLAY_MODE;
  
}


void WriteScore(long score){
  
  eepromLongWrite(0, score);

}

long ReadScore(){
  
  return eepromLongRead(0);

}


void eepromLongWrite(int address, long value)
{
    EEPROM.put(address, value);
    
}

long eepromLongRead(long address)
{
    
    long storedScore = 0;
    EEPROM.get(address, storedScore);
    
    return storedScore;
}


void printScore(unsigned long value) {
   int counter = 0;
   while (value > 0 && counter < 8) {
      int digit = value % 10;
      // print digit
      scoreLED.setDigit(0,counter,digit,false);
      value /= 10;
      counter++;
    }
    
   
}

void printHighScore(unsigned long value) {
   
  highScoreLED.clearDisplay(0);

  int counter = 0;
   while (value > 0 && counter < 8) {
      int digit = value % 10;
      // print digit
      highScoreLED.setDigit(0,counter,digit,false);
      value /= 10;
      counter++;
    }
    
   
}

void changeLEDIntensity(LedControl* control){

  for(int brightness= 0; brightness <=15; brightness++){
    control->setIntensity(0, brightness); // 15 = brightest
    delay(100);
  }

}

void writePlayOn7Segment(){
  scoreLED.clearDisplay(0);
  scoreLED.setChar(0,7,'P',false);
  scoreLED.setChar(0,6,'L',false);
  scoreLED.setChar(0,5,'A',false);
  scoreLED.setRow(0,4,0x3b); //Y
}


void writeBallCountOn7Segment(int ballsLeft){

  highScoreLED.clearDisplay(0);
  highScoreLED.setChar(0,7,'b',false);
  highScoreLED.setChar(0,6,'a',false);
  highScoreLED.setChar(0,5,'l',false);
  highScoreLED.setChar(0,4,'l',false);
  highScoreLED.setChar(0,3,' ',false);
  highScoreLED.setChar(0,2,ballsLeft,false);
}

void writeArduinoOn7Segment() {
  /* we always wait a bit between updates of the display */
  unsigned long delaytime=250;
  scoreLED.clearDisplay(0);
  scoreLED.setChar(0,7,'a',false);  //a
  attractDelay(delaytime);
  scoreLED.setRow(0,6,0x05); //r
  attractDelay(delaytime);
  scoreLED.setChar(0,5,'d',false); //d
  attractDelay(delaytime);
  scoreLED.setRow(0,4,0x1c);  //u
  attractDelay(delaytime);
  scoreLED.setRow(0,3,B00010000); //i
  attractDelay(delaytime);
  scoreLED.setRow(0,2,0x15); //n
  attractDelay(delaytime);
  scoreLED.setRow(0,1,0x1D); //o
  attractDelay(delaytime);
  
}

void scrollDigits() {
  unsigned long delaytime=250;
  for(int i=0;i<13;i++) {
    scoreLED.setDigit(0,3,i,false);
    scoreLED.setDigit(0,2,i+1,false);
    scoreLED.setDigit(0,1,i+2,false);
    scoreLED.setDigit(0,0,i+3,false);
    delay(delaytime);
  }
  scoreLED.clearDisplay(0);
  delay(delaytime);
}

void BallLossBeep()
{
    playModeBeep(gH, 250);
    playModeBeep(cH,500);     
}

void ScoreBeep()
{
    playModeBeep(gH, 250);
    
}

void beep(int note, int duration)
{

  if(gameMode == PLAY_MODE){
    return;
  }
  
  //Play tone on buzzerPin
  tone(BUZZER_PIN, note, duration);
  attractDelay(duration);
  //Stop tone on buzzerPin
  noTone(BUZZER_PIN);
 
  attractDelay(50);
 
}

void playModeBeep(int note, int duration)
{

  
  
  //Play tone on buzzerPin
  tone(BUZZER_PIN, note, duration);
  delay(duration);
  //Stop tone on buzzerPin
  noTone(BUZZER_PIN);
 
 
}

void attractDelay(int duration){
  if(gameMode == PLAY_MODE){
    return;
  }else{
    delay(duration);
  }
}

void StarWarsFirstSection()
{
  beep(a, 500);
  beep(a, 500);    
  beep(a, 500);
  beep(f, 350);
  beep(cH, 150);  
  beep(a, 500);
  beep(f, 350);
  beep(cH, 150);
  beep(a, 650);
 
  if(gameMode == PLAY_MODE){
    return;
  }

  attractDelay(500);
 
  beep(eH, 500);
  beep(eH, 500);
  beep(eH, 500);  
  beep(fH, 350);
  beep(cH, 150);
  beep(gS, 500);
  beep(f, 350);
  beep(cH, 150);
  beep(a, 650);

  if(gameMode == PLAY_MODE){
    return;
  }
 
  attractDelay(500);

}
 
void StarWarsSecondSection()
{
  beep(aH, 500);
  beep(a, 300);
  beep(a, 150);
  beep(aH, 500);
  beep(gSH, 325);
  beep(gH, 175);
  beep(fSH, 125);
  beep(fH, 125);    
  beep(fSH, 250);
 
  delay(325);
 
  beep(aS, 250);
  beep(dSH, 500);
  beep(dH, 325);  
  beep(cSH, 175);  
  beep(cH, 125);  
  beep(b, 125);  
  beep(cH, 250);  
 
  delay(350);
}

void Play_MarioUW()
{
    for (int thisNote = 0; thisNote < (sizeof(MarioUW_note)/sizeof(int)); thisNote++) {

    int noteDuration = 1000 / MarioUW_duration[thisNote];//convert duration to time delay
    tone(BUZZER_PIN, MarioUW_note[thisNote], noteDuration);

    int pauseBetweenNotes = noteDuration * 1.80;
    delay(pauseBetweenNotes);
    noTone(BUZZER_PIN); //stop music on pin BUZZER_PIN 
    }
}

void Play_Titanic()
{
    for (int thisNote = 0; thisNote < (sizeof(Titanic_note)/sizeof(int)); thisNote++) {

    int noteDuration = 1000 / Titanic_duration[thisNote];//convert duration to time delay
    tone(BUZZER_PIN, Titanic_note[thisNote], noteDuration);

    int pauseBetweenNotes = noteDuration * 2.70;
    delay(pauseBetweenNotes);
    noTone(BUZZER_PIN); //stop music on pin BUZZER_PIN 
    }
}

void Play_HB()
{
  int length = 28; // the number of notes

  int tempo = 150;

  char notes[] = "GGAGcB GGAGdc GGxecBA yyecdc";

  int beats[] = { 2, 2, 8, 8, 8, 16, 1, 2, 2, 8, 8,8, 16, 1, 2,2,8,8,8,8,16, 1,2,2,8,8,8,16 };

  for (int i = 0; i < length; i++) 
  {

     if (notes[i] == ' ') {
  
       delay(beats[i] * tempo); // rest
  
     } else {
  
       playHBNote(notes[i], beats[i] * tempo);
  
     }

     // pause between notes
    if(gameMode == PLAY_MODE)
    {
          return;
    }
     delay(tempo);

  }

}

void playHBTone(int tone, int duration) 
{

  for (long i = 0; i < duration * 1000L; i += tone * 2) 
  {
  
     digitalWrite(BUZZER_PIN, HIGH);
  
     delayMicroseconds(tone);
  
     digitalWrite(BUZZER_PIN, LOW);
  
     delayMicroseconds(tone);
  
  }

}

void playHBNote(char note, int duration) 
{

  char names[] = {'C', 'D', 'E', 'F', 'G', 'A', 'B',           
  
                   'c', 'd', 'e', 'f', 'g', 'a', 'b',
  
                   'x', 'y' };

  int tones[] = { 1915, 1700, 1519, 1432, 1275, 1136, 1014,
  
                   956,  834,  765,  593,  468,  346,  224,
  
                   655 , 715 };

  int SPEE = 5;

  // play the tone corresponding to the note name
  
  for (int i = 0; i < 17; i++) {
  
     if (names[i] == note) 
     {
        int newduration = duration/SPEE;
        playHBTone(tones[i], newduration);
        
     }
  
  }

}

