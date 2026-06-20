# Elevator Controller using STM32F407

A microcontroller-based elevator control system which simulates a real elevator mechanism using the **STM32F407 Discovery board**, a **DC motor**, **IR floor sensors**, **push buttons**, a **7-segment display**, and **LED indicators**.

## Project Overview

This project presents the design and implementation of an intelligent elevator controller that can manage floor movement, request handling, visual indication, and emergency response. The system is designed to operate a localized elevator model and demonstrates core embedded systems concepts such as PWM, GPIO interfacing, sensor handling, and real-time control.

## Features

- PWM-based motor speed control
- Floor detection using IR sensors
- Inside cabin and hall call button support
- 7-segment display for floor indication
- Up/Down direction LEDs
- Emergency stop and safe return behavior
- Embedded C implementation using HAL libraries

## Hardware Used

- STM32F407 Discovery Board
- L298N Motor Driver Module
- DC Motor
- IR Proximity Sensors
- Tactile Push Buttons
- 7-Segment Display
- Status LEDs
- Veroboard and supporting components

## Software Tools

- STM32CubeMX
- STM32CubeIDE
- Embedded C
- KiCad / Proteus for schematic design

## System Functionality

The elevator controller supports:
- Floor selection from inside the cabin
- Hall call requests from outside
- Current floor display
- Direction indication
- Emergency override handling
- Smooth motor motion using PWM


## Project Demo

A working video demonstration is included in the repository.

