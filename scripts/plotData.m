clc,clear, close all

fid = fopen('myLog.dat','rb');
data = fread(fid,Inf, 'int16');
fclose(fid);

voltageResolution = 1/16;
currentResolution = 200/(2^16);
voltageChan0 = data(1:5:end)*voltageResolution;
currentChan0 = data(2:5:end)*currentResolution;
currentChan1 = data(3:5:end)*currentResolution;
currentChan2 = data(4:5:end)*currentResolution;
currentChan3 = data(5:5:end)*currentResolution;

samplingRate = 1.25e6/1024;  %Hz The real sampling rate is not exactly 1.25kHz
voltageChannelNum = 1;
currentChannelNum = 4;

time = [0:1/samplingRate:(numel(data)-1)/((voltageChannelNum + currentChannelNum)*samplingRate)];

figure(1)
subplot(2,1,1), plot(time, voltageChan0), ylabel('Voltage (mV)'), xlabel('Time (s)')
subplot(2,1,2), plot(time, currentChan3), ylabel('Current (nA)'), xlabel('Time (s)')
