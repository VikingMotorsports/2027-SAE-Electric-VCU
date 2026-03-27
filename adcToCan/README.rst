.. zephyr:code-sample:: can-counter
   :name: Controller Area Network (CAN) counter
   :relevant-api: can_controller

   Send and receive CAN messages.

Overview
********

This sample demonstrates how to use the Controller Area Network (CAN) API.
Messages with standard and extended identifiers are sent over the bus.
Messages are received using message-queues and work-queues.
Reception is indicated by blinking the LED (if present) and output of
received counter values to the console.

Building and Running
********************

In loopback mode, the board receives its own messages. This could be used for
standalone testing.

The LED output pin is defined in the board's devicetree.

The sample can be built and executed for boards with a SoC that have an
integrated CAN controller or for boards with a SoC that has been augmented
with a stand alone CAN controller.

Integrated CAN controller
=========================

For the NXP TWR-KE18F board:

.. zephyr-app-commands::
   :zephyr-app: samples/drivers/can/counter
   :board: twr_ke18f
   :goals: build flash

Stand alone CAN controller
==========================

For the nrf52dk/nrf52832 board combined with the DFRobot CAN bus V2.0 shield that
provides the MCP2515 CAN controller:

.. zephyr-app-commands::
   :zephyr-app: samples/drivers/can/counter
   :board: nrf52dk/nrf52832
   :shield: dfrobot_can_bus_v2_0
   :goals: build flash

Sample output
=============

.. code-block:: console

   Change LED filter ID: 0
   Finished init.
   Counter filter id: 4

   uart:~$ Counter received: 0
   Counter received: 1
   Counter received: 2
   Counter received: 3

.. note:: The values shown above might differ.


.. zephyr:code-sample:: adc_dt
   :name: Analog-to-Digital Converter (ADC) with devicetree
   :relevant-api: adc_interface

   Read analog inputs from ADC channels.

Overview
********

This sample demonstrates how to use the :ref:`ADC driver API <adc_api>`.

Depending on the target board, it reads ADC samples from one or more channels
and prints the readings on the console. If voltage of the used reference can
be obtained, the raw readings are converted to millivolts.

The pins of the ADC channels are board-specific. Please refer to the board
or MCU datasheet for further details.

Building and Running
********************

The ADC peripheral and pinmux is configured in the board's ``.dts`` file. Make
sure that the ADC is enabled (``status = "okay";``).

In addition to that, this sample requires an ADC channel specified in the
``io-channels`` property of the ``zephyr,user`` node. This is usually done with
a devicetree overlay. The example overlay in the ``boards`` subdirectory for
the ``nucleo_l073rz`` board can be easily adjusted for other boards.

Configuration of channels (settings like gain, reference, or acquisition time)
also needs to be specified in devicetree, in ADC controller child nodes. Also
the ADC resolution and oversampling setting (if used) need to be specified
there. See :zephyr_file:`boards/nrf52840dk_nrf52840.overlay
<samples/drivers/adc/adc_dt/boards/nrf52840dk_nrf52840.overlay>` for an example of
such setup.

Building and Running for ST Nucleo L073RZ
=========================================

The sample can be built and executed for the
:zephyr:board:`nucleo_l073rz` as follows:

.. zephyr-app-commands::
   :zephyr-app: samples/drivers/adc/adc_dt
   :board: nucleo_l073rz
   :goals: build flash
   :compact:

To build for another board, change "nucleo_l073rz" above to that board's name
and provide a corresponding devicetree overlay.

Sample output
=============

You should get a similar output as below, repeated every second:

.. code-block:: console

   ADC reading:
   - ADC_0, channel 7: 36 = 65mV

.. note:: If the ADC is not supported, the output will be an error message.
