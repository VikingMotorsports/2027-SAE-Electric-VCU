#script for finding and opening stm board serial port in PUTTY

#searches serial devices for "STMicroelectronics", selects just port info, then grep returns the line with just COMxx
$COMPORT = Get-CimInstance -Class Win32_SerialPort -Filter "name like 'STMicroelectronics%'" | SELECT DeviceID | findstr.exe 'COM'

#opens putty, connects to serial using COM port found above
putty -serial $COMPORT -sercfg 115200,8,n,1,N