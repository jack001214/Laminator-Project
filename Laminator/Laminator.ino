#include <Arduino_FreeRTOS.h>
#include <semphr.h>  // add the FreeRTOS functions for Semaphores (or Flags).
#include <TM1637.h>
#include <EEPROM.h>
// Instantiation and pins configurations
TM1637 tm1637(6, 5);
//RGB LED pins. common anode (activate on LOW)
#define RED 7
#define GREEN 8
#define BLUE 9
//Relay PIN
#define relay 11
// buttons on PORTD.
#define left_button 2
#define middle_button 3
#define right_button 4
const int debounceDelay = 21;  // milliseconds to wait until stable
const int taprespone = 15;
const int holdresponse = 80;

//global variables & "handles"
bool isEditMode;
int isHeating;
int buffer_disp_temp_set;
int buffer_disp_curr_temp;
//EEPROM - Value write location.
int addr = 0x0;
//https://learn.adafruit.com/thermistor/using-a-thermistor
// which analog pin to connect
#define THERMISTORPIN A0
// resistance at 25 degrees C
#define THERMISTORNOMINAL 100000
// temp. for nominal resistance (almost always 25 C)
#define TEMPERATURENOMINAL 25
// how many samples to take and average, more takes longer
// but is more 'smooth'
#define NUMSAMPLES 12
// The beta coefficient of the thermistor (usually 3000-4000)
#define BCOEFFICIENT 3950
// the value of the 'other' resistor
#define SERIESRESISTOR 100000
int samples[NUMSAMPLES];
 float steinhart;
//Tasks
void TaskButton( void *pvParameters );
void TaskDisplay( void *pvParameters );
void TempTask( void *pvParameters );
void UpdateTempTask( void *pvParameters );
void setup() {
  isEditMode = false;
  tm1637.init();
  tm1637.setBrightness(5);
  pinMode(RED, OUTPUT);
  pinMode(GREEN, OUTPUT);
  pinMode(BLUE, OUTPUT);
  pinMode(relay,OUTPUT);
  
  //off
  digitalWrite(RED, HIGH);
  digitalWrite(GREEN, HIGH);
  digitalWrite(BLUE, HIGH);
         digitalWrite(relay,LOW);

buffer_disp_temp_set = EEPROM.read(addr);
  Serial.begin(115200); //no semp

  // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
  xTaskCreate(TaskButton, (const portCHAR *)"Button_poll", 128, NULL, 1, NULL);
  xTaskCreate(TaskDisplay, (const portCHAR *)"TM1637_update", 128, NULL, 1, NULL);
  xTaskCreate(TempTask, (const portCHAR *)"temp_set_read", 128, NULL, 1, NULL); //very important.
  xTaskCreate(UpdateTempTask, (const portCHAR *)"temp_change_display", 128, NULL, 1, NULL);

}



// debounce returns the stable switch state
boolean debounce(int pin)
{
  boolean state;
  boolean previousState;

  previousState = !digitalRead(pin);          // store switch state
  for (int counter = 0; counter < debounceDelay; counter++)
  {
    vTaskDelay( 1 / portTICK_PERIOD_MS);                 // wait for 1 millisecond
    state = !digitalRead(pin);  // read the pin
    if ( state != previousState)
    {
      counter = 0; // reset the counter if the state changes
      previousState = state;  // and save the current state
    }
  }
  return state;
}
void TaskButton( void *pvParameters )
{
  //Function: Read the desired pins  and act accordingly
  (void) pvParameters;

  pinMode(left_button, INPUT_PULLUP);
  pinMode(middle_button, INPUT_PULLUP);
  pinMode(right_button, INPUT_PULLUP);
  int startaccl;
  int endaccl;
  int toggle = 1;
  for (;;)
  {
    
    //Start the heating process
    if (debounce(left_button) == false && debounce(middle_button) == false && debounce(right_button) == true)
    {
      startaccl = xTaskGetTickCount();
      while (debounce(right_button))
      {
        //what to do when the button is held? NOTHING ... yet
        taskYIELD();
      }

      endaccl = xTaskGetTickCount();
      if ((endaccl - startaccl) > (taprespone) && (endaccl - startaccl)  < holdresponse )
      {
     
        toggle +=1;

      } 
      
       
      if(toggle == 2){
        //turn on heater.
        isHeating = 3;
      //toggle = 1;
      }
      if(toggle == 3){
        //pause (stop the heater)
        isHeating = 4;
     
       toggle = 1;
      }
    }
    //Go into edit mode.
    
    if (debounce(left_button) == true && debounce(middle_button) == true && debounce(right_button) == false)
    {
       isHeating = 0;
           isEditMode = true;
      //Edit mode is now here.
      digitalWrite(RED, HIGH);
      digitalWrite(GREEN, HIGH);
      digitalWrite(BLUE, LOW);
       vTaskDelay( 1000 / portTICK_PERIOD_MS);    //delay is the key here
  
     
      for (;;) {
        if (debounce(left_button) == true)
        {
          startaccl = xTaskGetTickCount();
          while (debounce(left_button))
          {
            //what to do when the button is held? NOTHING ... yet
            taskYIELD();
          }
          endaccl = xTaskGetTickCount();
          //Serial.println(endaccl - startaccl);
          if ((endaccl - startaccl) <= taprespone) { //when the button is not held but tapped
            buffer_disp_temp_set += 1;
            vTaskDelay( 50 / portTICK_PERIOD_MS);    //delay is the key here
          } else if ((endaccl - startaccl) > taprespone && (endaccl - startaccl)  < holdresponse ) {
            buffer_disp_temp_set += 10;
            vTaskDelay( 25 / portTICK_PERIOD_MS);    //delay is the key here
          }

        }
        if (debounce(middle_button) == true)
        {
          startaccl = xTaskGetTickCount();
          while (debounce(middle_button))
          {
            //what to do when the button is held? NOTHING ... yet
            taskYIELD();
          }
          endaccl = xTaskGetTickCount();
          //Serial.println(endaccl - startaccl);
          if ((endaccl - startaccl) <= taprespone) { //when the button is not held but tapped
            if (buffer_disp_temp_set > 0) {
              buffer_disp_temp_set -= 1;
            }
            vTaskDelay( 50 / portTICK_PERIOD_MS);    //delay is the key here
          } else if ((endaccl - startaccl) > taprespone && (endaccl - startaccl)  < holdresponse ) {
            if (buffer_disp_temp_set > 0) {
              buffer_disp_temp_set -= 10;
            }
            vTaskDelay( 25 / portTICK_PERIOD_MS);    //delay is the key here
          }

        }
        if (debounce(right_button) == true)
        {
           EEPROM.update(addr,buffer_disp_temp_set);
          isEditMode = false;
          digitalWrite(RED, HIGH);
          digitalWrite(GREEN, HIGH);
          digitalWrite(BLUE, HIGH);
          toggle = 1;
          break;
        } //debounce(right_button)

      }
    }
  }
}
void TaskDisplay( void *pvParameters )
{
  //Function: change the contents of the display from a 'buffer' and select a 'mode' when editing
  (void) pvParameters;
  for (;;)
  {
    if (isEditMode)
    {
      tm1637.dispNumber(buffer_disp_temp_set);
      vTaskDelay( 250 / portTICK_PERIOD_MS);
      tm1637.offMode();
      vTaskDelay( 250 / portTICK_PERIOD_MS);
    }//isEditMode
    else
    {

      tm1637.dispNumber(buffer_disp_curr_temp);
      vTaskDelay( 25 / portTICK_PERIOD_MS);
    }
  }
}
void TempTask( void *pvParameters )
{
  //Function: Control the relay and set the required tempertue.
  (void) pvParameters;

  for (;;)
  {
    uint8_t i;
    float average;

    // take N samples in a row, with a slight delay
    for (i = 0; i < NUMSAMPLES; i++) {
      samples[i] = analogRead(THERMISTORPIN);
      vTaskDelay( 10 / portTICK_PERIOD_MS);
    }

    // average all the samples out
    average = 0;
    for (i = 0; i < NUMSAMPLES; i++) {
      average += samples[i];
    }
    average /= NUMSAMPLES;

    //Serial.print("Average analog reading ");
    // Serial.println(average);
    // convert the value to resistance
    average = 1023 / average - 1;
    average = SERIESRESISTOR / average;

    // Serial.print("Thermistor resistance ");
    //Serial.println(average);

   
    steinhart = average / THERMISTORNOMINAL;     // (R/Ro)
    steinhart = log(steinhart);                  // ln(R/Ro)
    steinhart /= BCOEFFICIENT;                   // 1/B * ln(R/Ro)
    steinhart += 1.0 / (TEMPERATURENOMINAL + 273.15); // + (1/To)
    steinhart = 1.0 / steinhart;                 // Invert
    steinhart -= 273.15;                         // convert to C

    
    
  if(isHeating == 3){
    if(steinhart < buffer_disp_temp_set )
    {
      //relay on.
      digitalWrite(RED, LOW);
      digitalWrite(GREEN, HIGH);
      digitalWrite(BLUE, HIGH);
      vTaskDelay( 250 / portTICK_PERIOD_MS); 
      digitalWrite(relay,HIGH);

    } else{
      //Temp reached or too low. relay off
      digitalWrite(RED, HIGH);
      digitalWrite(GREEN, LOW);
      digitalWrite(BLUE, HIGH);
      vTaskDelay( 250 / portTICK_PERIOD_MS); 
       digitalWrite(relay,LOW);
         
    }
     
  } else if (isHeating == 4){
    //Heating is paused.
    digitalWrite(RED, HIGH);
      digitalWrite(GREEN, HIGH);
      digitalWrite(BLUE, HIGH);
       digitalWrite(relay,LOW);
  }
  }
}
void UpdateTempTask( void *pvParameters )
{
  //Function: Update the current temperture on the display
  (void) pvParameters;

  for (;;)
  {
     buffer_disp_curr_temp = steinhart;
    vTaskDelay( 250 / portTICK_PERIOD_MS);
  }
}
void loop() {

}
