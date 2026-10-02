#powershell script for building and flashing to board

#Zephyr workspace location and name
$projectname = 'zephyrproject'
$projectdir = '~\zephyr'

#Target board to flash
$board = 'nucleo_c092rc'

#Application title
$app = 'counter'

#Application directory (from main repo folder)
$dir = '/Zephyr_apps'
#Repository path (aka current location), will return here
$path = 'C:\Users\casey\github\2027-SAE-Electric-VCU'

cd $projectdir\$projectname\
.venv\scripts\activate.ps1
west build -p always -b $board $path\$dir\$app
west flash
cd $path