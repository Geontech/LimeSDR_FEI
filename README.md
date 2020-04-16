# LimeSDR Redhawk Device


## Dependencies on CentOS 7
You will need `cmake3` from EPEL:

    sudo yum install epel-release cmake3 gnuplot -y
    sudo yum install libusb* -y
    sudo yum install sqlite sqlite-devel -y

There's probably other deps but I haven't tested this build from a clean image so I already had them...


## Build/Install LimeSDR Drivers
There are several different tools built by the LimeSuite project that you might not care about if you just want the driver and docs. There was a `wxWidgets` issue in CentOS 7 that prevents the build of some GUI applications but those flags can be turned off. The `wxWidgets` and `LimeSuite` install instructions are below. If you do not want `wxWidgets` skkp that install and remove the "-DwxWidgets_USE_STATIC:BOOL=ON -DwxWidgets_CONFIG_EXECUTABLE:FILEPATH=~/bin/wxWidgets-staticlib/bin/wx-config" from the cmake3 step in LimeSuite. Also, I had to explicitly turn on the documentation build.

### wxWidgets 3.1.0

	$ cd ~/bin/
	$ wget https://github.com/wxWidgets/wxWidgets/releases/download/v3.1.0/wxWidgets-3.1.0.tar.bz2
	$ tar -xf wxWidgets-3.1.0.tar.bz2  
	$ cd wxWidgets-3.1.0/
	$ mkdir -p ~/bin/wxWidgets-staticlib
	$ ./autogen.sh 
	$ ./configure --with-opengl --disable-shared --enable-monolithic --with-libjpeg --with-libtiff --with-libpng --with-zlib --disable-sdltest --enable-unicode --enable-display --enable-propgrid --disable-webkit --disable-webview --disable-webviewwebkit --prefix=`echo ~/bin/wxWidgets-staticlib` CXXFLAGS="-std=c++0x"
	$ make -j4
	$ sudo make install

### LimeSuite

	$ cd /tmp
    $ git clone https://github.com/myriadrf/LimeSuite
    $ cd LimeSuite
    $ git checkout stable
    $ mkdir builddir && cd builddir
    $ cmake3 .. -DENABLE_GUI=OFF -DENABLE_API_DOXYGEN=ON -DwxWidgets_USE_STATIC:BOOL=ON -DwxWidgets_CONFIG_EXECUTABLE:FILEPATH=~/bin/wxWidgets-staticlib/bin/wx-config
    $ make -j4
    $ sudo make install
    $ sudo ldconfig

    # enable non-root users to access usb-based devices like the LimeSDR
    $ cd ../udev-rules
    $ sudo ./install.sh

I thought the `ldconfig` step is supposed to do this but I still had to add `/usr/local/lib` to my library path:

    $ export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

Now plug in your LimeSDR and see if you can find it:

    $ LimeUtil --find
      * [LimeSDR-USB, media=USB 3.0, module=FX3, addr=1d50:6108, serial=0009061C02D10A14]

You may have also built the `LimeQuickTest` utility which does more stuff:

    $ LimeQuickTest
    [ TESTING STARTED ]
    ->Start time: Sat Dec 14 10:49:03 2019

    Firmware version mismatch!
      Expected firmware version 4, but found version 3
      Follow the FW and FPGA upgrade instructions:
      http://wiki.myriadrf.org/Lime_Suite#Flashing_images
      Or run update on the command line: LimeUtil --update

    Gateware version mismatch!
      Expected gateware version 2, revision 21
      But found version 2, revision 11
      Follow the FW and FPGA upgrade instructions:
      http://wiki.myriadrf.org/Lime_Suite#Flashing_images
      Or run update on the command line: LimeUtil --update

    ->Device: LimeSDR-USB, media=USB 3.0, module=FX3, addr=1d50:6108, serial=0009061C02D10A14
      Serial Number: 0009061C02D10A14

    # ... more tests

    => Board tests PASSED <=

    Elapsed time: 1.40 seconds

This shows a mismatch in firmware (ARM) and gateware (FPGA) versions with this software driver since I haven't updated that in a really long time so let's do that.


## Update LimeSDR Firmware/Gateware
This is amazingly simple if you are connected to the Internet, you simply run `LimeUtil --update` and it will determine what needs to be updated, downloads it and programs the device. If you're not on the Internet... then I guess look at [these instructions](http://wiki.myriadrf.org/Lime_Suite#Flashing_images).

When we run the test again after updating, no more warnings:

    $ LimeQuickTest 
    [ TESTING STARTED ]
    ->Start time: Sat Dec 14 10:53:52 2019

    ->Device: LimeSDR-USB, media=USB 3.0, module=FX3, addr=1d50:6108, serial=0009061C02D10A14
      Serial Number: 0009061C02D10A14

    # ... more tests

    => Board tests PASSED <=

    Elapsed time: 1.55 seconds

## Build/Install LimeSDR Redhawk Device
Now you should be ready to build the Redhawk Device.

    $ git clone https://github.com/timcardenuto/LimeSDR_FEI
    $ cd LimeSDR_FEI
    $ ./build.sh
    $ ./build.sh install

To test the Device you can use the Redhawk IDE. NOTE: make sure you open it from the same terminal that has the `LD_LIBRARY_PATH` set to include the installed location of the `libLimeSuite.so` library (in the default case this is `/usr/local/lib`). (**Note**: Adding `/usr/local/lib` will cause pip and other python functions to break, so if you need to autogenerate code the LD_LIBRARY_PATH must be restored). Otherwise when you launch the Device it will fail to find that library. This is probably something that should be installed to the Redhawk shared library path. Also again, this library path should be part of the system path after using `ldconfig`... not sure why it doesn't.

    $ rhide &

With the Redhawk IDE open, navigate to the `REDHAWK Explorer` panel > `Target SDR` > `Devices` > Right click on `LimeSDR_FEI` > `Launch in Sandbox` > `cpp`. This should launch the Device and show a bunch of configuration info in the Console. To Allocate a Tuner, right click on the running LimeSDR Device and select Allocate, then enter the Frequency, Bandwidth, and Sample Rate that you want.

TODO there are bugs with this. Not sure what acceptable values are and no data shows up when it says it's successful.... Probably need to update API's for both LimeSDR and Redhawk Device since it's been 2 years.

## Testing LimeSDR Redhawk Device

### FM Test
Test the LimeSDR device by checking that the receiver can pick up the local FM radio stations.

1. After installing the LimeSDR_FEI go to TARGET SDR > Node > Right click on LimeSDR_FEI
2. Click `Launch Device Manager`
3. Connect to the TOA
4. Expand Device Managers > LimeSDR_FEI > LimeSDR_FEI_1 > FrontEnd Tuner 
5. Right click on RX_Digitizer and press allocate
6. Enter in 90MHz center frequency, 10MHz bandwidth, 20Msps sample rate, and then click OK
7. Right click dataFloat_out and then "Plot FFT"
8. Review the Raster graph
9. Zoom in on each dark white line and identify where the peak of the frequency is (check for the existence of this station)

