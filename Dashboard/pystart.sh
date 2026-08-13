#!/bin/bash

nmcli con up VCUWIFI
#waiting for Wifi to be up before starting script
sleep 3
source venv/bin/activate

python3  driver_dash_pyqt_no_pit_display.py
