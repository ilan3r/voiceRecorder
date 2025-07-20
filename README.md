Description: 
- real-time voice recorder module to capture a recording, save it to micro SD card, and play it back
- uses DMA circular buffering to receive I2S Data from digital microphone while writing data to wav file on SD card 
- uses DMA circular buffering to receive audio data from wav file on SD card while sending data to amplifier for playback

In progress: 
- allow the user to view saved recordings and choose which one to play on the speaker

Next steps: 
- add display to navigate through recordings 
- add buttons (with timers/interrupts), button state tracking
- add state machine and menu navigation
- volume control 
- use morse code and a single button to name files 
- pause and play functionality during recording playback
- use FreeRTOS scheduling and shared resrouces to manage 


Hardware
- Nucelo-F401RE Development Board
- INMP441 Digital Microphone
- MAX98357A Digital Audio Amplifier
- 8Ω 4W Speaker
- Micro SD Card + Micro SD Card SPI Module

Resources/Tutorials
(Learning Embedded World Youtube) https://www.youtube.com/watch?v=bbmZk5bRrR4&t=36s 
(STM32 World Youtube) https://www.youtube.com/watch?v=rk8rMyOc76M&t=660s 
(Steppe School Youtube) https://www.youtube.com/watch?v=NJXrJQPO7jk

