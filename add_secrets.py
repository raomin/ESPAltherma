Import("env")

import os

try:
  from dotenv import load_dotenv
except ImportError:
  env.Execute("$PYTHONEXE -m pip install python-dotenv")
  from dotenv import load_dotenv

from dotenv import load_dotenv

load_dotenv()

# check if environment variable exists (try/except), then read out...
wifi_ssid = os.environ['ESPALTHERMA_WIFI_SSID']
wifi_pwd = os.environ['ESPALTHERMA_WIFI_PWD']

mqtt_host = os.environ['ESPALTHERMA_MQTT_SERVER']
mqtt_port = os.environ['ESPALTHERMA_MQTT_PORT']
mqtt_user = os.environ['ESPALTHERMA_WIFI_USERNAME']
mqtt_pwd = os.environ['ESPALTHERMA_MQTT_PWD']


env.Append(CPPDEFINES=[
  ("ESPALTHERMA_WIFI_SSID", wifi_ssid),
  ("ESPALTHERMA_WIFI_PWD", wifi_pwd),
  ("ESPALTHERMA_MQTT_SERVER", mqtt_host),
  ("ESPALTHERMA_MQTT_PORT", mqtt_port),
  ("ESPALTHERMA_WIFI_USERNAME", mqtt_user),
  ("ESPALTHERMA_MQTT_PWD", mqtt_pwd)
])