![](doc/images/logo.png)

<hr/>

<p align="center">
<a href="https://travis-ci.com/raomin/ESPAltherma"><img src="https://app.travis-ci.com/raomin/ESPAltherma.svg?branch=main&status=passed" /></a>
&nbsp;
<img src="https://img.shields.io/github/last-commit/raomin/ESPAltherma?style=for-the-badge" />
&nbsp;
<img src="https://img.shields.io/github/license/raomin/ESPAltherma?style=for-the-badge" />
&nbsp;
<a href="https://github.com/sponsors/raomin/"><img src="https://github.com/raomin/ESPAltherma/blob/main/doc/images/sponsor.png?raw=true"></a>
&nbsp;
<a href="https://www.buymeacoffee.com/raomin" target="_blank"><img src="https://img.shields.io/badge/Buy%20me%20a%20beer-%245-orange?style=for-the-badge&logo=buy-me-a-beer" /></a>
</p>

<hr/>

<p><b>ESPAltherma</b> is a solution to monitor Daikin Altherma / ROTEX / HOVAL Belaria heat pump activity using just Arduino on an <b>ESP32</b> or <b>ESP8266</b> Microcontroller.</p>

_If this project has any value for you, please consider [buying me a 🍺](https://www.buymeacoffee.com/raomin) or even better [sponsoring ESPAltherma](https://github.com/sponsors/raomin/)!. I don't do this for money but it feels good to get some support! Thanks :)_

## Features

  <ul style="list-style-position: inside;">
    <li>Easy configuration with web based config system.</li>
    <li>Connects with the serial port of Altherma on port X10A.</li>
    <li>Needs just an ESP32, no need for extra hardware. ESP8266 is also supported.</li>
    <li>Queries the Altherma for selected values at defined interval.</li>
    <li>Converts and formalizes all values in a JSON message sent over MQTT.</li>
    <li>Easily integrates with Home Assistant's MQTT auto-discovery.</li>
    <li>Supports update OverTheAir</li>
    <li>Log messages in Serial + WebUI + Screen (m5StickC)</li>
    <li>Optional: can control your heat pump. (Heating, Colling and SmartGrid)</li>
    <li>Optional: Connects with the CAN-Bus (external controller or transciever needed).</li>
</ul>

## Preview

![](doc/images/screenshot.png)

# Documentation

- [Prerequisites](/doc/Prerequisites.md)
- [Getting Started](/doc/GettingStarted.md)
- [Troubleshooting](/doc/Troubleshooting.md)
- [Integrating with Home Assitant](/doc/IntegratingHomeAssitant.md)
- [FAQ](/doc/FAQ.md)

# Contributing

- You can find tools and information in the [contrib](contrib/) subfolder
- The `Daikin I` protocol is documented [here](/doc/Daikin%20I%20protocol.md)
- The `Daikin S` protocol is documented [here](/doc/Daikin%20S%20protocol.md)

# ❤ Regular Sponsors ❤

<a href="https://github.com/gerione">@gerione</a><br/>
<a href="https://github.com/kloni">@retrack (Antoine Coetsier)</a><br/>
<a href="https://github.com/EvertJob">@EvertJob (toppe)</a><br/>
<a href="https://github.com/FusisCaesar">@FusisCaesar</a><br/>
<a href="https://github.com/Mychel60">Michael</a><br/>

# License
ESPAltherma is licensed under ![MIT Licence](https://img.shields.io/github/license/raomin/ESPAltherma.svg?style=for-the-badge)
