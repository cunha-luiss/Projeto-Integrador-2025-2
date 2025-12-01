#define SERVO_PIN 27
#include <ESP32Servo.h>
#include <Arduino.h>

Servo servo;

void setupServo(){

    servo.attach(SERVO_PIN);
    servo.write(20); // posição inicial

}

void moverServo(int graus){
    servo.write(graus);
}