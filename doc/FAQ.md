![](images/logo.png)

# FAQ

## Great! I can now monitor my heat pump! Can I change the configuration values too?

Not directly. It might be possible to change registry values using the serial port but I'm not aware of this. If you know, comment on [the dedicated issue](/../../issues/1).

However, ESPAltherma, supports an extra GPIO to control a relay that you can plug as *external On/Off thermostat*. See [**Controling your Daikin Altherma heat pump**](#controling-your-daikin-altherma-heat-pump).

If you want to configure your heat pump using an arduino, you can interact with the P1P2 serial protocol (the one of the digital thermostats) using the [nice work on P1P2Serial](https://github.com/Arnold-n/P1P2Serial) of Arnold Niessen.

## Where can I get more info on the protocol used?

It took quite some time to reverse engineer the protocol. If you're interested, I documented my findings [here](doc/Daikin%20I%20protocol.md).

## Is it safe? Can I break my machine?

It is as safe as interacting with a serial port can be. Pretty safe if you are a bit careful. Use is entirely at your own risk. No liability.

## Why not using the Daikin LAN adapter?

Of course you can probably achieve the same with the BRP069A62 adapter. However, it is expensive, not wifi and less fun than doing it yourself :)

## I selected a value but it is always returning 0 (or OFF)

The definition files contains values for a range of product. It is possible that some of the values are not implemented in your specific heat pump.

If it says 'conv XXX not avail.' it is that I did not implement this specific conversion of value. If you need this value, create an issue and I'll implement it.

## What is the meaning of this value?

Some times the names of the values can be cryptic. Sometimes, the names are more informative on other models: You can look for the registry in other model this can give you a hint. Eg.: One one file `0x62,15` is `"Pressure sensor"` => on the other `0x62,15` is `"Refrigerant pressure sensor"`.

I'm not an expert in heat pump, so I don't understand all possible values. Collectively however, I'm sure that we can understand a lot.

I created [a page in the WIKI](https://github.com/raomin/ESPAltherma/wiki/Information-about-Values). You can add your comments on the register values and suggest possible better names!

## My Daikin heat pump is not an Daikin Altherma. Can I still control it?

No, ESPAltherma supports only Altherma protocol. Other (AC only) units also have a serial port but using other protocols that would require extra reverse engineering to be implemented.

## How can I update ESPAltherma remotely?

Yes! ESPAltherma source code is upgraded often. Your ESPAltherma can be updated Over-The-Air without having to unplug it from the heat pump:

1. Download the updated code from the repository (or pull new changes) and report your configuration.
2. Open platformio.ini and uncomment the following line on your specific environment:

```ini
upload_port = ESPAltherma.local
```

With this parameter, the upload will happen over wifi. Note: your local firewall should allow incoming connection ; also, it can fail from time to time, if it happens just relaunch update.

## I'm using OpenHAB (or others) can I get the values in separated MQTT topics?

Yes, ESPAltherma now supports sending each value to a specific topic in addition to sending a complete JSON on the main topic.
To activate this specific feature uncomment the following lines from `src/setup.h`

```c++
//Uncomment this if you want to activate the One Value <-> One Topic mode. Each value will be sent to a specific topic below
#define ONEVAL_ONETOPIC
#define MQTT_OneTopic "espaltherma/OneATTR/" //Keep the ending "/" !!
```

Now each value will be published in `espaltherma/OneATTR/[valuename]` eg `espaltherma/OneATTR/Boiler Heating Target Temp.`

## How can I contribute?

Every contribution to this project is highly appreciated! Don't fear to create issues to report possible bugs or feature request. Pull requests which enhance or fix ESPAltherma are also greatly appreciated for everybody!

If this project is useful to you, and if you want, <b>[you can buy me a beer](https://www.buymeacoffee.com/raomin)</b>! It feels good and really helps improving ESPAltherma. Thanks :)

You can also [sponsor this project](https://github.com/sponsors/raomin/) (ie regular beers :)) and become an official supporter of ESPAltherma and get your badge on this page!