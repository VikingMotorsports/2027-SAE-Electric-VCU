#powershell install Script

$ProjectName = 'zephyrproject'
#function Install {	#python -m venv $ProjectName\.venv	#Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope LocalMachine -force
	python -m venv $ProjectName\.venv
	. "$PSScriptRoot\$ProjectName\.venv\Scripts\Activate.ps1"
	pip install west
	west init $ProjectName
	cd .\$ProjectName\
	west update
	west zephyr-export
	python -m pip install @((west packages pip) -split ' ')
	cd .\zephyr\
	west sdk install
	cd ..
#}