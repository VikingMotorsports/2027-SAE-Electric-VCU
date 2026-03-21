#powershell script for building and flashing to board

$projectname = 'zephyrproject'
$board = 'nucleo_c092rc'
$app = 'sensor_shell' 
cd ~\zephyr\$projectname\
.venv\scripts\activate.ps1
west build -p always -b $board ..\..\OneDrive\Desktop\ECE413\VMS-VCU\$app
west flash
cd ~\OneDrive\Desktop\ECE413\VMS-VCU