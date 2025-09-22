# Propulsion_Testbench
This project is a rocket propulsion testbench built using Arduino. It uses an HX711 load cell amplifier to measure thrust, logs the data to an SD card, and controls ignition through a MOSFET for safety.

The system indicates its current state through a buzzer and three LED strips, which are connected to the perfboard via screw terminals and wrapped around the motor test stand. The buzzer produces different tones, and the LED strips change colors to match each state.

The four states are as follows:
```
0 - Pre-Ignition
1 - Armed
2 - Standby
3 - Abort
```
The schematic for this testbench is created in KiCad (PDF attached for reference), and the circuit is designed using the open-source DIY Layout Creator.

The perfboard design allowed most components to be soldered as standalone modules, keeping the setup straightforward and making debugging easier during testing.

Overall, the system is aimed at providing a simple, reliable, and easy-to-use setup for testing and analyzing rocket motor performance.

<div align="center">
  <img src="https://github.com/user-attachments/assets/3c24aff0-6400-4be7-9cc8-288a73d4bf4d" width="45%" />
  <img src="https://github.com/user-attachments/assets/1ac51c0b-7310-43b8-8c5e-917663e9d29a" width="45%" />
</div>

<div align="center">
<img src="https://github.com/user-attachments/assets/34863cf1-c323-4de7-b558-85bb46bd599a" width="60%" />
</div>
