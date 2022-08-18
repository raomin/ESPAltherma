var str_val = function(item) {
  return itemRegistry.getItem(item).getState().toString();
};

var float_val = function(item) {
  return parseFloat(itemRegistry.getItem(item).getState());
};

var cop = 0;

var operation_mode = str_val('espaltherma_operation_mode');
var freeze_protection = str_val('espaltherma_freeze_protection');

if (operation_mode === 'Heating' && freeze_protection === 'OFF') {
  var current = float_val('espaltherma_compressor_primary_current');

  if (current > 0) {
    var voltage = float_val('smartpi_v1');
    if (! voltage) {
      voltage = 232;
    }

    var cos_phi = float_val('smartpi_cos1');
    if (! cos_phi) {
      cos_phi = 0.98;
    }

    var power = current * voltage * cos_phi;

    var flow = float_val('espaltherma_flow_sensor');
    var lw_temp = float_val('espaltherma_leaving_water_temp_before_buh');
    var iw_temp = float_val('espaltherma_inlet_water_temp');

    var diff_temp = lw_temp - iw_temp;

    var factor_flow = 59.7384; // converts l/min to kg/h for water at 30°C
    var thermal_capacity = 1.1626; // of water in Wh/(kg•K)

    cop = flow * factor_flow * thermal_capacity * diff_temp / power;
    cop = Math.round(cop * 1000) / 1000;
  }
}

events.sendCommand('espaltherma_cop', cop);
