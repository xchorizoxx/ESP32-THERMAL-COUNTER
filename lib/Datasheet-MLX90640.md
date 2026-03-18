MLX90640 32x24 IR array
Datasheet
1. Features and Benefits
 Small size, low cost 32x24 pixels IR array
 Easy to integrate
 Industry standard four lead TO39 package
 Factory calibrated
 Noise Equivalent Temperature Difference
(NETD) 0.1K RMS @1Hz refresh rate
 I2C compatible digital interface
 Programmable refresh rate 0.5Hz…64Hz
 3.3V supply voltage
 Current consumption less than 23mA
 2 FOV options – 55°x35° and 110°x75°
 Operating temperature -40°C ÷ 85°C
 Target temperature -40°C ÷ 300°C
 Complies with RoHS regulations
2. Application Examples
 High precision non-contact temperature
measurements
 Intrusion / Movement detection
 Presence detection / Person localization
 Temperature sensing element for
intelligent building air conditioning
 Thermal Comfort sensor in automotive Air
Conditioning control system
 Microwave ovens

Industrial temperature control of moving
parts

Visual IR thermometers

Driver software for MCU available at:
https://github.com/melexis/mlx90640-
library.git
3. Description
The MLX90640 is a fully calibrated 32x24 pixels
thermal IR array in an industry standard 4-lead TO39
package with digital interface.
The MLX90640 contains 768 FIR pixels. An ambient
sensor
is
integrated
to
measure
the ambient
temperature of the chip and supply sensor to measure
the VDD. The outputs of all sensors IR, Ta and VDD are
stored in internal RAM and are accessible through I2C.
Array
M pixels
SDA
Band gap
reference and
PTAT sensor
EEPROM
I2C
SCL
M amplifiers
M ADC
Regulator for
digital part
34 MHz RC
oscillator
Storage RAM
Vss
Vdd
Figure 1 Block diagram

MLX90640 32x24 IR array
Datasheet
Page 2 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
Contents
1. Features and Benefits ............................................................................................................................ 1
2. Application Examples ............................................................................................................................. 1
3. Description ............................................................................................................................................ 1
4. Ordering Information ............................................................................................................................ 6
5. Glossary of Terms .................................................................................................................................. 7
6. Pin Definitions and Descriptions ............................................................................................................ 8
7. Absolute Maximum Ratings ................................................................................................................... 8
8. General Electrical Specifications ............................................................................................................ 9
9. False pixel correction ........................................................................................................................... 10
10. Detailed General Description ............................................................................................................. 10
10.1. Pixel position ................................................................................................................................... 10
10.2. Communication protocol ............................................................................................................... 11
10.2.1. Low level ................................................................................................................................... 11
10.3. Measurement mode ....................................................................................................................... 12
10.4. Refresh rate ..................................................................................................................................... 12
10.5. Measurement flow ......................................................................................................................... 13
10.6. Reading patterns ............................................................................................................................. 14
10.7. Address map ................................................................................................................................... 16
10.7.1. Internal registers....................................................................................................................... 16
10.7.2. RAM ........................................................................................................................................... 18
10.7.3. EEPROM .................................................................................................................................... 19
11. Calculating Object Temperature ........................................................................................................ 22
11.1. Restoring calibration data from EERPOM ..................................................................................... 22
11.1.1. Restoring the VDD sensor parameters .................................................................................... 22
11.1.2. Restoring the Ta sensor parameters ....................................................................................... 22
11.1.3. Restoring the offset .................................................................................................................. 23
11.1.4. Restoring the Sensitivity .................................................................................................. 24
11.1.5. Restoring the Kv(i,j) coefficient ................................................................................................ 25
11.1.6. Restoring the Kta(i,j) coefficient .............................................................................................. 25
11.1.7. Restoring the GAIN coefficient (common for all pixels) ......................................................... 26
11.1.8. Restoring the KsTa coefficient (common for all pixels) .......................................................... 26

MLX90640 32x24 IR array
Datasheet
Page 3 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
11.1.9. Restoring corner temperatures (common for all pixel) .......................................................... 26
11.1.10. Restoring the KsTo coefficient (common for all pixels) ........................................................ 27
11.1.11. Restoring sensitivity correction coefficients for each temperature range ......................... 27
11.1.12. Restoring the Sensitivity ............................................................................................... 28
11.1.13. Restoring the offset of the Compensation Pixel (CP) ........................................................... 28
11.1.14. Restoring the Kv CP coefficient .............................................................................................. 28
11.1.15. Restoring the Kta CP coefficient ............................................................................................ 28
11.1.16. Restoring the TGC coefficient ................................................................................................ 29
11.1.17. Restoring the resolution control coefficient ......................................................................... 29
11.2. Temperature Calculation ................................................................................................................ 30
11.2.1. Example Input Data .................................................................................................................. 30
11.2.2. Temperature calculation .......................................................................................................... 35
12. Performance graphs .......................................................................................................................... 47
12.1. Accuracy .......................................................................................................................................... 47
12.1.1. Pixel accuracy ............................................................................................................................ 47
12.1.2. Ta accuracy ............................................................................................................................... 48
12.2. Startup time .................................................................................................................................... 49
12.2.1. First valid data ........................................................................................................................... 49
12.2.2. Thermal behavior...................................................................................................................... 49
12.3. Noise performance and resolution ................................................................................................ 50
12.4. Field of view (FOV) .......................................................................................................................... 52
13. Application information ..................................................................................................................... 53
13.1. Optical considerations .................................................................................................................... 53
13.2. Electrical considerations ................................................................................................................ 53
13.3. Using the device in “image mode” ................................................................................................ 54
14. Application Comments ...................................................................................................................... 55
15. Mechanical drawings ......................................................................................................................... 56
15.1. FOV 55° ............................................................................................................................................ 56
15.2. FOV 110° ......................................................................................................................................... 57
15.3. Device marking ............................................................................................................................... 58
16. Standard Information ........................................................................................................................ 59
17. ESD Precautions ................................................................................................................................. 59
18. Revision history table ........................................................................................................................ 59

MLX90640 32x24 IR array
Datasheet
Page 4 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
19. Contact .............................................................................................................................................. 60
20. Disclaimer .......................................................................................................................................... 60
Tables
Table 1 Ordering information .......................................................................................................................................................... 6
Table 2 Glosarry of terms ................................................................................................................................................................ 7
Table 3 Pin definition ...................................................................................................................................................................... 8
Table 4 Absolute maximum ratings ................................................................................................................................................. 8
Table 5 Electrical specification ........................................................................................................................................................ 9
Table 6 Priorities of subpage controls (0x0800D) ............................................................................................................................17
Table 7 Configuration parameters memory ....................................................................................................................................19
Table 8 EEPROM to registers mapping ............................................................................................................................................19
Table 9 EEPROM overview (words) .................................................................................................................................................20
Table 10 Calibration parameters memory (EEPROM - bits) ..............................................................................................................21
Table 11 Calculation example input data ........................................................................................................................................30
Table 12 Calculation example calibration data ................................................................................................................................34
Table 13 XOR truth table ................................................................................................................................................................42
Table 14 Noise performance ..........................................................................................................................................................51
Table 15 Available FOV options ......................................................................................................................................................52
Figures
Figure 1 Block diagram ................................................................................................................................................................... 1
Figure 2 MLX90640 Overview and pin description ........................................................................................................................... 8
Figure 3 Pixel in the whole FOV ......................................................................................................................................................10
Figure 4 I2C write command format (default SA=0x33 is used) ........................................................................................................11
Figure 5 I2C read command format (default SA=0x33 is used) .........................................................................................................11
Figure 6 Refresh rate timing ...........................................................................................................................................................12
Figure 7 Recommended measurement flow ...................................................................................................................................13
Figure 8 TV mode reading pattern (only highlighted cells are updated) ...........................................................................................15
Figure 9 Chess reading pattern (only highlighted cells are updated) ................................................................................................15
Figure 10 MXL90640 memory map .................................................................................................................................................16
Figure 11 Status register (0x8000) bits meaning .............................................................................................................................16
Figure 12 Control register1 (0x800D) bits meaning .........................................................................................................................17
Figure 13 I2C configuration register (0x800F) bits meaning .............................................................................................................18
Figure 14 RAM memory map (Chess pattern mode) – factory default mode ....................................................................................18
Figure 15 RAM memory map (Interleaved mode) ...........................................................................................................................18
Figure 16 To calculation flow .........................................................................................................................................................35
Figure 17 Absolute temperature accuracy – MLX90640BAA (left) and MLX90640BAB (right) ...........................................................47
Figure 19 MLX90640BAx noise vs refresh rate for different device types .........................................................................................50
Figure 20 MLX90640BAA noise vs pixel and refresh rate at 1Hz and 2Hz .........................................................................................50
Figure 21 MLX90640BAA noise vs pixel and refresh rate at 4Hz, 8Hz and 16Hz ................................................................................50
Figure 22 MLX90640BAB noise vs pixel and refresh rate at 1Hz and 2Hz .........................................................................................51
Figure 23 MLX90640BAB noise vs pixel and refresh rate at 4Hz, 8Hz and 16Hz ................................................................................51
Figure 24 Field Of View measurement ............................................................................................................................................52

MLX90640 32x24 IR array
Datasheet
Page 5 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
Figure 26 MLX90640 electrical connections ....................................................................................................................................53
Figure 27 Calculation flow in thermal image mode .........................................................................................................................54
Figure 28 Mechanical drawing of 55° FOV device ............................................................................................................................56
Figure 29 Mechanical drawing of 110° FOV device ..........................................................................................................................57

MLX90640 32x24 IR array
Datasheet
Page 6 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
4. Ordering Information
Product
Temperature
Package
Option Code
Custom
Configuration
Packing
Form
Definition
MLX90640
E
SF
BAA
000
TU
32x24 IR array
MLX90640
E
SF
BAB
000
TU
32x24 IR array
Legend:
Temperature Code:
E: -40°C to 85°C
Package Code:
“SF” for TO39 package
Option Code:
xAx – TGC is disabled and may not be changed
Option Code:
xxA – FOV = 110°x75°
xxB – FOV = 55°x35°
Custom configuration
000 – standard product
Packing Form:
“TU” - Tubes
Ordering Example:
“MLX90640ESF-BAA-000-TU”
Table 1 Ordering information

MLX90640 32x24 IR array
Datasheet
Page 7 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
5. Glossary of Terms
TC
Temperature Coefficient (in ppm/°C)
POR
Power On Reset
IR
Infra-Red
Ta
Ambient Temperature – the temperature of the TO39 package
IR data
Infrared data (raw data from ADC proportional to IR energy received by the sensor)
ADC
Analog To Digital Converter
TGC
Temperature Gradient Coefficient
FOV
Field Of View
nFOV
Field Of View of the N-th pixel
I2C
Inter-Integrated Circuit communication protocol
SDA
Serial Data
SCL
Serial Clock
LSB
Least Significant Bit
MSB
Most Significant Bit
Fps
Frames per Second – data refresh rate
MD
Master Device
SD
Slave Device
ASP
Analog Signal Processing
DSP
Digital Signal Processing
ESD
Electro Static Discharge
EMC
Electro Magnetic Compatibility
CP
Compensation Pixel
NC
Not Connected
NA
Not Applicable
TBD
To Be Defined
Table 2 Glosarry of terms

MLX90640 32x24 IR array
Datasheet
Page 8 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
6. Pin Definitions and Descriptions
Pin #
Name
Description
1
SDA
I2C serial data (input / output)
2
VDD
Positive supply
3
GND
Negative supply (Ground)
4
SCL
I2C serial clock (input only)
Table 3 Pin definition
Figure 2 MLX90640 Overview and pin description
7. Absolute Maximum Ratings
Parameter
Symbol
Min.
Typ.
Max.
Unit
Remark
Supply Voltage (over voltage)
VDD
5
V
Supply Voltage (operating max voltage)
VDD
3.6
Reverse Voltage (each pin)
-0.3
V
Operating Temperature
TAMB
-40
+85
°C
Storage Temperature
TST
-40
+85
°C
Not in plastic tubes
ESD sensitivity (AEC Q100 002)
4
kV
SDA DC sink current
40
mA
Table 4 Absolute maximum ratings
Exceeding the absolute maximum ratings may cause permanent damage. Exposure to absolute maximum-rated
conditions for extended periods may affect device reliability.

MLX90640 32x24 IR array
Datasheet
Page 9 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
8. General Electrical Specifications
Electrical Parameter
Symbol
Min.
Typ.
Max.
Unit
Condition
Supply Voltage
VDD
3
3.3
3.6
V
Supply Current
IDD
15
20
25
mA
POR level up analog
VPOR_UP
2.2
2.6
V
VDD rising
POR level down analog
VPOR_DOWN
2.55
V
VDD falling
POR hysteresis
VPOR_hys
50
mV
I2C address(NOTE 3)
0x01
0x33(default)
0xFF
Input high voltage
(SDA, SCL)
VIH
0.7*VDD
V
Over Ta and VDD
Input low voltage
(SDA, SCL)
VLOW
0.3*VDD
V
Over Ta and VDD
SDA output low voltage
VOL
0.4
V
Over Ta and VDD
ISINK=3mA
SDA leakage
ISDA_leak
± 10
µA
VSDA=3.6V, Ta=85°C
SCL leakage
ISCL_leak
± 10
µA
VSCL=3.6V, Ta=85°C
SDA capacitance
CSDA
10
pF
SCL capacitance
CSCL
10
pF
Acknowledge setup time
TSUAC(MD)
0.45
µs
Acknowledge hold time
TDUAC(MD)
0.45
µs
Acknowledge setup time
TSUAC(SD)
0.45
µs
Acknowledge hold time
TDUAC(SD)
0.45
µs
I2C clock frequency
FI2C
0.4
1
MHz
EEPROM erase/write cycles
10
times
Write cell time
TWRITE
5
ms
Table 5 Electrical specification
NOTE 1: For best performance it is recommended to keep the supply voltage as accurate and stable as possible to 3.3V
± 0.05V
NOTE 2: When a data in EEPROM cell to be changed an erase (write 0x0000) must be done prior to writing the new
value. After each write at least 5ms delay is needed in order to writing process to take place.
NOTE 3: Slave address 0x00 must be avoided.
NOTE 4: According to I2C standard the max sink current is specified to be 20mA, however due to the thermal
considerations (the dissipated power into the driver) the max current is limited to 10mA. This is the only parameter
which does not comply with the FM+ specification.
NOTE 5: Max EEPROM I2C speed operations to be done at 400kHz

MLX90640 32x24 IR array
Datasheet
Page 10 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
9. False pixel correction
The imager can have up to 4 defective pixels, with either no output or out of specification temperature reading.
These pixels are identified in the EEPROM table of the sensor and can be read out through the I 2C. The defective pixel
result can be replaced by an interpolation of its neighboring pixels.
10. Detailed General Description
10.1. Pixel position
The array consists of 768 IR sensors (also called pixels). Each pixel is identified with its row and column position as
Pix(i,j) where i is its row number (from 1 to 24) and j is its column number (from 1 to 32)
Figure 3 Pixel in the whole FOV
Row 1
Col 32
Col 3
Col 2
Row 2
Row 3
Row 24
Col 1
0
Reference tab
GND
VDD
SCL
SDA

MLX90640 32x24 IR array
Datasheet
Page 11 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
10.2. Communication protocol
The device use I2C protocol with support of FM+ mode (up to 1MHz clock frequency) and can be only slave on the bus.
The SDA and SCL ports are 5V tolerant and the sensor can be directly connected to a 5V I2C network.
The slave address is programmable and can have up to 127 different slave addresses.
10.2.1. Low level
10.2.1.1. Start / Stop conditions
Each communication session is initiated by a START condition and ends with a STOP condition. A START condition is
initiated by a HIGH to LOW transition of the SDA while a STOP is generated by a LOW to HIGH transition. Both changes must
be done while the SCL is HIGH.
10.2.1.2. Device addressing
The master is addressing the slave device by sending a 7-bit slave address after the START condition. The first seven bits are
dedicated for the address and the 8th is Read/Write (R/W) bit. This bit indicates the direction of the transfer:

Read (HIGH) means that the master will read the data from the slave

Write (LOW) means that the master will send data to the slave
10.2.1.3. Acknowledge
During the 9th clock following every byte transfer the transmitter releases the SDA line. The receiver acknowledges
(ACK) receiving the byte by pulling SDA line to low or does not acknowledge (NoACK) by letting the SDA ‘HIGH’.
10.2.1.4. I2C command format
Figure 4 I2C write command format (default SA=0x33 is used)
Figure 5 I2C read command format (default SA=0x33 is used)
SCL
SDA
1
0
1
0
A
A
S
A
A
A
P
Slave address
W
1
1
0
MSByte address
LSByte address
MSByte data
LSByte data
I2C write
SCL
SDA
1
0
1
0
A
A
S
A
A
NAK P
Slave address
W
1
1
0
MSByte address
LSByte address
MSByte data
LSByte data
I2C read
S
1
0
1
0
R
1
1
0
A
Slave address

MLX90640 32x24 IR array
Datasheet
Page 12 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
10.3. Measurement mode
In this mode the measurements are constantly running. Depending on the selected frame rate Fps in the control
register, the data for IR pixels and Ta will be updated in the RAM each
second. In this mode the external microcontroller
has full access to the internal registers and memories of the device.
10.4. Refresh rate
The refresh rate is configured by “Control register 1” (0x800D) i.e. if “Refresh rate control” = 011  4Hz this would mean
that each 250ms a new subpage data is available in the RAM.
NOTE: It is possible to program the desired refresh rate into device EEPROM eliminating the necessity to reconfigure the
device every time it is powered on. The corresponding EEPROM cell is at address 0x240C (see Table 8)
Which subpage is updated is indicated by the “Last measured subpage” field.
It is important to read both subpages as the necessary information for the Ta calculations is only available by combining the
data from both subpages i.e. the Ta is refreshed with an update speed twice as low as the one set in “Refresh rate control”.
When a complete new data set (subpage) is available, a dedicated bit is set to indicate this – bit 3 “New data available in
RAM” in “Status register” (0x8000). It is up to the customer to reset the bit once the data has been read.
Figure 6 Refresh rate timing
Subpage 0
Subpage 1
Subpage 0
Subpage 1
Refresh rate control = 011b (4Hz)
250ms
250ms
250ms
250ms
Set bit “New data available in RAM”

MLX90640 32x24 IR array
Datasheet
Page 13 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
10.5. Measurement flow
Following measurement flow is recommended:
POR
Wait 80ms + delay determined by the refresh rate
Absolute temp measurement?
Yes
0.5 Hz  4 sec
1 Hz  2 sec
2 Hz  1 sec
….
64 Hz  0.03125 sec
No
Wait app 4 min
Read meas data
Clear bit “New data available in RAM” - Bit3 in 0x8000
Wait time determined by RR – 20%
Measurement Flow
Is “New data available in RAM” set
No
Sub frame “0”
Yes
Read meas data
Clear bit “New data available in RAM” - Bit3 in 0x8000
Calculate the temperature of the sub frame “1”
Sub frame “1”
Calculate the temperature of the sub frame “0”
Wait time determined by RR – 20%
Is “New data available in RAM” set
No
Yes
Image processing
decision making
Step mode ?
Set Start Of Measurement – Bit5 in 0x8000
Yes
No
Step mode ?
Set Start Of Measurement – Bit5 in 0x8000
Yes
No
Image processing
decision making
Extract calibration data from EEPROM and store in RAM
Just once after POR
Figure 7 Recommended measurement flow

MLX90640 32x24 IR array
Datasheet
Page 14 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
10.6. Reading patterns
The array frame is divided in two subpages and depending of bit 12 in “Control register 1” (0x800D) – “Reading pattern”
there are two modes of the pixel arrangement:
-
Chess pattern mode (factory default)
-
TV interleave mode
NOTE1: As a standard the MLX90640 is calibrated in Chess pattern mode, this results in better fixed pattern noise behaviour
of the sensor when in chess pattern mode. For best results Melexis advices to use chess pattern mode.
NOTE2: Please make sure a proper configuration of the subpage control bit is done. See: Table 6 Priorities of subpage controls

MLX90640 32x24 IR array
Datasheet
Page 15 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
Figure 8 TV mode reading pattern (only highlighted cells are updated)
Figure 9 Chess reading pattern (only highlighted cells are updated)
0x0400
1
2
3
4
5
6
7
8
9
10
11
12
13
14
15
16
17
18
19
20
21
22
23
24
25
26
27
28
29
30
31
32
0x0400
1
2
3
4
5
6
7
8
9
10
11
12
13
14
15
16
17
18
19
20
21
22
23
24
25
26
27
28
29
30
31
32
0x0420
33
34
35
36
37
38
39
40
41
42
43
44
45
46
47
48
49
50
51
52
53
54
55
56
57
58
59
60
61
62
63
64
0x0420
33
34
35
36
37
38
39
40
41
42
43
44
45
46
47
48
49
50
51
52
53
54
55
56
57
58
59
60
61
62
63
64
0x0440
65
66
67
68
69
70
71
72
73
74
75
76
77
78
79
80
81
82
83
84
85
86
87
88
89
90
91
92
93
94
95
96
0x0440
65
66
67
68
69
70
71
72
73
74
75
76
77
78
79
80
81
82
83
84
85
86
87
88
89
90
91
92
93
94
95
96
0x0460
97
98
99 100 101 102 103 104 105 106 107 108 109 110 111 112 113 114 115 116 117 118 119 120 121 122 123 124 125 126 127 128
0x0460
97
98
99 100 101 102 103 104 105 106 107 108 109 110 111 112 113 114 115 116 117 118 119 120 121 122 123 124 125 126 127 128
0x0480 129 130 131 132 133 134 135 136 137 138 139 140 141 142 143 144 145 146 147 148 149 150 151 152 153 154 155 156 157 158 159 160
0x0480 129 130 131 132 133 134 135 136 137 138 139 140 141 142 143 144 145 146 147 148 149 150 151 152 153 154 155 156 157 158 159 160
0x04A0 161 162 163 164 165 166 167 168 169 170 171 172 173 174 175 176 177 178 179 180 181 182 183 184 185 186 187 188 189 190 191 192
0x04A0 161 162 163 164 165 166 167 168 169 170 171 172 173 174 175 176 177 178 179 180 181 182 183 184 185 186 187 188 189 190 191 192
0x04C0 193 194 195 196 197 198 199 200 201 202 203 204 205 206 207 208 209 210 211 212 213 214 215 216 217 218 219 220 221 222 223 224
0x04C0 193 194 195 196 197 198 199 200 201 202 203 204 205 206 207 208 209 210 211 212 213 214 215 216 217 218 219 220 221 222 223 224
0x04E0 225 226 227 228 229 230 231 232 233 234 235 236 237 238 239 240 241 242 243 244 245 246 247 248 249 250 251 252 253 254 255 256
0x04E0 225 226 227 228 229 230 231 232 233 234 235 236 237 238 239 240 241 242 243 244 245 246 247 248 249 250 251 252 253 254 255 256
0x0500 257 258 259 260 261 262 263 264 265 266 267 268 269 270 271 272 273 274 275 276 277 278 279 280 281 282 283 284 285 286 287 288
0x0500 257 258 259 260 261 262 263 264 265 266 267 268 269 270 271 272 273 274 275 276 277 278 279 280 281 282 283 284 285 286 287 288
0x0520 289 290 291 292 293 294 295 296 297 298 299 300 301 302 303 304 305 306 307 308 309 310 311 312 313 314 315 316 317 318 319 320
0x0520 289 290 291 292 293 294 295 296 297 298 299 300 301 302 303 304 305 306 307 308 309 310 311 312 313 314 315 316 317 318 319 320
0x0540 321 322 323 324 325 326 327 328 329 330 331 332 333 334 335 336 337 338 339 340 341 342 343 344 345 346 347 348 349 350 351 352
0x0540 321 322 323 324 325 326 327 328 329 330 331 332 333 334 335 336 337 338 339 340 341 342 343 344 345 346 347 348 349 350 351 352
0x0560 353 354 355 356 357 358 359 360 361 362 363 364 365 366 367 368 369 370 371 372 373 374 375 376 377 378 379 380 381 382 383 384
0x0560 353 354 355 356 357 358 359 360 361 362 363 364 365 366 367 368 369 370 371 372 373 374 375 376 377 378 379 380 381 382 383 384
0x0580 385 386 387 388 389 390 391 392 393 394 395 396 397 398 399 400 401 402 403 404 405 406 407 408 409 410 411 412 413 414 415 416
0x0580 385 386 387 388 389 390 391 392 393 394 395 396 397 398 399 400 401 402 403 404 405 406 407 408 409 410 411 412 413 414 415 416
0x05A0 417 418 419 420 421 422 423 424 425 426 427 428 429 430 431 432 433 434 435 436 437 438 439 440 441 442 443 444 445 446 447 448
0x05A0 417 418 419 420 421 422 423 424 425 426 427 428 429 430 431 432 433 434 435 436 437 438 439 440 441 442 443 444 445 446 447 448
0x05C0 449 450 451 452 453 454 455 456 457 458 459 460 461 462 463 464 465 466 467 468 469 470 471 472 473 474 475 476 477 478 479 480
0x05C0 449 450 451 452 453 454 455 456 457 458 459 460 461 462 463 464 465 466 467 468 469 470 471 472 473 474 475 476 477 478 479 480
0x05E0 481 482 483 484 485 486 487 488 489 490 491 492 493 494 495 496 497 498 499 500 501 502 503 504 505 506 507 508 509 510 511 512
0x05E0 481 482 483 484 485 486 487 488 489 490 491 492 493 494 495 496 497 498 499 500 501 502 503 504 505 506 507 508 509 510 511 512
0x0600 513 514 515 516 517 518 519 520 521 522 523 524 525 526 527 528 529 530 531 532 533 534 535 536 537 538 539 540 541 542 543 544
0x0600 513 514 515 516 517 518 519 520 521 522 523 524 525 526 527 528 529 530 531 532 533 534 535 536 537 538 539 540 541 542 543 544
0x0620 545 546 547 548 549 550 551 552 553 554 555 556 557 558 559 560 561 562 563 564 565 566 567 568 569 570 571 572 573 574 575 576
0x0620 545 546 547 548 549 550 551 552 553 554 555 556 557 558 559 560 561 562 563 564 565 566 567 568 569 570 571 572 573 574 575 576
0x0640 577 578 579 580 581 582 583 584 585 586 587 588 589 590 591 592 593 594 595 596 597 598 599 600 601 602 603 604 605 606 607 608
0x0640 577 578 579 580 581 582 583 584 585 586 587 588 589 590 591 592 593 594 595 596 597 598 599 600 601 602 603 604 605 606 607 608
0x0660 609 610 611 612 613 614 615 616 617 618 619 620 621 622 623 624 625 626 627 628 629 630 631 632 633 634 635 636 637 638 639 640
0x0660 609 610 611 612 613 614 615 616 617 618 619 620 621 622 623 624 625 626 627 628 629 630 631 632 633 634 635 636 637 638 639 640
0x0680 641 642 643 644 645 646 647 648 649 650 651 652 653 654 655 656 657 658 659 660 661 662 663 664 665 666 667 668 669 670 671 672
0x0680 641 642 643 644 645 646 647 648 649 650 651 652 653 654 655 656 657 658 659 660 661 662 663 664 665 666 667 668 669 670 671 672
0x06A0 673 674 675 676 677 678 679 680 681 682 683 684 685 686 687 688 689 690 691 692 693 694 695 696 697 698 699 700 701 702 703 704
0x06A0 673 674 675 676 677 678 679 680 681 682 683 684 685 686 687 688 689 690 691 692 693 694 695 696 697 698 699 700 701 702 703 704
0x06C0 705 706 707 708 709 710 711 712 713 714 715 716 717 718 719 720 721 722 723 724 725 726 727 728 729 730 731 732 733 734 735 736
0x06C0 705 706 707 708 709 710 711 712 713 714 715 716 717 718 719 720 721 722 723 724 725 726 727 728 729 730 731 732 733 734 735 736
0x06E0 737 738 739 740 741 742 743 744 745 746 747 748 749 750 751 752 753 754 755 756 757 758 759 760 761 762 763 764 765 766 767 768
0x06E0 737 738 739 740 741 742 743 744 745 746 747 748 749 750 751 752 753 754 755 756 757 758 759 760 761 762 763 764 765 766 767 768
Subpage 0 --> 0x8000 = 0xXXX8
Subpage 1 --> 0x8000 = 0xXXX9
0x0400
1
2
3
4
5
6
7
8
9
10
11
12
13
14
15
16
17
18
19
20
21
22
23
24
25
26
27
28
29
30
31
32
0x0400
1
2
3
4
5
6
7
8
9
10
11
12
13
14
15
16
17
18
19
20
21
22
23
24
25
26
27
28
29
30
31
32
0x0420
33
34
35
36
37
38
39
40
41
42
43
44
45
46
47
48
49
50
51
52
53
54
55
56
57
58
59
60
61
62
63
64
0x0420
33
34
35
36
37
38
39
40
41
42
43
44
45
46
47
48
49
50
51
52
53
54
55
56
57
58
59
60
61
62
63
64
0x0440
65
66
67
68
69
70
71
72
73
74
75
76
77
78
79
80
81
82
83
84
85
86
87
88
89
90
91
92
93
94
95
96
0x0440
65
66
67
68
69
70
71
72
73
74
75
76
77
78
79
80
81
82
83
84
85
86
87
88
89
90
91
92
93
94
95
96
0x0460
97
98
99 100 101 102 103 104 105 106 107 108 109 110 111 112 113 114 115 116 117 118 119 120 121 122 123 124 125 126 127 128
0x0460
97
98
99 100 101 102 103 104 105 106 107 108 109 110 111 112 113 114 115 116 117 118 119 120 121 122 123 124 125 126 127 128
0x0480 129 130 131 132 133 134 135 136 137 138 139 140 141 142 143 144 145 146 147 148 149 150 151 152 153 154 155 156 157 158 159 160
0x0480 129 130 131 132 133 134 135 136 137 138 139 140 141 142 143 144 145 146 147 148 149 150 151 152 153 154 155 156 157 158 159 160
0x04A0 161 162 163 164 165 166 167 168 169 170 171 172 173 174 175 176 177 178 179 180 181 182 183 184 185 186 187 188 189 190 191 192
0x04A0 161 162 163 164 165 166 167 168 169 170 171 172 173 174 175 176 177 178 179 180 181 182 183 184 185 186 187 188 189 190 191 192
0x04C0 193 194 195 196 197 198 199 200 201 202 203 204 205 206 207 208 209 210 211 212 213 214 215 216 217 218 219 220 221 222 223 224
0x04C0 193 194 195 196 197 198 199 200 201 202 203 204 205 206 207 208 209 210 211 212 213 214 215 216 217 218 219 220 221 222 223 224
0x04E0 225 226 227 228 229 230 231 232 233 234 235 236 237 238 239 240 241 242 243 244 245 246 247 248 249 250 251 252 253 254 255 256
0x04E0 225 226 227 228 229 230 231 232 233 234 235 236 237 238 239 240 241 242 243 244 245 246 247 248 249 250 251 252 253 254 255 256
0x0500 257 258 259 260 261 262 263 264 265 266 267 268 269 270 271 272 273 274 275 276 277 278 279 280 281 282 283 284 285 286 287 288
0x0500 257 258 259 260 261 262 263 264 265 266 267 268 269 270 271 272 273 274 275 276 277 278 279 280 281 282 283 284 285 286 287 288
0x0520 289 290 291 292 293 294 295 296 297 298 299 300 301 302 303 304 305 306 307 308 309 310 311 312 313 314 315 316 317 318 319 320
0x0520 289 290 291 292 293 294 295 296 297 298 299 300 301 302 303 304 305 306 307 308 309 310 311 312 313 314 315 316 317 318 319 320
0x0540 321 322 323 324 325 326 327 328 329 330 331 332 333 334 335 336 337 338 339 340 341 342 343 344 345 346 347 348 349 350 351 352
0x0540 321 322 323 324 325 326 327 328 329 330 331 332 333 334 335 336 337 338 339 340 341 342 343 344 345 346 347 348 349 350 351 352
0x0560 353 354 355 356 357 358 359 360 361 362 363 364 365 366 367 368 369 370 371 372 373 374 375 376 377 378 379 380 381 382 383 384
0x0560 353 354 355 356 357 358 359 360 361 362 363 364 365 366 367 368 369 370 371 372 373 374 375 376 377 378 379 380 381 382 383 384
0x0580 385 386 387 388 389 390 391 392 393 394 395 396 397 398 399 400 401 402 403 404 405 406 407 408 409 410 411 412 413 414 415 416
0x0580 385 386 387 388 389 390 391 392 393 394 395 396 397 398 399 400 401 402 403 404 405 406 407 408 409 410 411 412 413 414 415 416
0x05A0 417 418 419 420 421 422 423 424 425 426 427 428 429 430 431 432 433 434 435 436 437 438 439 440 441 442 443 444 445 446 447 448
0x05A0 417 418 419 420 421 422 423 424 425 426 427 428 429 430 431 432 433 434 435 436 437 438 439 440 441 442 443 444 445 446 447 448
0x05C0 449 450 451 452 453 454 455 456 457 458 459 460 461 462 463 464 465 466 467 468 469 470 471 472 473 474 475 476 477 478 479 480
0x05C0 449 450 451 452 453 454 455 456 457 458 459 460 461 462 463 464 465 466 467 468 469 470 471 472 473 474 475 476 477 478 479 480
0x05E0 481 482 483 484 485 486 487 488 489 490 491 492 493 494 495 496 497 498 499 500 501 502 503 504 505 506 507 508 509 510 511 512
0x05E0 481 482 483 484 485 486 487 488 489 490 491 492 493 494 495 496 497 498 499 500 501 502 503 504 505 506 507 508 509 510 511 512
0x0600 513 514 515 516 517 518 519 520 521 522 523 524 525 526 527 528 529 530 531 532 533 534 535 536 537 538 539 540 541 542 543 544
0x0600 513 514 515 516 517 518 519 520 521 522 523 524 525 526 527 528 529 530 531 532 533 534 535 536 537 538 539 540 541 542 543 544
0x0620 545 546 547 548 549 550 551 552 553 554 555 556 557 558 559 560 561 562 563 564 565 566 567 568 569 570 571 572 573 574 575 576
0x0620 545 546 547 548 549 550 551 552 553 554 555 556 557 558 559 560 561 562 563 564 565 566 567 568 569 570 571 572 573 574 575 576
0x0640 577 578 579 580 581 582 583 584 585 586 587 588 589 590 591 592 593 594 595 596 597 598 599 600 601 602 603 604 605 606 607 608
0x0640 577 578 579 580 581 582 583 584 585 586 587 588 589 590 591 592 593 594 595 596 597 598 599 600 601 602 603 604 605 606 607 608
0x0660 609 610 611 612 613 614 615 616 617 618 619 620 621 622 623 624 625 626 627 628 629 630 631 632 633 634 635 636 637 638 639 640
0x0660 609 610 611 612 613 614 615 616 617 618 619 620 621 622 623 624 625 626 627 628 629 630 631 632 633 634 635 636 637 638 639 640
0x0680 641 642 643 644 645 646 647 648 649 650 651 652 653 654 655 656 657 658 659 660 661 662 663 664 665 666 667 668 669 670 671 672
0x0680 641 642 643 644 645 646 647 648 649 650 651 652 653 654 655 656 657 658 659 660 661 662 663 664 665 666 667 668 669 670 671 672
0x06A0 673 674 675 676 677 678 679 680 681 682 683 684 685 686 687 688 689 690 691 692 693 694 695 696 697 698 699 700 701 702 703 704
0x06A0 673 674 675 676 677 678 679 680 681 682 683 684 685 686 687 688 689 690 691 692 693 694 695 696 697 698 699 700 701 702 703 704
0x06C0 705 706 707 708 709 710 711 712 713 714 715 716 717 718 719 720 721 722 723 724 725 726 727 728 729 730 731 732 733 734 735 736
0x06C0 705 706 707 708 709 710 711 712 713 714 715 716 717 718 719 720 721 722 723 724 725 726 727 728 729 730 731 732 733 734 735 736
0x06E0 737 738 739 740 741 742 743 744 745 746 747 748 749 750 751 752 753 754 755 756 757 758 759 760 761 762 763 764 765 766 767 768
0x06E0 737 738 739 740 741 742 743 744 745 746 747 748 749 750 751 752 753 754 755 756 757 758 759 760 761 762 763 764 765 766 767 768
Subpage 0 --> 0x8000 = 0xXXX8
Subpage 1 --> 0x8000 = 0xXXX9

MLX90640 32x24 IR array
Datasheet
Page 16 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
10.7. Address map
Figure 10 MXL90640 memory map
10.7.1. Internal registers
There are a few internal registers that are customer accessible through which the device performance can be
customized:
Figure 11 Status register (0x8000) bits meaning
0x0000
0x03FF
ROM
0x0400
0x07FF
RAM
0x2400
0x273F
EEPROM
0x800D
0x8010
Registers
0x8000
0x800C
Registers
(MLX reserved)
0x8011
0x8016
Registers
(MLX reserved)
B15
B14
B13
B12
B11
B10
B9
B8
B7
B6
B5
B4
B3
B2
B1
B0
Enable overwrite
New data available in RAM
Status register - 0x8000
0
0
0
Measurement of subpage 0 has been measured
0
0
1
Measurement of subpage 1 has been measured
0
1
0
Melxis reserved
0
1
1
Melxis reserved
1
0
0
Melxis reserved
1
0
1
Melxis reserved
1
1
0
Melxis reserved
1
1
1
Melxis reserved
0
No new data is available in RAM (must be reset by the customer)
1
A new data is available in RAM
0
Data in RAM overwrite is disabled
1
Data in RAM overwrite is enabled
-
-
-
-
-
-
-
-
-
-
-
Melexis reserved
Last measured subpage
controlled by MLX90641
Melexis reserved

MLX90640 32x24 IR array
Datasheet
Page 17 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
Figure 12 Control register1 (0x800D) bits meaning
Enable subpage mode
(Bit 0)
Enable subpage repeat
(Bit 3)
Select subpage
(Bit 4)
Working mode
0
0
-
measure subpage 0 only
0
1
-
measure subpage 0 only
1
0
-
0  1  0  1 …
1
1
0
measure subpage 0 only
1
1
1
measure subpage 1 only
Table 6 Priorities of subpage controls (0x0800D)
B15
B14
B13
B12
B11
B10
B9
B8
B7
B6
B5
B4
B3
B2
B1
B0
Reading pattern
Enable subpages repeat
Enable data hold
Melexis reserved
Enable subpages mode
Control register 1 - 0x800D
0
No subpages, only one page will be measured
1
Subpade mode is activated (default)
0
Keep this bit = "0" (default)
0
Transfer the data into storage RAM at each measured frame (default)
1
Transfer the data into storage RAM only if en_overwrite = 1 (check 0x8000)
0
Toggles between subpage "0" and subpage "1" if Enable subpages mode = "1" (default)
1
Select subpage determines which subpage to be measured if Enable subpages mode = "1"
0
0
0
Subpage 0 is selected (default)
0
0
1
Subpage 1 is selected
0
1
0
Not Applicable
0
1
1
Not Applicable
1
0
0
Not Applicable
1
0
1
Not Applicable
1
1
0
Not Applicable
1
1
1
Not Applicable
0
0
0
IR refresh rate = 0.5Hz
0
0
1
IR refresh rate = 1Hz
0
1
0
IR refresh rate = 2Hz (default)
0
1
1
IR refresh rate = 4Hz
1
0
0
IR refresh rate = 8Hz
1
0
1
IR refresh rate = 16Hz
1
1
0
IR refresh rate = 32Hz
1
1
1
IR refresh rate = 64Hz
0
0
ADC set to 16 bit resolution
0
1
ADC set to 17 bit resolution
1
0
ADC set to 18 bit resolution (default)
1
1
ADC set to 19 bit resolution
0
Interleaved (TV) mode
1
Chess pattern (default)
-
-
-
Melexis reserved
Melexis reserved
Resolution control
Refresh rate control
Select subpage

MLX90640 32x24 IR array
Datasheet
Page 18 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
Figure 13 I2C configuration register (0x800F) bits meaning
10.7.2. RAM
Figure 14 RAM memory map (Chess pattern mode) – factory default mode
Figure 15 RAM memory map (Interleaved mode)
B7
B6
B5
B4
B3
B2
B1
B0
B7
B6
B5
B4
B3
B2
B1
B0
Melexis reserved
SDA driver
current limit control
I2C threshold levels
FM+ disable
I2C configuration register - 0x800F
0
FM+ mode enabled (default)
1
FM+ mode disabled
0
VDD reffered threshold (normal mode) (default)
1
1.8V reffered threshold (1.8V mode)
0
SDA driver current limit is ON (default)
1
SDA driver current limit is OFF
0
Melexis reserved
-
-
-
-
-
-
-
-
-
-
-
-
Melexis reserved
Melexis reserved
0x0400
1
2
…
…
31
32
0x041F
0x0420
33
34
…
…
63
64
0x043F
0x0440
65
66
…
…
95
96
0x045F
0x0460
…
0x06A0
0x047F
…
0x06BF
0x06C0 705 706
…
…
735 736 0x06DF
0x06E0 737 738
…
…
767 768 0x06FF
0x0700
0x0700=Ta_Vbe, 0x0708=CP(SP 0), 0x070A=GAIN
Melexis reserved
0x071F
0x0720
0x0720=Ta_PTAT, 0x0728=CP(SP1), 0x072A=VDDpix
Melexis reserved
0x073F
Subpage 0
Subpage 1
…
0x0400
0x041F
0x0420
0x043F
0x0440
0x045F
0x0460
…
0x06A0
0x047F
…
0x06BF
0x06C0
0x06DF
0x06E0
0x06FF
0x0700
0x071F
0x0720
0x073F
0x0720=Ta_PTAT, 0x0728=CP(SP1), 0x072A=VDDpix
Melexis reserved
Pixels 33...64 (subpage 1)
Pixels 65…96 (subpage 0)
…
Pixels 705…736 (subpage 0)
Pixels 737…768 (subpage 1)
0x0700=Ta_Vbe, 0x0708=CP(SP 0), 0x070A=GAIN
Melexis reserved
Pixels 1…32 (subpage 0)

MLX90640 32x24 IR array
Datasheet
Page 19 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
10.7.3. EEPROM
The EEPROM is used to store the calibration constants and the configuration parameters of the device
EEPROM address
Access
Meaning
0x2400
Melexis
Melexis reserved
0x2401
Melexis
Melexis reserved
0x2402
Melexis
Melexis reserved
0x2403
Melexis
Configuration register
0x2404
Melexis
Melexis reserved
0x2405
Melexis
Melexis reserved
0x2406
Melexis
Melexis reserved
0x2407
Melexis
Device ID1
0x2408
Melexis
Device ID2
0x2409
Melexis
Device ID3
0x240A
Melexis
Device Options
0x240B
Melexis
Melexis reserved
0x240C
Customer
Control register_1
0x240D
Customer
Control register_2
0x240E
Customer
I2CConfReg
0x240F
Customer
Melexis reserved / I2C_Address
Table 7 Configuration parameters memory
After POR the device read dedicated EEPROM cells and transfers their content to into the control and configuration register
of the device. This way the device is configured and prepared for operation. The relation between EEPROM and register
address is shown here after (explanation of the bit meaning can be found in section 10.7.1 Internal registers:
EEPROM address
Register address
Access
Name
Data [hex]
0x240C
0x800D
Customer
Control_register_1
1901
0x240D
0x800E
Customer
Control_register_2
0000
0x240E
0x800F
Customer
I2CConfReg
0000
0x240F
0x8010
Customer
Melexis internal use (8 bit)
I2C_Address (8bit)
BE33
Table 8 EEPROM to registers mapping

MLX90640 32x24 IR array
Datasheet
Page 20 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
Table 9 EEPROM overview (words)
Address
0
1
2
3
4
5
6
7
8
9
A
B
C
D
E
F
0x2400
Osc Trim
Ana Trim
MLX
Conf reg
MLX
MLX
MLX
ID 1
ID 2
ID 3
MLX
MLX
Cont reg 1 Cont reg 2
I2C conf
I2C add
0x2410
Scale OCC Pix os avg
0x2420
Scale ACC Pix α avg
0x2430
GAIN
PTAT_25
Kv, Kt ptat Kv Vdd_25
Kv_avg
MLX
Kv, Kta Sca ACP 1,2
Off - CP1,2 Kv, Kta Cp KsTa, TGC KsTo 4, 3
KsTo 2, 1
CT 4, 3
0x2440
0x2450
0x2460
0x2470
0x2480
0x2490
0x24A0
0x24B0
0x24C0
0x24D0
0x24E0
0x24F0
0x2500
0x2510
0x2520
0x2530
0x2540
0x2550
0x2560
0x2570
0x2580
0x2590
0x25A0
0x25B0
0x25C0
0x25D0
0x25E0
0x25F0
0x2600
0x2610
0x2620
0x2630
0x2640
0x2650
0x2660
0x2670
0x2680
0x2690
0x26A0
0x26B0
0x26C0
0x26D0
0x26E0
0x26F0
0x2700
0x2710
0x2720
0x2730
768 x Offset, α, Kta, Outlier
OCC_row_01…24 (6 x 4 x 3bit+sign)
OCC_column_01…32 (8 x 4 x 3bit+sign)
ACC_row_01…24 (6 x 4 x 3bit+sign)
ACC_column_01…32 (8 x 4 x 3bit+sign)
Kta_avg

MLX90640 32x24 IR array
Datasheet
Page 21 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
Table 10 Calibration parameters memory (EEPROM - bits)
NOTE 1: EEPROM addresses from 0x2440…0x273F contain the individual pixel calibration information and may not be
equal to 0x0000. In case any pixel data is equal to 0x0000 this means that this particular pixels has failed and the
calculation for To should not be trusted and avoided. Depending on the application, the To value for such pixels can be
replaced with a default value such as -273.15°C, can be equal to Ta or one calculate an average value from the adjacent
pixels.
NOTE 2: The LSB for EEPROM addresses from 0x2440…0x273F indicate if all pixel parameters are within the calibration
specification. If this bit is set i.e. = “1” this would mean that at least one of the calibration parameters for this
particular pixel is outside the calibration specifications and the pixel is considered as Outlier i.e. the sensor accuracy is
not guaranteed by the calibration.
NOTE 3: Pixels identified during calibration process as potentially long term deviating pixels are marked in the same
manner. Long term deviating pixels are identified in zone 1 and zone 2 only, zone 3 is excluded (for zone information
please refer to paragraph 12.1.1 Figure 18). An unidentified long term deviating pixel may be still present.
NOTE 4: The maximum number of deviating pixels is 4 (please check False pixel correction), none of the deviating pixels
are adjacent to each other. Depending on the application one may have to choose to replace the measurement results
of such pixel by an average of the temperature indicated by the adjacent pixels.
Address \ bit
15
14
13
12
11
10
9
8
7
6
5
4
3
2
1
0
0x2410
0x2411
0x2412
0x2413
0x2414
0x2415
0x2416
0x2417
0x2418
0x2419
0x241A
0x241B
0x241C
0x241D
0x241E
0x241F
0x2420
0x2421
0x2422
0x2423
0x2424
0x2425
0x2426
0x2427
0x2428
0x2429
0x242A
0x242B
0x242C
0x242D
0x242E
0x242F
0x2430
0x2431
0x2432
0x2433
0x2434
0x2435
0x2436
0x2437
0x2438
0x2439
0x243A
0x243B
0x243C
0x243D
0x243E
0x243F
0x2440
Outlier
0x2441
Outlier
…
…
0x245E
Outlier
0x245F
Outlier
0x2460
Outlier
0x2461
Outlier
…
…
0x273E
Outlier
0x273F
Outlier
…
…
…
± Offset pixel (24, 31)
α pixel (24, 31)
± Kta (24, 31)
± Offset pixel (24, 32)
α pixel (24, 32)
± Kta (24, 32)
± Offset pixel (1, 32)
α pixel (1, 32)
± Kta (1, 32)
± Offset pixel (2, 1)
α pixel (2, 1)
± Kta (2, 1)
± Offset pixel (2, 2)
α pixel (2, 2)
± Kta (2, 2)
± Offset pixel (1, 2)
α pixel (1, 2)
± Kta (1, 2)
…
…
…
± Offset pixel (1, 31)
α pixel (1, 31)
± Kta (1, 31)
± KsTo range 4 (CT2°C…)
± KsTo range 3 (CT1°C…CT2°C)
MLX
temp step x 10
CT4
CT3
KsTo Scale offset - 8
± Offset pixel (1, 1)
α pixel (1, 1)
± Kta (1, 1)
± Alpha (CP subpage_1 / CP subpage_0 - 1)*2^7
Alpha CP subpage_0
± Offset (CP subpage_1 - CP subpage_0)
± Offset CP subpage_0
± Kv_CP
± Kta_CP
± KsTa*2^13
TGC (±4)*2^7
± KsTo range 2 (0°C…CT1°C)
± KsTo range 1 (<0°C)
± Kta_avg_RowOdd-ColumnOdd
± Kta_avg_RowEven-ColumnOdd
± Kta_avg_RowOdd-ColumnEven
± Kta_avg_RowEven-ColumnEven
MLX
Res control calib
Kv_scale
Kta_scale_1
Kta_scale_2
±IL_CHESS_C3 - 5 bits
±IL_CHESS_C2 - 5 bits
±IL_CHESS_C1 - 6 bits
± PTAT_25
± Kv_PTAT
± Kt_PTAT
± Kv_Vdd
± Vdd_25
± Kv_avg_RowOdd-ColumnOdd
± Kv_avg_RowEven-ColumnOdd
± Kv_avg_RowOdd-ColumnEven
± Kv_avg_RowEven-ColumnEven
± ACC column 28
± ACC column 27
± ACC column 26
± ACC column 25
± ACC column 32
± ACC column 31
± ACC column 30
± ACC column 29
± GAIN
± ACC column 16
± ACC column 15
± ACC column 14
± ACC column 13
± ACC column 20
± ACC column 19
± ACC column 18
± ACC column 17
± ACC column 24
± ACC column 23
± ACC column 22
± ACC column 21
± ACC column 4
± ACC column 3
± ACC column 2
± ACC column 1
± ACC column 8
± ACC column 7
± ACC column 6
± ACC column 5
± ACC column 12
± ACC column 11
± ACC column 10
± ACC column 9
± ACC row 16
± ACC row 15
± ACC row 14
± ACC row 13
± ACC row 20
± ACC row 19
± ACC row 18
± ACC row 17
± ACC row 24
± ACC row 23
± ACC row 22
± ACC row 21
± ACC row 4
± ACC row 3
± ACC row 2
± ACC row 1
± ACC row 8
± ACC row 7
± ACC row 6
± ACC row 5
± ACC row 12
± ACC row 11
± ACC row 10
± ACC row 9
± OCC column 32
± OCC column 31
± OCC column 30
± OCC column 29
Alpha scale - 30
Scale_ACC_row
Scale_ACC_column
Scale_ACC_remnand
Pix_sensitivity_average
± OCC column 20
± OCC column 19
± OCC column 18
± OCC column 17
± OCC column 24
± OCC column 23
± OCC column 22
± OCC column 21
± OCC column 28
± OCC column 27
± OCC column 26
± OCC column 25
± OCC column 8
± OCC column 7
± OCC column 6
± OCC column 5
± OCC column 12
± OCC column 11
± OCC column 10
± OCC column 9
± OCC column 16
± OCC column 15
± OCC column 14
± OCC column 13
± OCC row 20
± OCC row 19
± OCC row 18
± OCC row 17
± OCC row 24
± OCC row 23
± OCC row 22
± OCC row 21
± OCC column 4
± OCC column 3
± OCC column 2
± OCC column 1
± OCC row 8
± OCC row 7
± OCC row 6
± OCC row 5
± OCC row 12
± OCC row 11
± OCC row 10
± OCC row 9
± OCC row 16
± OCC row 15
± OCC row 14
± OCC row 13
(Alpha PTAT - 8)*4
scale_Occ_row
scale_Occ_col
scale_Occ_rem
± Pix_os_average
± OCC row 4
± OCC row 3
± OCC row 2
± OCC row 1

MLX90640 32x24 IR array
Datasheet
Page 22 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
11. Calculating Object Temperature
11.1. Restoring calibration data from EERPOM
NOTE: All data in the EEPROM is coded as two’s complement (unless otherwise noted)
In the example we are restoring the calibration data for pixel (12, 16)
11.1.1. Restoring the VDD sensor parameters
Following formula is used to calculate the VDD of the sensor:
[ ]
If 
[ ]
11.1.2. Restoring the Ta sensor parameters
Following formula is used to calculate the Ta of the sensor:
(
)
, °C
Where:
[ ]
If 
[ ]
If 
[ ]
[ ]

MLX90640 32x24 IR array
Datasheet
Page 23 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
If 
(
)
Where:
[ ]
If 
[ ]
If 
[ ]
11.1.3. Restoring the offset
[ ]
If 
[ ]
(i.e. the four most significant bits, signed)
If 
[ ]
(unsigned)
[ ]
(i.e. the four most significant bits, signed)
If 
[ ]
(unsigned)
[ ]
(i.e. the six most significant bits, signed)
If 
[ ] (unsigned)
11.1.3.1. Restoring the offset in case of Interleaved reading pattern
To compensate the IR data for interleaved reading pattern following formula is used:
( ) ( ( ))

MLX90640 32x24 IR array
Datasheet
Page 24 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
Highlighted in yellow parameters are extracted hereafter.
As a default the device is factory calibrated in Chess pattern mode thus the best performance will be when a Chess
pattern is used. However some customers may choose to use the device in interleaved mode which will degrade th e
device performance. In this case a correction can be applied to restore to some extend the performance. Once the IR
data is compensated the calculation for To is done using default flow. The goal of this correction is to equalize the
offset of the pixels due to the different pattern reading modes. We can achieve this by using several correction
coefficients stored into the device EEPROM extracted and decoded as follows:
[ ]
If 
[ ]
If 
[ ]
If 
The above calculated parameters have to be applied as a correction for the offset of each individual pixel. We do need
additional patterns in order to make these calculations and the formula to calculate those patterns are as shown below
depending on the pixels number:
(
) (
(
)
)
( (
) (
) (
) (
))
11.1.4. Restoring the Sensitivity
Where (calculating for pixel (12,16)) :
[ ]
[ ]

MLX90640 32x24 IR array
Datasheet
Page 25 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
[ ]
(i.e. the four most significant bits, signed)
If 
[ ]
(unsigned)
[ ]
(i.e. the four most significant bits, signed )
If 
[ ]
(unsigned)
[ ]
If 
[ ] (unsigned)
11.1.5. Restoring the Kv(i,j) coefficient
depend on the pixel position in the array i.e. if the pixel row and column is odd or even
If row number is ODD (1, 3, 5…23) and column number is ODD (1, 3, 5…31) then
[ ]
If row number is EVEN (2, 4, 6…24) and column number is ODD (1, 3, 5…31) then
[ ]
If row number is ODD (1, 3, 5…23) and column number is EVEN (2, 4, 6…32) then
[ ]
If row number is EVEN (2, 4, 6…24) and column number is EVEN (2, 4, 6…32) then [ ]
If 
(signed)
Where:
[ ]
(unsigned)
11.1.6. Restoring the Kta(i,j) coefficient
Where:
[ ]
(signed)
If 

MLX90640 32x24 IR array
Datasheet
Page 26 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
depends on the pixel position in the array i.e. if the pixel row and column is odd or even
If row number is ODD (1, 3, 5…23) and column number is ODD (1, 3, 5…31) then
[ ]
If row number is EVEN (2, 4, 6…24) and column number is ODD (1, 3, 5…31) then [ ]
If row number is ODD (1, 3, 5…23) and column number is EVEN (2, 4, 6…32) then
[ ]
If row number is EVEN (2, 4, 6…24) and column number is EVEN (2, 4, 6…32) then [ ]
If 
[ ]
(unsigned)
[ ] (unsigned)
11.1.7. Restoring the GAIN coefficient (common for all pixels)
[ ] (signed)
If 
11.1.8. Restoring the KsTa coefficient (common for all pixels)
Where:
[ ]
(signed)
If 
11.1.9. Restoring corner temperatures (common for all pixel)
The information regarding corner temperatures is stored into device EEPROM and is restored as follows:
[ ]
[ ]
[ ]
Or we can construct the temperatures for the ranges as follows:
CT1=-40°C (hard codded) < Range 1 > CT2=0°C (hard codded) < Range 2 > CT3 < Range 3 > CT4 < Range 4

MLX90640 32x24 IR array
Datasheet
Page 27 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
11.1.10. Restoring the KsTo coefficient (common for all pixels)
Where:
[ ] (unsigned)
Where:
[ ] (signed)
If 
Where:
[ ]
(signed)
If 
Where:
[ ] (signed)
If 
Where:
[ ]
(signed)
If 
11.1.11. Restoring sensitivity correction coefficients for each temperature range
( ( ))
( ) ( )

MLX90640 32x24 IR array
Datasheet
Page 28 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
11.1.12. Restoring the Sensitivity
Please note that there are two sensitivities for the compensation pixel – one for each subpage
[ ]
(
)
Where:
[ ]
[ ]
(signed)
If 
11.1.13. Restoring the offset of the Compensation Pixel (CP)
Please note that there are two offsets for the compensation pixel – one for each subpage
[ ] (signed)
If 
Where:
[ ]
(signed)
If 
11.1.14. Restoring the Kv CP coefficient
[ ]
(unsigned) (the same one as for the coefficients)
Where:
[ ]
(signed)
If 
11.1.15. Restoring the Kta CP coefficient

MLX90640 32x24 IR array
Datasheet
Page 29 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
[ ]
(unsigned) (the same one as for the coefficients)
Where:
[ ] (signed)
If 
11.1.16. Restoring the TGC coefficient
Where:
[ ] (signed)
If 
NOTE 1: In a MLX90640ESF–BAx–000-TU device, the TGC coefficient is set to 0 and must not be changed.
NOTE 2: In a MLX90640ESF–BCx–000-TU device, the EEPROM contains a typical value for the TGC coefficient but the
user may choose to adjust the value such to best fit for a specific application. Using the TGC increases noise in the
temperature calculations which can be reduced by external filtering (averaging) of the CP sensor data. By making the
TGC coefficient “0” the gradients compensation is bypassed.
11.1.17. Restoring the resolution control coefficient
[ ]
(unsigned)

MLX90640 32x24 IR array
Datasheet
Page 30 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
11.2. Temperature Calculation
11.2.1. Example Input Data
11.2.1.1. Example Measurement Data
Input data name
Input data value
Object temperature
80°C
Emissivity (ε)
1
Control register 1 (Resctrl)
0x0901 (2 decimal)
RAM[0x056F] (pix(12,16) data)
0x0261 (609)
Vbe - RAM[0x0700]
0x4BF2 (19442)
CP subpage 0 – RAM[0x0708]
0xFFCA (-54)
CP subpage 1 – RAM[0x0728]
0xFFC8 (-56)
GAIN - RAM[0x070A]
0x1881 (6273)
PTAT - RAM[0x0720]
0x06AF (1711)
VDD - RAM[0x072A]
0xCCC5 (-13115)
Table 11 Calculation example input data
11.2.1.2. Example Calibration Data
EEPROM
address
Calibration parameter name
Parameter
value
Decoded value
0x2410
K_PTAT – 4 bits
Scale_OCC_row – 4 bits
Scale_OCC_column – 4 bits
Scale_OCC_remnand – 4 bits
0x4210
K_PTAT = 9
Scale_OCC_row = 2
Scale_OCC_column = 1
Scale_OCC_remnand = 0
0x2411
Pix_os_average – 16 bits
0xFFBB
Pix_os_average = -69
0x2412
OCC_rows_04 – 4 bits
OCC_rows_03 – 4 bits
OCC_rows_02 – 4 bits
OCC_rows_01 – 4 bits
0x0202
OCC_rows_04 = 0
OCC_rows_03 = 2
OCC_rows_02 =0
OCC_rows_01 = 2
0x2413
OCC_rows_08 – 4 bits
OCC_rows_07 – 4 bits
OCC_rows_06 – 4 bits
OCC_rows_05 – 4 bits
0xF202
OCC_rows_08 = -1
OCC_rows_07 = 2
OCC_rows_06 = 0
OCC_rows_05 = 2
0x2414
OCC_rows_12 – 4 bits
OCC_rows_11 – 4 bits
0xF2F2
OCC_rows_12 = -1
OCC_rows_11 = 2

MLX90640 32x24 IR array
Datasheet
Page 31 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
OCC_rows_10 – 4 bits
OCC_rows_09 – 4 bits
OCC_rows_10 = -1
OCC_rows_09 = 2
0x2415
OCC_rows_16 – 4 bits
OCC_rows_15 – 4 bits
OCC_rows_14 – 4 bits
OCC_rows_13 – 4 bits
0xE2E2
OCC_rows_16 = -2
OCC_rows_15 = 2
OCC_rows_14 = -2
OCC_rows_13 = 2
0x2416
OCC_rows_20 – 4 bits
OCC_rows_19 – 4 bits
OCC_rows_18 – 4 bits
OCC_rows_17 – 4 bits
0xD1E1
OCC_rows_20 = -3
OCC_rows_19 = 1
OCC_rows_18 = -2
OCC_rows_17 = 1
0x2417
OCC_rows_24 – 4 bits
OCC_rows_23 – 4 bits
OCC_rows_22 – 4 bits
OCC_rows_21 – 4 bits
0xB1D1
OCC_rows_24 = -5
OCC_rows_23 = 1
OCC_rows_22 = -3
OCC_rows_21 = 1
0x2418
OCC_column_04 – 4 bits
OCC_column_03 – 4 bits
OCC_column_02 – 4 bits
OCC_column_01 – 4 bits
0xF10F
OCC_column_04 = -1
OCC_column_03 = 1
OCC_column_02 = 0
OCC_column_01 = -1
0x2419
OCC_column_08 – 4 bits
OCC_column_07 – 4 bits
OCC_column_06 – 4 bits
OCC_column_05 – 4 bits
0xF00F
OCC_column_08 = -1
OCC_column_07 = 0
OCC_column_06 = 0
OCC_column_05 = -1
0x241A
OCC_column_12 – 4 bits
OCC_column_11 – 4 bits
OCC_column_10 – 4 bits
OCC_column_09 – 4 bits
0xE0EF
OCC_column_12 = -2
OCC_column_11 = 0
OCC_column_10 = -2
OCC_column_09 = -1
0x241B
OCC_column_16 – 4 bits
OCC_column_15 – 4 bits
OCC_column_14 – 4 bits
OCC_column_13 – 4 bits
0xE0EF
OCC_column_16 = -2
OCC_column_15 = 0
OCC_column_14 = -2
OCC_column_13 = -1
0x241C
OCC_column_20 – 4 bits
OCC_column_19 – 4 bits
OCC_column_18 – 4 bits
OCC_column_17 – 4 bits
0xE1E1
OCC_column_20 = -2
OCC_column_19 = 1
OCC_column_18= -2
OCC_column_17 = 1
0x241D
OCC_column_24 – 4 bits
OCC_column_23 – 4 bits
OCC_column_22 – 4 bits
OCC_column_21 – 4 bits
0xF3F2
OCC_column_24 = -1
OCC_column_23 = 3
OCC_column_22= -1
OCC_column_21 = 2
0x241E
OCC_column_28 – 4 bits
OCC_column_27 – 4 bits
OCC_column_26 – 4 bits
OCC_column_25 – 4 bits
0xF404
OCC_column_28 = -1
OCC_column_27 = 4
OCC_column_26= 0
OCC_column_25 = 4
0x241F
OCC_column_32 – 4 bits
OCC_column_31 – 4 bits
0xE504
OCC_column_32 = -2
OCC_column_31 = 5

MLX90640 32x24 IR array
Datasheet
Page 32 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
OCC_column_30 – 4 bits
OCC_column_29 – 4 bits
OCC_column_30= 0
OCC_column_29 = 4
0x2420
Alpha scale – 4 bits
Scale_ACC_row – 4 bits
Scale_ACC_column – 4 bits
Scale_ACC_remnand – 4 bits
0x79A6
Alpha scale = 37
Scale_ACC_row = 9
Scale_ACC_column = 10
Scale_ACC_remnand = 6
0x2421
Pix_sensitivity_average - 16 bits
0x2F44
Pix_sensitivity_average = 8.80391E-08
0x2422
ACC_rows_04 – 4 bits
ACC_rows_03 – 4 bits
ACC_rows_02 – 4 bits
ACC_rows_01 – 4 bits
0xFFDD
ACC_rows_04 = -1
ACC_rows_03 = -1
ACC_rows_02 = -3
ACC_rows_01 = -3
0x2423
ACC_rows_08 – 4 bits
ACC_rows_07 – 4 bits
ACC_rows_06 – 4 bits
ACC_rows_05 – 4 bits
0x2210
ACC_rows_08 = 2
ACC_rows_07 = 2
ACC_rows_06 = 1
ACC_rows_05 = 0
0x2424
ACC_rows_12 – 4 bits
ACC_rows_11 – 4 bits
ACC_rows_10 – 4 bits
ACC_rows_09 – 4 bits
0x3333
ACC_rows_12 = 3
ACC_rows_11 = 3
ACC_rows_10 = 3
ACC_rows_09 = 3
0x2425
ACC_rows_16 – 4 bits
ACC_rows_15 – 4 bits
ACC_rows_14 – 4 bits
ACC_rows_13 – 4 bits
0x2233
ACC_rows_16 = 2
ACC_rows_15 = 2
ACC_rows_14 = 3
ACC_rows_13 = 3
0x2426
ACC_rows_20 – 4 bits
ACC_rows_19 – 4 bits
ACC_rows_18 – 4 bits
ACC_rows_17 – 4 bits
0xEF01
ACC_rows_20 = -2
ACC_rows_19 = -1
ACC_rows_18 = 0
ACC_rows_17 = 1
0x2427
ACC_rows_24 – 4 bits
ACC_rows_23 – 4 bits
ACC_rows_22 – 4 bits
ACC_rows_21 – 4 bits
0x9ACC
ACC_rows_24 = -7
ACC_rows_23 = -6
ACC_rows_22 = -4
ACC_rows_21 = -4
0x2428
ACC_column_04 – 4 bits
ACC_column_03 – 4 bits
ACC_column_02 – 4 bits
ACC_column_01 – 4 bits
0xEEDC
ACC_column_04 = -1
ACC_column_03 = -1
ACC_column_02 = -2
ACC_column_01 = -3
0x2429
ACC_column_08 – 4 bits
ACC_column_07 – 4 bits
ACC_column_06 – 4 bits
ACC_column_05 – 4 bits
0x10FF
ACC_column_08 = 1
ACC_column_07 = 0
ACC_column_06 = -1
ACC_column_05 = -1
0x242A
ACC_column_12 – 4 bits
ACC_column_11 – 4 bits
ACC_column_10 – 4 bits
ACC_column_09 – 4 bits
0x2221
ACC_column_12 = 2
ACC_column_11 = 2
ACC_column_10 = 2
ACC_column_09 = 1

MLX90640 32x24 IR array
Datasheet
Page 33 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
0x242B
ACC_column_16 – 4 bits
ACC_column_15 – 4 bits
ACC_column_14 – 4 bits
ACC_column_13 – 4 bits
0x3333
ACC_column_16 = 3
ACC_column_15 = 3
ACC_column_14 = 3
ACC_column_13 = 3
0x242C
ACC_column_20 – 4 bits
ACC_column_19 – 4 bits
ACC_column_18 – 4 bits
ACC_column_17 – 4 bits
0x2333
ACC_column_20 = 2
ACC_column_19 = 3
ACC_column_18= 3
ACC_column_17 = 3
0x242D
ACC_column_24 – 4 bits
ACC_column_23 – 4 bits
ACC_column_22 – 4 bits
ACC_column_21 – 4 bits
0x0112
ACC_column_24 = 0
ACC_column_23 = 1
ACC_column_22= 1
ACC_column_21 = 2
0x242E
ACC_column_28 – 4 bits
ACC_column_27 – 4 bits
ACC_column_26 – 4 bits
ACC_column_25 – 4 bits
0xEEFF
ACC_column_28 = -2
ACC_column_27 = -2
ACC_column_26= -1
ACC_column_25 = -1
0x242F
ACC_column_32 – 4 bits
ACC_column_31 – 4 bits
ACC_column_30 – 4 bits
ACC_column_29 – 4 bits
0xBBDD
ACC_column_32 = -5
ACC_column_31 = -5
ACC_column_30= -3
ACC_column_29 = -3
0x2430
GAIN
0x18EF
GAIN = 6383
0x2431
PTAT_25
0x2FF1
PTAT_25 = 12273
0x2432
Kv_PTAT - 6 bits
Kt_PTAT - 10 bits
0x5952
Kv_PTAT = 0.005371094
Kt_PTAT = 42.25
0x2433
K_Vdd - 8 bits
Vdd_25 - 8 bits
0x9D68
K_Vdd = -3168
Vdd_25 = -13056
0x2434
Kv_avg_RO_CO – 4 bits
Kv_avg_RE_CO – 4 bits
Kv_avg_RO_CE – 4 bits
Kv_avg_RE_CE – 4 bits
0x5454
Kv_avg_RO_CO = 5
Kv_avg_RE_CO = 4
Kv_avg_RO_CE = 5
Kv_avg_RE_CE = 4
0x2435
IL_CHESS_C3 – 5 bits
IL_CHESS_C2 – 5 bits
IL_CHESS_C1 – 6 bits
0x0994
IL_CHESS_C3 = 0.125
IL_CHESS_C2 = 3
IL_CHESS_C1 = 1.25
0x2436
Kta_avg_RO_CO – 8 bits
Kta_avg_RE_CO – 8 bits
0x6956
Kta_avg_RO_CO = 105
Kta_avg_RE_CO = 86
0x2437
Kta_avg_RO_CE – 8 bits
Kta_avg_RE_CE – 8 bits
0x5354
Kta_avg_RO_CE = 83
Kta_avg_RE_CE = 84
0x2438
Resolution_control_cal – 2 bits
Kv_scale – 4 bits
Kta_scale_1 – 4 bits
Kta_scale_1 – 4 bits
0x2363
Resolution_control_cal = 2
Kv_scale = 3
Kta_scale_1 = 14
Kta_scale_1 = 3
0x2439
CP_SP_1/SP_0_ratio – 6 bits
0xE446
CP_SP_1/SP_0_ratio = -0.0546875

MLX90640 32x24 IR array
Datasheet
Page 34 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
Alpha_CP_SP_0 – 10 bits
Alpha_CP_SP_0 = 4.0745362639427E-09
0x243A
CP_off_delta (SP_1 - SP_0) – 6 bits
Offset_CP_SP_0 – 10 bits
0xFBB5
CP_off_delta (SP_1 - SP_0) = -2
Offset_CP_SP_0 = -75
0x243B
Kv_CP – 8 bits
Kta_CP – 8 bits
0x044B
Kv_CP = 0.5
Kta_CP = 0.00457763671875
0x243C
KsTa – 8 bits
TGC – 8 bits
0xF020
KsTa = -0.001953125
TGC = 1
0x243D
KsTo2 (0°C…CT3°C) – 8 bits
KsTo1 (<0°C) – 8 bits
0x9797
KsTo2 (0°C…CT3°C) = -0.0008010864
KsTo1 (<0°C) = -0.0008010864
0x243E
KsTo4 (CT4°C…) – 8 bits
KsTo3 (CT3°C…CT4°C) – 8 bits
0x9797
KsTo4 (CT4°C…) = -0.0008010864
KsTo3 (CT3°C…CT4°C) = -0.0008010864
0x243F
Step – 2 bits
CT4 – 4 bits
CT3 – 4 bits
KsTo_scale – 4 bits
0x2889
Step = 20°C
CT4 = 320°C
CT3 = 160°C
KsTo_scale = 17
Table 12 Calculation example calibration data

MLX90640 32x24 IR array
Datasheet
Page 35 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
11.2.2. Temperature calculation
After the parameters restore the temperature calculation is done using following calculation flow (assuming that the
EEPROM data are already extracted):
Figure 16 To calculation flow
For this example we calculate the temperature of pixel (12, 16) i.e. row=12 and the column=16.
Values marked with green are extracted from device EEPROM
Values marked with grey are final parameter values or are values to be used for next calculations
11.2.2.1. Resolution restore
The device is calibrated with default resolution setting = 2 (corresponding to ADC resolution set to 18bit see Fig 11) i.e.
if the one choose to change the ADC resolution setting to a different one a correction of the data must be done. First
we must restore the resolution at which the device has been calibrated which is stored at EERPOM 0x2438.
Where:
[ ]
(unsigned)
[ ]
(unsigned)
Supply voltage value calculation (common for all pixels) - 11.2.2.2
Ambient temperature calculation (common for all pixels) - 11.2.2.3
Gain compensation - 11.2.2.5.1
IR data compensation – offset, VDD and Ta - 11.2.2.5.3
IR data Emissivity compensation - 11.2.2.5.4
IR data gradient compensation - 11.2.2.7
Normalizing to sensitivity - 11.2.2.8
Calculating To - 11.2.2.9
Image (data) processing

MLX90640 32x24 IR array
Datasheet
Page 36 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
In case the ADC resolution is changed the one must multiply the coefficient with the RAM data for VDD
only. Please note that the data for Vbe, PTAT and IR pixels (including CP) must not be changed.
11.2.2.2. Supply voltage value calculation (common for all pixels)
[ ]
Where: Constants calculation of the EEPROM stored values (can be done just once after POR)
[ ]
If 
[ ]
VDD calculations:
[ ]
If  [ ] LSB
11.2.2.3. Ambient temperature calculation (common for all pixels)
(
)
, °C
Where:
[ ]
If 
[ ]
If 
[ ]

MLX90640 32x24 IR array
Datasheet
Page 37 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
[ ]
If  [ ] LSB
[ ]
If 
(
)
Where:
[ ] = 0x06AF = 1711
If 
[ ]
If 
[ ]
(
) (
)
(
)
,°C
(
)
°C
11.2.2.4. Gain parameter calculation (common for all pixels)
[ ]
[ ]
If  [ ]
[ ]
If 

MLX90640 32x24 IR array
Datasheet
Page 38 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
Please note that this value is updated every frame and it is the same for all pixels including CP regardless the subpage
number
11.2.2.5. Pixel data calculations
The pixel addressing is following the pattern as described in Reading pattern shown in Fig 5:
11.2.2.5.1. Gain compensation
The first step of the data processing on raw IR data is always the gain compensation, regardless of pixel or subpage
number.
[ ] [ ]
[ ]
If  [ ]
11.2.2.5.2. Offset calculation
[ ]
If 
As the row=12, we select EEPROM cell 0x2414 (± OCC_rows_12…08 (4 x 4bit)) and extract the four most significant bits
corresponding to parameter OCC_rows_12. If another row number is selected, the corresponding OCC parameter must
be selected.
[ ]
If 
[ ]
Please note that is a common parameter for all calculation
As the column=16, we select EEPROM cell 0x2425 (± OCC_column_16…13 (4 x 4bit)) and extract the four most
significant bits corresponding to parameter OCC_columns_16. If another column number is selected, the
corresponding OCC parameter must be selected.
[ ]
If 
[ ]
Please note that is a common parameter for all calculation

MLX90640 32x24 IR array
Datasheet
Page 39 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
[ ]
If 
[ ]
11.2.2.5.3. IR data compensation – offset, VDD and Ta
( ) ( ( ))
Where:
[ ]
If 
As row and column numbers are even then
[ ]
If 
[ ]
[ ]
As row and column numbers are even:
[ ]
If 
(signed)
Where:
[ ]
( ) ( )

MLX90640 32x24 IR array
Datasheet
Page 40 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
11.2.2.5.4. IR data Emissivity compensation
Emissivity compensation: For the example we assume Emissivity = 1. Note that the Emissivity coefficient is user
defined and it is not stored in the device EEPROM)
11.2.2.6. CP data calculations
11.2.2.6.1. Compensating the GAIN of CP pixel
[ ]
[ ]
If  [ ]
[ ]
[ ]
If  [ ]
NOTE: In order to limit the noise in the final To calculation it is advisable to filter the CP readings at this point of
calculation. A good practice would be to apply a Moving Average Filter with length of 16 or higher.
11.2.2.6.2. Compensating offset, Ta and VDD of CP pixel
( ) ( ( ))
The value of the offset for compensating pixel for the subpage 1 depends on the reading pattern. In case the chess
reading pattern mode is used following formula is to be applied:
( ) ( ( ))
In case of interleaved mode is used following formula is to be applied:
( ) ( ( ))
The correction parameter (highlighted in yellow) is extracted in 11.1.3.1
Where:
[ ]
If 

MLX90640 32x24 IR array
Datasheet
Page 41 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
Where:
[ ]
If 
Where:
[ ]
(unsigned) (the same one as for the coefficients)
[ ]
If 
[ ]
(unsigned) (the same one as for the coefficients)
Where:
[ ]
If 
( ) ( )
( ) ( )
11.2.2.7. IR data gradient compensation
As stated in “Reading patterns” the device can work in two different readings modes (Chess pattern – the default one
and IL (Interleave mode)).
Depending on the device measurement mode and we can define a pattern which will help us
to automatically switch between both subpages.
-
In case of Chess pattern is selected please use following expression:
( (
) (
(
)
) ) ( (
) )
-
In case of Interleaved pattern please use following expression:

MLX90640 32x24 IR array
Datasheet
Page 42 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
( (
) (
(
)
) )
Where the function is giving the truncated whole number without fractional component of the result.
Where is exclusive or or exclusive disjunction is a logical operation that outputs true only when inputs differ. The truth
table is as follows:
Input 1
Input 2
Output
0
0
0
0
1
1
1
0
1
1
1
0
Table 13 XOR truth table
Example: Let’s assume that the
If we are in chess mode:
( (
) (
(
)
) ) ( (
) )
( (
) )
( (
) )
If we are in IL mode:
( (
) (
(
)
) ) ( (
) )
( (
) )
( )
Where:
[ ]
If 

MLX90640 32x24 IR array
Datasheet
Page 43 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
11.2.2.8. Normalizing to sensitivity
( ( )) ( )
[ ]
(
) (
)
Where:
[ ]
[ ]
If 
Where:
[ ]
(common for all pixels)
If 
Where:
[ ]
[ ]
[ ]
If 
[ ]
[ ]
If 
[ ]
[ ]
If 

MLX90640 32x24 IR array
Datasheet
Page 44 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
[ ]
( ( )) ( )
( ( )) ( )
11.2.2.9. Calculating To for basic temperature range (0°C…CT3 °C)
Where:
[ ]
If 
[ ]
As the IR signal received by the sensor has two components:
1. IR signal emitted by the object
2. IR signal reflected from the object (the source of this signal is surrounding environment of the sensor)
In order to compensate correctly for the emissivity and achieve best accuracy we need to know the surrounding
temperature which is responsible for the second component of the IR signal namely the reflected part - . In case this
temperature is not available and cannot be provided it might be replaced by .
Let’s assume °C.
√
√
√
√
, °C

MLX90640 32x24 IR array
Datasheet
Page 45 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
11.2.2.9.1. Calculations for extended temperature ranges
In order to extent the object temperature range and get the best possible accuracy an additional calculation cycle is needed.
We can identify 4 object temperature ranges (each temperature range has its own so called Corner Temperature – CT which
is the temperature at which the range starts):
-
Object temperature range 1 = -40°C … 0°C (Corner temperature for this range is -40°C and cannot be changed)
-
Object temperature range 2 = 0°C … CT3°C (Corner temperature for this range is 0°C and cannot be changed)
-
Object temperature range 3 = CT3°C … CT4°C
-
Object temperature range 4 = CT4°C …
In order to be able to carry out temperature calculation for the ranges outside of temperature range 2 (To = 0°C…CT3)
an additional parameters are needed and must be extracted from the device EEPROM. Those parameters are:
-
So called corner temperature (CTx) i.e. the value of temperature at the beginning of the range. Please
note that the corner temperatures for range 1 is fixed to -40°C and corner temperatures for range 2 is
fixed to 0°C while CT3 and CT4 are adjustable
-
Sensitivity slope for each range – KsTox
-
calculated in 11.2.2.9
11.2.2.9.1.1. Restoring corner temperatures
The information regarding corner temperatures is stored into device EEPROM and is restored as follows:
[ ]
[ ]
[ ]
Or we can construct the temperatures for the ranges as follows:
CT1=-40°C < Range 1 > CT2=0°C < Range 2 > CT3=160°C < Range 3 > CT4=320°C < Range 4
11.2.2.9.1.2. Restoring the sensitivity slope for each range
has been extracted in 11.1.10
Where:
[ ] (signed)
If 
Where:
[ ] (signed)

MLX90640 32x24 IR array
Datasheet
Page 46 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
If 
Where:
[ ]
(signed)
If 
Now we can calculate sensitivity correction coefficients for each temperature range:
( ( ))
( ( ))
( ) ( )
( ) ( )
11.2.2.9.1.3. Extended To range calculation
The input parameter for this calculation is the object temperature calculated in 11.2.2.9
If < 0°C we are in range 1 and we will use the parameters ( , and )
If 0°C < < CT3°C we are in range 2 and we will use the parameters ( , and )
If CT3°C < < CT4°C we are in range 3 and we will use the parameters ( , and )
If CT4°C < we are in range 4 and we will use the parameters ( , and )
√
( )

MLX90640 32x24 IR array
Datasheet
Page 47 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
12. Performance graphs
12.1. Accuracy
12.1.1. Pixel accuracy
All accuracy specifications apply under settled isothermal conditions only.
Furthermore, the accuracy is only valid if the object fills the FOV of the sensor completely.
Parameter definitions:
Frame accuracy is defined as average value of the all (768) pixels in the frame or for frame can be expressed as:
̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅
∑
̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅
Non-uniformity is defined as the maximum deviation of each individual pixel reading vs. the absolute accuracy.
(|
̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅|)
Pixel absolute accuracy is defined as:
Figure 17 Absolute temperature accuracy – MLX90640BAA (left) and MLX90640BAB (right)
Example: If we assume that the sensor (BAA type, zone 1) is measuring a target at 80°C that would mean that there should be
no pixel with error bigger than:
NOTES:
1) For best performance it is recommended to keep the supply voltage as accurate and stable as possible to 3.3V ±
0.05V
2) As a result of long term (years) drift there can be an additional measurement deviation of ± 3°C for object
temperatures around room temperature.
TBD
Frame Accuracy ± 1°C
Uniformity zone1 ± 1°C
Uniformity zone2 ± 2°C
Contact MLX
Contact MLX
Frame accuracy ± 1°C
Non-uniformity zone1 ± 0.5°C
Non-uniformity zone2 ± 1°C
Non-uniformity zone3 ± 2°C ± 2%*|To-Ta|
Frame Accuracy ± 2°C
NU zone1±1°C± 2%*|To-Ta|
NU zone2±2°C± 2%*|To-Ta|
NU zone3±3°C± 2%*|To-Ta|
Contact MLX
Frame accuracy ± 2°C
NU zone1±1°C± 2%*|To-Ta|
NU zone2±2°C± 2%*|To-Ta|
NU zone3±3°C± 2%*|To-Ta|
TBD
TBD
TBD
Frame Accuracy ± 5°C
Non-uniformity ± 2%*|To-Ta|
To, °C
-40°C
0°C
50°C
-40°C
0°C
100°C
Ta, °C
200°C
300°C
85°C
400°C
TBD
TBD
TBD
TBD
TBD
TBD
TBD
To, °C
-40°C
0°C
50°C
-40°C
0°C
100°C
200°C
300°C
85°C
400°C
Ta, °C

MLX90640 32x24 IR array
Datasheet
Page 48 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
Figure 18 Different accuracy zones depending on device type (BAA on the left and BAB on the right)
12.1.2. Ta accuracy
Absolute accuracy for the Ta channel (die temperature):
NOTE: Actual sensor surrounding temperature would be approximately 8°C lower
Zone 1
Zone 3
Zone 3
Zone 3
Zone 3
Zone 2
Zone 1
Zone 2
MLX90640BAA
MLX90640BAB

MLX90640 32x24 IR array
Datasheet
Page 49 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
12.2. Startup time
12.2.1. First valid data
After POR the first valid data is available after (depending on the selected refresh rate) which is calculated as:
, ms
(Example refresh rate is 2Hz – the default value)
It is always subpage 0 to be measured first after POR then subpage 1 and so on alternating.
NOTE: In case one changes the refresh rate on the fly (by writing new values into device register (0x800D)) the settings
will take place only after the subpage under measurement is finished.
12.2.2. Thermal behavior
Although electrically the device is set and running there is thermal stabilization time necessary before the device can
reach the specified accuracy – up to 4 min.
Vdd
40ms
2Hz
Subpage 0
Subpage 1
Set 8Hz
Subpage 0
Default
Active 2Hz refresh rate
8Hz refresh rate start

MLX90640 32x24 IR array
Datasheet
Page 50 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
12.3. Noise performance and resolution
There are two bits in the configuration register that allow changing the resolution of the MLX90640 measurements.
Increasing the resolution decreases the quantization noise and improves the overall noise performance.
Measurement conditions for the noise are: To=Ta=25°C
NOTE: Due to the nature of the thermal infrared radiation, it is normal that the noise will decrease for high temperature and
increase for lower temperatures
Figure 19 MLX90640BAx noise vs refresh rate for different device types
Not all pixels have the same noise performance. Because of the optical performance of the integrated lens, it is normal
that the pixels in the corner of the frame are noisier in comparison with the sensors in the middle. The graphs bellow
show the distribution of the noise density versus the pixel position in the frame (pixel number)
Figure 20 MLX90640BAA noise vs pixel and refresh rate at 1Hz and 2Hz
Figure 21 MLX90640BAA noise vs pixel and refresh rate at 4Hz, 8Hz and 16Hz

MLX90640 32x24 IR array
Datasheet
Page 51 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
Figure 22 MLX90640BAB noise vs pixel and refresh rate at 1Hz and 2Hz
Figure 23 MLX90640BAB noise vs pixel and refresh rate at 4Hz, 8Hz and 16Hz
NETD (K)
1Hz RMS noise (temperature equivalent), all pixels
MLX90640
Average
Min
Standard deviation
BAA
0.14
0.1
0.05
BAB
0.25
0.2
0.05
Table 14 Noise performance

MLX90640 32x24 IR array
Datasheet
Page 52 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
12.4. Field of view (FOV)
Figure 24 Field Of View measurement
The specified FOV is calculated for the wider direction, in this case for the 32 pixels.
FOV
X direction
Y direction
Central pointing from normal
(X & Y direction)
Typ
Typ
Max
MLX90640-ESF-BAA
110°
75°
5°
MLX90640-ESF-BAB
55°
35°
3°
Table 15 Available FOV options
Point heat source
Rotated sensor
Angle of incidence
100%
50%
Sensitivity
Field Of View

MLX90640 32x24 IR array
Datasheet
Page 53 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
13. Application information
13.1. Optical considerations
As this is an optical device a care must be taking such that the device performs according to the specification. One such
parameter is FOV obstruction. It is paramount that the FOV in the optical path is kept clear. The external aperture is
designed such to shape the FOV of the device and is installed prior calibration process thus cam be considered as part
of the device which does not impact the performance but may be used as a reference for the so called “Optical free
zone” – see Figure 27 hereafter.
Figure 25 Application examples concerning the optical consideration
13.2. Electrical considerations
Figure 26 MLX90640 electrical connections
As the MLX90640Bxx is fully I2C compatible it allows to have a system in which the MCU may be supplied with VDD=2.6V…5V
while the sensor it’s self is supplied from separate supply VDD1=3.3V (or even left with no supply i.e. VDD=0V), with the I2C
connection running at supply voltage of the MCU.

MLX90640 32x24 IR array
Datasheet
Page 54 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
13.3. Using the device in “image mode”
In some applications may not be necessary to calculate the temperature but rather to have just and image (for
instance in machine vision systems). In this case it is not necessary to carry out all calculations which would save
computation time or allow the one to use weaker CPU.
In order to get thermal image only following computation flow is to be used:
Figure 27 Calculation flow in thermal image mode
Ambient temperature calculation (common for all pixels) - 11.2.2.3
Gain compensation - 11.2.2.5.1
IR data compensation – offset, VDD and Ta - 11.2.2.5.3
IR data gradient compensation - 11.2.2.7
Normalizing to sensitivity - 11.2.2.8
Image (data) processing
Supply voltage value calculation (common for all pixels) - 11.2.2.2

MLX90640 32x24 IR array
Datasheet
Page 55 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
14. Application Comments
Significant contamination at the optical input side (sensor filter) might cause unknown additional filtering/distortion of the
optical signal and therefore result in unspecified errors.
IR sensors are inherently susceptible to errors caused by thermal gradients. There are physical reasons for these phenomena
and, in spite of the careful design of the MLX90640Bxx, it is recommended not to subject the MLX90640Bxx to heat transfer
and especially transient conditions.
The MLX90640Bxx is designed and calibrated to operate as a non-contact thermometer in settled conditions. Using the
thermometer in a very different way will result in unknown results.
Capacitive loading on an I2C can degrade the communication. Some improvement is possible with use of current sources
compared to resistors in pull-up circuitry. Further improvement is possible with specialized commercially available bus
accelerators. With the MLX90640Bxx additional improvement is possible by increasing the pull-up current (decreasing the
pull-up resistor values). Input levels for I2C compatible mode have higher overall tolerance than the I2C specification, but the
output low level is rather low even with the high-power I2C specification for pull-up currents. Another option might be to go
for a slower communication (clock speed), as the MLX90640Bxx implements Schmidt triggers on its inputs in I2C compatible
mode and is therefore not really sensitive to rise time of the bus (it is more likely the rise time to be an issue than the fall
time, as far as the I2C systems are open drain with pull-up).
Power dissipation within the package may affect performance in two ways: by heating the “ambient” sensitive element
significantly beyond the actual ambient temperature, as well as by causing gradients over the package that will inherently
cause thermal gradient over the cap
Power supply decoupling capacitor is needed as with most integrated circuits. MLX90640Bxx is a mixed-signal device with
sensors, small signal analog part, digital part and I/O circuitry. In order to keep the noise low power supply switching noise
needs to be decoupled. High noise from external circuitry can also affect noise performance of the device. In many
applications a 100nF SMD plus 10µF ceramic capacitors close to the Vdd and Vss pins would be a good choice. It should be
noted that not only the trace to the Vdd pin needs to be short, but also the one to the Vss pin. Using MLX90640Bxx with
short pins improves the effect of the power supply decoupling.
Check www.melexis.com for most recent application notes about MLX90640Bxx.

MLX90640 32x24 IR array
Datasheet
Page 56 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
15. Mechanical drawings
15.1. FOV 55°
Figure 28 Mechanical drawing of 55° FOV device

MLX90640 32x24 IR array
Datasheet
Page 57 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
15.2. FOV 110°
Figure 29 Mechanical drawing of 110° FOV device

MLX90640 32x24 IR array
Datasheet
Page 58 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
15.3. Device marking
The MLX90640 is laser marked with 10 symbols as follows.

Example: “0CA1010218” – Device type MLX90640BAA from lot 10102, sub LOT split 18 and Thermal Gradient
Compensation activated.
0
A
A
xxxxx
xx
Laser marking
2 digits Split number
5 digits LOT number
A
FOV = 110°
B
FOV = 55°
A
Device without thermal gradient compensation (TGC = 0 and may not be changed)
C
Device with thermal gradient compensation (TGC = -4…+3.98)
0
MLX90640

MLX90640 32x24 IR array
Datasheet
Page 59 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
16. Standard Information
Our products are classified and qualified regarding soldering technology, solderability and moisture sensitivity level
according to standards in place in Semiconductor industry.
For further details about test method references and for compliance verification of selected soldering method for
product integration, Melexis recommends reviewing on our web site the General Guidelines soldering
recommendation. For all soldering technologies deviating from the one mentioned in above document (regarding peak
temperature, temperature gradient, temperature profile etc.), additional classification and qualification tests have to
be agreed upon with Melexis.
For package technology embedding trim and form post-delivery capability, Melexis recommends consulting the
dedicated trim & forming recommendation application note: lead trimming and forming recommendations
Melexis is contributing to global environmental conservation by promoting lead free solutions. For more information
on qualifications of RoHS compliant products (RoHS = European directive on the Restriction Of the use of certain
Hazardous Substances) please visit the quality page on our website: http://www.melexis.com/en/quality-environment
17. ESD Precautions
Electronic semiconductor products are sensitive to Electro Static Discharge (ESD).
Always observe Electro Static Discharge control procedures whenever handling semiconductor products.
18. Revision history table
25/07/2016
Initial release
15/12/2016
Calibration data stored into EEPROM, pixel reading modes explained
17/01/2017
Some errors fixed
07/02/2017
Some calculations errors fixed
24/02/2017
Noise, FOV and accuracy graphs added, some inaccuracies fixed
02/03/2017
Overall rearranged , some typo and grammar mistakes fixed
18/05/2017
Two’s complement for IR data from RAM and CP, added outlier identification in EEPROM , added
application information
07/07/2017
Slave address changed to 0x240F, default mode is chess, CP RAM address changed 0x0709 ->
0x0708 and 0x0729 -> 0x0728, resolution control included in calculations, PCB under TO can
removed
30/08/2017
Laser marking added, Max number of fail pixels added, Measurement flow (continuous and step
mode) added, FOV definitions updated
10/10/2017
Added a note regarding CP averaging. Add dimension tolerances in mechanical drawings. Spelling
errors corrected

MLX90640 32x24 IR array
Datasheet
Page 60 of 60
REVISION 12 – DECEMBER 3, 2019
3901090640
11/04/2018
Updated accuracy table including BAB version, CP for different subpages, compensation for
different reading patterns, extended temperature ranges calculations.
03/08/2018
Added: github driver link, ESD changed from 2kV to 4kV, Step mode removed, Internal register
tables updated
03/12/2019
Rev 12: Max storage temp changed: 125°C to 85°C, added long term accuracy note, Ta accuracy
added, added optical considerations, electrical consideration diagram updated with 10µF, added
chamfer info, Doc server № added in the footer
19. Contact
For the latest version of this document, go to our website at www.melexis.com.
For additional information, please contact our Direct Sales team and get help for your specific needs:
Europe, Africa
Telephone: +32 13 67 04 95
Email : sales_europe@melexis.com
Americas
Telephone: +1 603 223 2362
Email : sales_usa@melexis.com
Asia
Email : sales_asia@melexis.com
20. Disclaimer
The information furnished by Melexis herein (“Information”) is believed to be correct and accurate. Melexis disclaims (i) any and all liability in connection with or arising out of the
furnishing, performance or use of the technical data or use of the product(s) as described herein (“Product”) (ii) any and all liability, including without limitation, special,
consequential or incidental damages, and (iii) any and all warranties, express, statutory, implied, or by description, including warranties of fitness for particular purpose, non-
infringement and merchantability. No obligation or liability shall arise or flow out of Melexis’ rendering of technical or other services.
The Information is provided "as is” and Melexis reserves the right to change the Information at any time and without notice. Therefore, before placing orders and/or prior to
designing the Product into a system, users or any third party should obtain the latest version of the relevant information to verify that the information being relied upon is current.
Users or any third party must further determine the suitability of the Product for its application, including the level of reliability required and determine whether it is fit for a
particular purpose.
The Information is proprietary and/or confidential information of Melexis and the use thereof or anything described by the Information does not grant, explicitly or implicitly, to
any party any patent rights, licenses, or any other intellectual property rights.
This document as well as the Product(s) may be subject to export control regulations. Please be aware that export might require a prior authorization from competent authorities.
The Product(s) are intended for use in normal commercial applications. Unless otherwise agreed upon in writing, the Product(s) are not designed, authorized or warranted to be
suitable in applications requiring extended temperature range and/or unusual environmental requirements. High reliability applications, such as medical life-support or life-
sustaining equipment are specifically not recommended by Melexis.
The Product(s) may not be used for the following applications subject to export control regulations: the development, production, processing, operation, maintenance, storage,
recognition or proliferation of 1) chemical, biological or nuclear weapons, or for the development, production, maintenance or storage of missiles for such weapons: 2) civil
firearms, including spare parts or ammunition for such arms; 3) defence related products, or other material for military use or for law enforcement; 4) any applications that, alone
or in combination with other goods, substances or organisms could cause serious harm to persons or goods and that can be used as a means of violence in an armed conflict or any
similar violent situation.
The Products sold by Melexis are subject to the terms and conditions as specified in the Terms of Sale, which can be found at https://www.melexis.com/en/legal/terms-and-
conditions.
This document supersedes and replaces all prior information regarding the Product(s) and/or previous versions of this document.
Melexis NV © - No part of this document may be reproduced without the prior written consent of Melexis. (2016)
ISO/TS 16949 and ISO14001 Certified
