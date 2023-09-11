bluez-ibeacon
=============

Complete example of using Bluez as an iBeacon

How to use
==========

To use this example you will need to install [Bluez](http://www.bluez.org/)
either compiled by hand or through a development packaged libbluetooth. BTLE
support requires a recent version of Bluez so make sure to install the latest
version available. 
要使用此示例，您需要安装[Bluez](http://www.bluez.org/)，手动编译或通过开发包libbluetooth。
BTLE支持需要最新版本的Bluez，因此请确保安装可用的最新版本。

After installing Bluez you can make the ibeacon binary in the bluez-beacon
directory. 
安装Bluez后，您可以在Bluez -beacon目录中制作ibeacon二进制文件

Fire up XCode and run the BeaconDemo app on a device that supports BTLE such
as the iPhone 5 or later. The information displayed on screen is needed to run
the Bluez beacon.
启动XCode并在支持BTLE的设备(如iPhone 5或更高版本)上运行BeaconDemo应用程序。
运行Bluez beacon需要屏幕上显示的信息。

Take the UUID displayed in the app along with the major and minor version and
plug those into the ibeacon binary like this:
获取应用程序中显示的UUID以及主版本和次版本，并将它们插入ibeacon二进制文件，如下所示:
```
./ibeacon 200 <UUID> <Major Number> <Minor Number> -29
```

If everything goes correctly you will get an alert on the device that you
have entered the region of the beacon. It can take a few seconds to register
so you may want to give it time if it doesn't pick up instantly. You may also
want to double check that the UUID is entered correctly if it doesn't seem to
work.
如果一切正常，您将在设备上得到一个警报，您已经进入信标区域。
它可能需要几秒钟来注册，所以如果它不能立即恢复，你可能需要一些时间。
如果UUID似乎不起作用，你可能还想再次检查UUID是否输入正确。


The passbook example uses a UUID of e2c56db5-dffb-48d2-b060-d0f5a71096e0, a
marjor number of 1 and a minor number of 1. After installing it you can use
the ibeacon program to advertise for it with the following options:
本例中passbook的UUID为e2c56db5-dffb-48d2-b060-d0f5a71096e0，主用号码为1，副号码为1。
安装之后，你可以使用ibeacon程序通过以下选项来通告它:

```
./ibeacon 200 e2c56db5dffb48d2b060d0f5a71096e0 1 1 -29
```

*Note*

I've used both the [IOGEAR Bluetooth 4.0 USB Micro Adapter (GBU521)](http://www.amazon.com/dp/B007GFX0PY)
and the [Cirago Bluetooth 4.0 USB Mini Adapter (BTA8000)](http://www.amazon.com/dp/B0090I9NRE) successfully.

License
=======

MIT - See the LICENSE file
