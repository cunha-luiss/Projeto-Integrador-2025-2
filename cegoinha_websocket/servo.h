#ifndef SERVO_H  // Isso é um "Include Guard"
#define SERVO_H
#include <ESP32Servo.h>
#include <Arduino.h>

void setupServo();
void moverServo(int graus);
#endif