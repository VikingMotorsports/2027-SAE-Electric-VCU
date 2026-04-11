#powershell script for building and flashing to board

$projectname = 'zephyrproject'
$board = 'nucleo_c092rc'
<<<<<<< HEAD
$app = 'canToPi' 
=======
>>>>>>> c6e51a0fbd6301c2c3ff23b62fc89898d0940420
$app = 'canToDisplay' 
$dir = '2027-SAE-Electric-VCU'
$path = 'C:\Users\casey\OneDrive\Desktop\ECE413'

cd ~\zephyr\$projectname\
.venv\scripts\activate.ps1
west build -p always -b $board $path\$dir\$app
west flash
cd $path\$dir