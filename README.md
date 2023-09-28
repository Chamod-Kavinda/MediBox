# MediBox
A Medibox is a tool that is used to remind users to take their medications at the appointed times and to store medications appropriately according to the rules.

## First stage

we developed a menu which is able to
* Set time zone by taking the offset from UTC as input
* Ability to set 3 alarms
* Ability to disable all three alarms at once

Three buttons were used to navigate the display and to set the time  and the time zone. An OLED display is used to display the current time and to display the menu option when the push button is clicked.

Because the chosen time zone is fetched from the NTP server over WI-FI, the ESP32 microcontroller, which has an integrated Bluetooth and WI-FI module, is used for this device.

We need to constantly monitor the temperature and the humidity inside the device to store the medical drugs under the given conditions. For this, a DHT22 temperature and humidity sensor is used.

A buzzer and an LED are utilized to notify the user of the medical intake time and to warn them of any unfavorable conditions inside the device.


## Second Stage

This stage attempts to improve Medibox's fundamental functions and add new features to increase the device's utility.

- It is essential to monitor light intensity when storing certain medicines as they may be sensitive to sunlight. For this, an LDR sensor is used.
-  A shaded sliding window has been installed to prevent excessive light from entering the Medibox. The shaded sliding window is connected to a servo motor responsible for adjusting the light intensity entering the Medibox.
	- The following equation represents the relationship between the motor angle and the intensity of light entering the Medibox:
	-  θ = θ_offset + (180 − θoffset) × I × γ 
	- where, 
	- θ is the motor angle 
	- θ_offset is the minimum angle (default value of 30 degrees) 
	- I is the intensity of light, ranging from 0 to 1 
	- γ is the controlling factor (default value of 0.75)
	
- Different medicines may have different requirements for the minimum angle and the controlling factor used to adjust the position of the shaded sliding window.
- To enable the user to adjust the minimum angle and controlling factor, Node-RED dashboard is created.
- In addition, I have implemented a dashboard to display the temperature and Humidity and another dashboard to change the alarm time schedules.

### Wokwi Simulation
![Wokwi](https://github.com/Chamod-Kavinda/MediBox/assets/129760133/7d17adc1-0e60-4faf-911c-1e53c07fa2c0)

### Dashboard
![Dashboard](https://github.com/Chamod-Kavinda/MediBox/assets/129760133/53251433-091d-4f7b-84ab-4e52ca24e0f7)

