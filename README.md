# The Macro Pad

This macro pad is a custom-built USB device with a very simple purpose: to send commands to a Linux system. It's a small, keyboard-like device with the following features:

- 5 rotary encoders
- 10 Cherry MX Blue switches
- 2 linear faders

You can see an image of it here: ![image.png](assets/image.png).

There is no schematic or 3d model provided. 

The Linux-side driver is stored in the `rawhid` directory. It's a simple HID device that sends commands to the system.  
The driver is written in C. The device itself is a simple USB device that sends HID packets to the system. The packets contain the state of the switches and the values of the rotary encoders and faders.

The driver is designed to be simple. It's a single executable. The configuration file is a .ini file that specifies which commands to execute. There is an example config provided [here](rawhid/config.ini.example). the program expects a config at `~/.config/macropad/config.ini`.
