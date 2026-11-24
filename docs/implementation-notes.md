# Implementation notes

## `Blinky.c`: application control

`main()` initializes the LED, GLCD and joystick drivers and displays the menu. The firmware uses a polling superloop, not an RTOS. `Draw_Menu()` highlights the current selection; `Run_Gallery()`, `Run_MP3_Player()`, and `Run_Snake()` are invoked from the menu and return when the user exits.

## GPIO joystick

The five joystick inputs use port-1 bit masks. `Joystick_Init()` configures the pins for GPIO inputs with internal pull-ups. `Joystick_Read()` interprets a low input level as a pressed button. `Joystick_WaitRelease()` supplies a release wait and short debounce delays.

## Photo gallery

The main source references `photo1_pixels`, `photo2_pixels`, and `photo3_pixels`, which were stored in separate C header files in the full hardware project. The photos were resized using Microsoft Paint before conversion to 160x120 RGB565 pixel data.

## Snake

The game stores the x/y coordinates of its body segments in arrays with index 0 as the head. Movement updates coordinates, checks boundaries and self-collision, and redraws the affected tiles. Food positions come from a deterministic pseudo-random sequence with an occupancy check and fallback scan. Timing uses a busy-wait with `__NOP()`, not a dedicated game tick timer.

## USB audio

`Blinky.c` invokes `MediaAudio_Start()` / `MediaAudio_Stop()` through `MediaAudio.h`. The complete implementation exists in `MediaAudio.c` and other Keil-derived USB Audio example modules **not included here**.

The full project uses USB device audio streaming from the host Windows computer, an ADC input for the potentiometer, Timer0-driven audio processing, and DAC speaker output. Exiting audio mode disconnects the USB device and disables its Timer0 interrupt.

## Reproducing the original firmware

This portfolio repository is **not directly compilable** without external dependencies. A valid build requires authorized copies of the correct LPC1768 Keil/CMSIS source, GLCD and USB Audio example components, custom RGB565 images, and a suitable µVision project with its memory mapping (including the USB DMA scatter file).

Do not treat files in this repository as a complete hardware-ready firmware package.

## Known boundaries

- Three photo assets were excluded because the original image redistribution rights have not been confirmed.
- Vendor/source example files were excluded because their embedded copyright notices restrict reuse/redistribution.
- Build logs, `.axf` binaries, object files and old backup projects were omitted intentionally.
- The full integrated project previously built with zero errors and warnings in Keil; this curated excerpt has not been compiled independently.
