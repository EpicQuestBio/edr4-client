# ELEMENTS ER4COMMLIB FOR E4 DEVICE - PROGRAMMING EXAMPLE
## BRIEF DESCRIPTION
This project briefly illustrates how to use fundamental functions offered by the ER4COMMLIB to develop your applications for the ELEMENTS SRL e4 device. The main.cpp example shows
- how to detect the device
- how to connect to the device
- how to check the voltage/current channel numbers
- how to check the available current ranges and sampling rates and how to set the desired values
- how to use the digital offset compensation
- how to check the voltage stimulus protocol offered by the device
- how to choose a protocol, set its parameters and apply it to the device.
- a few illustrative error codes returned commonly
The protocol results (voltage and current traces) are saved to file.
## REQUIREMENTS
Currently supported platforms:
- Windows 10+
### DRIVERS
Install EDR4 to make sure all the needed drivers are correctly installed [elements-ic.com/downloads/](https://elements-ic.com/downloads/)
## CONTENT OF THIS ARCHIVE
- readme.md: this readme file
- LICENSE: the license we chose
- main.cpp: a short coding example exploiting the er4commlib
- include folder: which includes all the headers for the er4commlib
- libs\er4commlib folder: which includes the dynamically and statically built er4commlib
- libs\dependencies folder: which includes the third party libraries
- scripts\plotData.m: a Matlab script to plot the voltage and current traces saved to file by main.cpp when applying a protocol
### BUILDING THE LIBRARY YOURSELF
The er4commlib source code is available on github [github.com/Elements-SRL/er4commLib](https://github.com/Elements-SRL/er4commLib/)
## HOW TO COMPILE THIS EXAMPLE
main.cpp can be compiled using visual studio 2017 or newer 64bit
- unzip this archive in a folder of your choice (hereafter FOLDER)
- check you have Visual Studio installed (the Community release can be used for free)
- add the preprocessor defines ER4COMMLIB_STATIC, FTD2XX_STATIC, FT_VER_MAJOR, FT_VER_MINOR, FT_VER_BUILD and FTDIMPSSE_STATIC
- add include and all its subfolders to the additional include directories
- add libs\er4commlib\static and libs\er4commlib\dependencies to the additional libraries directories
- add er4commlib.lib, ftd2xx.lib and libMPSSE.lib to the additional dependencies
## HOW TO RUN THIS EXAMPLE
- connect the e4 device to your PC
- run the main.exe executable produced by the previous compilation steps
- a new myLog.dat file will be saved in folder
- visualize the results using the Matlab script plotData.m or write your own visualization script e.g. in Python
- the data is saved in file myLog.dat with the following configuration:
	- current range 2nA
	- sampling rate 1.25kHz
	- model cell 	100MOhm
	- protocol time 10s
	- protocol		conductance protocol
		- holding voltage 	2mV
		- pulse voltage	 	100mV
		- step voltage 		20mV
		- holding time 		5ms
		- pulse time 		4ms