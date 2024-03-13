![](images/logo.png)

# Integrating with Home Assitant

ESPAltherma integrates easily with Home Assistant using [mqtt discovery](https://www.home-assistant.io/docs/mqtt/discovery/).

After setup, ESPAltherma will generate 2 entities on Home Assistant:

![](images/haentities.png)

- `sensor.althermasensors` holds the values as attributes.

- `switch.altherma` activates the relay connected to the `PIN_THERM`

## Declaring sensor entities

In Home Assistant, all values reported by ESPAltherma are `attribute`s of the `entity` sensor.althermasensors.

![](images/attribs.png)

If you want to integrate specific `attribute`s in graphs, gauge etc. you need to declare them as `sensor`s using `template` in your `configuration.yaml`. See [HA doc on Template](https://www.home-assistant.io/integrations/template/).

Eg. this template declares the 2 operation modes as entities, the DHW tank temperature and the current of the primary inverter:

```yaml
template:
  - unique_id: "espaltherma"  # will be prefixed to all unique IDs
    sensors:
    - name: "Operation mode"
      unique_id: "operation"
      state: "{{ state_attr('sensor.althermasensors','Operation Mode') }}"
    - name: "Indoor Operation mode"
      unique_id: "iuoperation"
      state: "{{ state_attr('sensor.althermasensors','I/U operation mode') }}"
    - name: "DHW Temp"
      unique_id: "dhw"
      state: "{{ state_attr('sensor.althermasensors','DHW tank temp. (R5T)') }}"
      unit_of_measurement: '°C'
    - name: "Inverter primary current"
      unique_id: "inv_primary_current"
      state: "{{ state_attr('sensor.althermasensors','INV primary current (A)') }}"
      unit_of_measurement: 'A'
      device_class: current
```

After restarting Home Assistant, these entities can be added to an history card:

![](images/historycard.png)

## A Climate entity

To control heating through the On/Off switch, declare a Climate (aka thermostat) entity monitoring a temperature sensor.

```yaml
climate:
  - platform: generic_thermostat
    name: Altherma
    heater: switch.altherma
    target_sensor: sensor.temproom1
    min_temp: 15
    max_temp: 25
    cold_tolerance: 0.5
    hot_tolerance: 0.5
    min_cycle_duration:
      minutes: 30
    away_temp: 15
    precision: 0.1
```

Then, add a Thermostat card somewhere:

![ha thermostat](images/thermostat.png)

## Calculating COP

The information returned by ESPAltherma allows to calculate the coefficient of performance (COP). It is the ratio of the heat delivered by your heat pump to the energy consumed by it.

When put in terms of ESPAltherma variables, the COP can be define as a sensor like this in the `sensor:` section of Home Assistant:

```yaml
    - name: "COP"
      unique_id: "espaltherma_cop"
      unit_of_measurement: 'COP'
      state: "{% if is_state_attr('sensor.althermasensors','Operation Mode', 'Heating') and is_state_attr('sensor.althermasensors','Freeze Protection', 'OFF')  %}
{{
  ((state_attr('sensor.althermasensors','Flow sensor (l/min)')| float * 0.06 * 1.16 * (state_attr('sensor.althermasensors','Leaving water temp. before BUH (R1T)') | float - state_attr('sensor.althermasensors','Inlet water temp.(R4T)')|float) )
    /
  (state_attr('sensor.althermasensors','INV primary current (A)') | float * state_attr('sensor.althermasensors','Voltage (N-phase) (V)')|float / 1000))
  |round(2)
}}
{% else %} 0 {%endif%}"
```