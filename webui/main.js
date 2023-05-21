"use strict";
let customParametersList = [];
let selectedModelPresets = [];
let selectedModelParameters = [];
let filteredModelParameters = [];
let customCommandsList = [];
let modelCommands = [];
let models = [];
let canCommands = [];
let boardDefaults = {};
let currentFirmwareVersion = "";

let fetchWifiNetworksBtnValue;
let fetchWifiErrorCounter = 0;

let fetchDataIntervalHandler;
let fetchDataIntervalId;

window.addEventListener('load', async function () {
    document.getElementById('submit').addEventListener('click', sendConfigData);
    document.getElementById('startUpdate').addEventListener('click', sendUpdate);
    document.getElementById('btnWifiListRefresh').addEventListener('click', loadWifiNetworks);
    document.getElementById('linkAddValueCode').addEventListener('click', addValueCode);
    document.getElementById('linkRemoveValueCode').addEventListener('click', removeValueCode);
    document.getElementById('ssid_select').addEventListener('change', selectWifiNetwork);

    await loadBoardDefaults();
    await refreshModels();
    await refreshCANCommands();
    await loadConfig();

    document.getElementById('nav-main').querySelectorAll('a').forEach(function(navLink) {
        navLink.addEventListener('click', handleNavigation);
    });

    document.getElementById('loading-dialog').close();
});

async function handleNavigation(event)
{
    event.preventDefault();

    document.getElementById('nav-main').querySelectorAll('a').forEach(function(navLink) {
        navLink.removeAttribute('role');
    });

    document.querySelectorAll('article[role=tab]').forEach(function(navLink) {
        navLink.style.display = 'none';
    });

    let targetElement = event.target || event.srcElement;
    targetElement.setAttribute('role', 'button');

    let articleId = targetElement.getAttribute('data-link');
    document.getElementById(articleId).style.display = 'block';
}

async function loadBoardDefaults()
{
    await fetch('/loadBoardInfo', {
        method: "GET"
    })
    .then(function(response) { return response.json(); })
    .then(function(data) {
        const pinSelects = document.querySelectorAll('input[data-pins]');

        pinSelects.forEach((input) => {
            input.type = "number";
            input.addEventListener('change', (event) => {
                if(event.target.value == "") {
                    event.target.value = event.target.defaultValue;
                }
            });

        });

        boardDefaults = data['Default'];
        currentFirmwareVersion = data['Version'];
        document.getElementById('firmware-version').innerText = currentFirmwareVersion;
        resetToDefaults();
    })
    .catch(function(err) {
        alert('Fetching pins data failed! Message: ' + err);
    });
}

async function updateWifiFields()
{
    let useStandalone = document.getElementById('standalone_wifi').checked;
    let localUpdateFunction = function(fieldId) {
        if(useStandalone)
            document.getElementById(fieldId).setAttribute("disabled", true);
        else
            document.getElementById(fieldId).removeAttribute("disabled");
    }

    localUpdateFunction('ssid');
    localUpdateFunction('ssid_select');
    localUpdateFunction('btnWifiListRefresh');
    localUpdateFunction('ssid_password');

    if(useStandalone && document.getElementById('ssid_staticip').checked)
    {
        document.getElementById('ssid_staticip').checked = false;
        show('staticip', true);
    }
    else
    {
        show('staticip', !document.getElementById('ssid_staticip').checked);
    }

    localUpdateFunction('ssid_staticip');
    localUpdateFunction('ssid_ip');
    localUpdateFunction('ssid_subnet');
    localUpdateFunction('ssid_gateway');
    localUpdateFunction('primary_dns');
    localUpdateFunction('secondary_dns');
}

async function resetToDefaults()
{
    if(!Object.keys(boardDefaults).length)
        return;

    document.getElementById('pin_rx').value = boardDefaults['pin_rx'];
    document.getElementById('pin_tx').value = boardDefaults['pin_tx'];
    document.getElementById('pin_heating').value = boardDefaults['pin_heating'];
    document.getElementById('pin_cooling').value = boardDefaults['pin_cooling'];
    document.getElementById('pin_sg1').value = boardDefaults['pin_sg1'];
    document.getElementById('pin_sg2').value = boardDefaults['pin_sg2'];
    document.getElementById('pin_can_uart_rx').value = boardDefaults['uart']['pin_rx'];
    document.getElementById('pin_can_uart_tx').value = boardDefaults['uart']['pin_tx'];
    document.getElementById('pin_can_spi_mosi').value = boardDefaults['spi']['mosi'];
    document.getElementById('pin_can_spi_miso').value = boardDefaults['spi']['miso'];
    document.getElementById('pin_can_spi_sck').value = boardDefaults['spi']['sck'];
    document.getElementById('pin_can_spi_cs').value = boardDefaults['spi']['cs'];
    document.getElementById('pin_can_spi_int').value = boardDefaults['spi']['int'];
    document.getElementById('pin_can_spi_mhz').value = boardDefaults['spi']['mhz'];
    document.getElementById('can_speed_kbps').value = boardDefaults['can_speed_kbps'];
    document.getElementById('can_mqtt_topic_name').value = boardDefaults['can_mqtt_topic_name'];
    document.getElementById('can_autopoll_mode_disabled').checked = true;
    document.getElementById('can_autopoll_time').value = boardDefaults['can_autopoll_time'];
    document.getElementById('pin_enable_config').value = boardDefaults['pin_enable_config'];
    document.getElementById('frequency').value = boardDefaults['frequency'];
    document.getElementById('mqtt_topic_name').value = boardDefaults['mqtt_topic_name'];
    document.getElementById('mqtt_onetopic_name').value = boardDefaults['mqtt_onetopic_name'];
    document.getElementById('mqtt_port').value = boardDefaults['mqtt_port'];

    document.querySelectorAll('input[data-pins]').forEach((input) => {
        input.defaultValue = input.value;
    });
}

async function loadConfig()
{
    await fetch('/loadConfig', {
        method: "GET"
    })
    .then(function(response) { return response.json(); })
    .then(async function(data) {
        // if no config exists yet
        if(Object.keys(data).length == 0)
            return;

        document.getElementById('standalone_wifi').checked = data['STANDALONE_WIFI'];

        if(!data['STANDALONE_WIFI'])
        {
            document.getElementById('ssid').value = data['SSID'];
            document.getElementById('ssid_password').value = data['SSID_PASSWORD'];
            document.getElementById('ssid_staticip').checked = data['SSID_STATIC_IP'];

            if(data['SSID_STATIC_IP'])
            {
                document.getElementById('ssid_ip').value = data['SSID_IP'];
                document.getElementById('ssid_subnet').value = data['SSID_SUBNET'];
                document.getElementById('ssid_gateway').value = data['SSID_GATEWAY'];
                document.getElementById('primary_dns').value = data['SSID_PRIMARY_DNS'];
                document.getElementById('secondary_dns').value = data['SSID_SECONDARY_DNS'];
            }
        }

        updateWifiFields();

        document.getElementById('mqtt_server').value = data['MQTT_SERVER'];
        document.getElementById('mqtt_username').value = data['MQTT_USERNAME'];
        document.getElementById('mqtt_password').value = data['MQTT_PASSWORD'];
        document.getElementById('mqtt_topic_name').value = data['MQTT_TOPIC_NAME'];
        document.getElementById('mqtt_jsontable').checked = data['MQTT_USE_JSONTABLE'];
        document.getElementById('mqtt_use_onetopic').checked = data['MQTT_USE_ONETOPIC'];

        if(data['MQTT_USE_ONETOPIC'])
        {
            document.getElementById('mqtt_onetopic_name').value = data['MQTT_ONETOPIC_NAME'];
            show('onetopic');
        }

        document.getElementById('mqtt_port').value = data['MQTT_PORT'];
        document.getElementById('pin_enable_config').value = data['PIN_ENABLE_CONFIG'];

        document.getElementById('x10a_enabled').checked = data['X10A_ENABLED'];
        document.getElementById('heating_enabled').checked = data['HEATING_ENABLED'];
        document.getElementById('cooling_enabled').checked = data['COOLING_ENABLED'];
        document.getElementById('sg_enabled').checked = data['SG_ENABLED'];
        document.getElementById('can_enabled').checked = data['CAN_ENABLED'];

        if(data['X10A_ENABLED'])
        {
            let dataX10AConfig = data['X10A_CONFIG'];
            document.getElementById('pin_rx').value = dataX10AConfig['PIN_RX'];
            document.getElementById('pin_tx').value = dataX10AConfig['PIN_TX'];
            document.getElementById('x10a_protocol').value = dataX10AConfig['X10A_PROTOCOL'];
            document.getElementById('frequency').value = dataX10AConfig['FREQUENCY'];

            let webuiSelectionValues = JSON.parse(dataX10AConfig['WEBUI_SELECTION_VALUES']);
            if(document.getElementById('model').querySelector('option[value="' + webuiSelectionValues['model'] + '"]'))
            {
                document.getElementById('model').value = webuiSelectionValues['model'];
                await refreshLanguages();
            }

            if(document.getElementById('language').querySelector('option[value="' + webuiSelectionValues['language'] + '"]'))
            {
                document.getElementById('language').value = webuiSelectionValues['language'];
                await updatePresets();

                document.getElementById('presetParameters').value = webuiSelectionValues['presetParameters'];
            }

            customParametersList = dataX10AConfig['PARAMETERS'];

            show('x10a');
            show('nav-x10a');
        }

        if(data['HEATING_ENABLED'])
        {
            document.getElementById('pin_heating').value = data['PIN_HEATING'];
            show('heating');
        }

        if(data['COOLING_ENABLED'])
        {
            document.getElementById('pin_cooling').value = data['PIN_COOLING'];
            show('cooling');
        }

        if(data['SG_ENABLED'])
        {
            document.getElementById('pin_sg1').value = data['PIN_SG1'];
            document.getElementById('pin_sg2').value = data['PIN_SG2'];
            document.getElementById('sg_relay_trigger').checked = data['SG_RELAY_HIGH_TRIGGER'];
            show('smartgrid');
        }

        let convertedCommandsList = [];
        if(data['CAN_ENABLED'])
        {
            let dataCANConfig = data['CAN_CONFIG'];
            let canBusPrefix = '';
            let canICType = '';
            if(dataCANConfig['CAN_BUS'] == 1)
            {
                canBusPrefix = 'spi_';
                document.getElementById('pin_can_spi_mosi').value = dataCANConfig['SPI']['MOSI'];
                document.getElementById('pin_can_spi_miso').value = dataCANConfig['SPI']['MISO'];
                document.getElementById('pin_can_spi_sck').value = dataCANConfig['SPI']['SCK'];
                document.getElementById('pin_can_spi_cs').value = dataCANConfig['SPI']['CS'];
                document.getElementById('pin_can_spi_int').value = dataCANConfig['SPI']['INT'];
                document.getElementById('pin_can_spi_mhz').value = dataCANConfig['SPI']['MHZ'];
            }
            else if(dataCANConfig['CAN_BUS'] == 2)
            {
                canBusPrefix = 'uart_';
                document.getElementById('pin_can_uart_rx').value = dataCANConfig['UART']['PIN_RX'];
                document.getElementById('pin_can_uart_tx').value = dataCANConfig['UART']['PIN_TX'];
            }
            else if(dataCANConfig['CAN_BUS'] == 3)
            {
                canBusPrefix = 'bt_';
                document.getElementById('pin_can_bt_devicename').value = dataCANConfig['BLUETOOTH']['DEVICENAME'];
                document.getElementById('pin_can_bt_use_pin').checked = dataCANConfig['BLUETOOTH']['USE_PIN'];

                if(dataCANConfig['BLUETOOTH']['USE_PIN'])
                {
                    document.getElementById('pin_can_bt_pin').value = dataCANConfig['BLUETOOTH']['PIN'];
                    show('bt_use_pin');
                }
            }

            if(dataCANConfig['CAN_IC'] == 1)
            {
                canICType = 'mcp2515';
            }
            else if(dataCANConfig['CAN_IC'] == 2)
            {
                canICType = 'elm327';
            }
            else if(dataCANConfig['CAN_IC'] == 3)
            {
                canICType = 'sja1000';
            }

            document.getElementById("can_ic_type").value = canBusPrefix + canICType;
            updateCANConfigDisplay();

            document.getElementById('can_speed_kbps').value = dataCANConfig['CAN_SPEED_KBPS'];
            document.getElementById('can_mqtt_topic_name').value = dataCANConfig['CAN_MQTT_TOPIC_NAME'];
            document.getElementById('can_readonly_enabled').checked = dataCANConfig['CAN_READONLY_ENABLED'];
            document.getElementById('can_sniffing_enabled').checked = dataCANConfig['CAN_SNIFFING_ENABLED'];

            switch(dataCANConfig['CAN_AUTOPOLL_MODE'])
            {
                case 1: // passiv
                    document.getElementById('can_autopoll_mode_passiv').checked = true;
                    break;
                case 2: // auto
                    document.getElementById('can_autopoll_mode_auto').checked = true;
                    document.getElementById('can_autopoll_time').value = dataCANConfig['CAN_AUTOPOLL_TIME'];
                    show('can_autopoll');
                    break;
                default: // dislabed
                    document.getElementById('can_autopoll_mode_disabled').checked = true;
                    break;
            }

            dataCANConfig['COMMANDS'].forEach(command => {
                let hexBytes = [];
                command[2].forEach(intByte => {
                    hexBytes.push(('0'+intByte.toString(16)).substr(-2).toUpperCase());
                });

                const dataArray = {
                    "name":     command[0],
                    "label":    command[1],
                    "command":  hexBytes.join(' '),
                    "id":       command[3].toString(),
                    "divisor":  command[4].toString(),
                    "writable": command[5].toString(),
                    "unit":     command[6],
                    "type":     command[7]
                };

                if(command[8] != null)
                {
                    dataArray["value_code"] = command[7];
                }

                convertedCommandsList.push(dataArray);
            });

            show('can_pins');
            show('nav-can');
        }

        customCommandsList = convertedCommandsList;

        updateParametersTable('selectedParametersTable', customParametersList);
        updateParameters();

        updateCommandsTable('selectedCommandsTable', customCommandsList);
        updateCommands();
    })
    .catch(function(err) {
        alert('Fetching config failed! Message: ' + err);
    });
}

async function selectWifiNetwork(event)
{
    event.preventDefault();

    const ssid = document.getElementById('ssid');
    const ssidSelect = document.getElementById('ssid_select');

    if(ssidSelect.value != '')
    {
        ssid.value = ssidSelect.value;
        ssidSelect.value = '';
    }
}

async function loadWifiNetworks(event)
{
    const ssidSelect = document.getElementById('ssid_select');
    const btnWifiListRefresh = document.getElementById('btnWifiListRefresh');

    if(btnWifiListRefresh.getAttribute('aria-busy') == 'true')
        return;

    fetchWifiNetworksBtnValue = btnWifiListRefresh.text;
    btnWifiListRefresh.text = '';
    btnWifiListRefresh.setAttribute('aria-busy', 'true');

    while (ssidSelect.options.length > 1)
        ssidSelect.remove(1);

    await fetch('/wifi/loadNetworks', {
        method: "GET"
    })
    .then(function(response) {
        if(response.status == 200)
        {
            setTimeout(loadfWifiNetworksFinished, 5000);
        }
        else
        {
            alert('Fetching wifi list failed! Message: ' + response);
        }
    })
    .catch(function(err) {
        alert('Fetching wifi list failed! Message: ' + err);
    });
}

async function loadfWifiNetworksFinished()
{
    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), 3000);

    const ssidSelect = document.getElementById('ssid_select');
    const btnWifiListRefresh = document.getElementById('btnWifiListRefresh');

    await fetch('/', {
        method: "GET",
        signal: controller.signal
    })
    .then(function(responsePing) {
        if(responsePing.status == 200)
        {
            fetch('/wifi/loadFinished', {
                method: "GET"
            })
            .then(function(response) { return response.json(); })
            .then(function(data) {
                for (let key in data) {
                    let option = document.createElement("option");
                    option.text = data[key]["SSID"] + " (Quality:" + data[key]["RSSI"] + ") " + data[key]["EncryptionType"];
                    option.value = data[key]["SSID"];
                    ssidSelect.add(option);
                }

                btnWifiListRefresh.setAttribute('aria-busy', 'false');
                btnWifiListRefresh.text = fetchWifiNetworksBtnValue;
                fetchWifiErrorCounter = 0;
            });
        }
    })
    .catch(function(err) {
        fetchWifiErrorCounter++;
        if(fetchWifiErrorCounter == 5) {
            alert('Fetching of Wifi scanning result failed. Please ensure to reconnect to the Standalone Wifi! Continuing...');
        }
        setTimeout(loadfWifiNetworksFinished, 5000);
    })
    .finally(function() {
        clearTimeout(timeoutId);
    });;
}

async function validateForm()
{
    const standalone_wifi = document.getElementById('standalone_wifi').checked;

    if(!standalone_wifi)
    {
        const ssid = document.getElementById('ssid');
        ssid.setAttribute('aria-invalid', ssid.value == '');

        const ssid_staticip = document.getElementById('ssid_staticip');
        if(ssid_staticip.checked)
        {
            const ssid_ip = document.getElementById('ssid_ip');
            ssid_ip.setAttribute('aria-invalid',  ssid_ip.value == '' || !ValidateIPaddress(ssid_ip.value));

            const ssid_subnet = document.getElementById('ssid_subnet');
            ssid_subnet.setAttribute('aria-invalid',  ssid_subnet.value == '' || !ValidateIPaddress(ssid_subnet.value));

            const ssid_gateway = document.getElementById('ssid_gateway');
            ssid_gateway.setAttribute('aria-invalid',  ssid_gateway.value == '' || !ValidateIPaddress(ssid_gateway.value));

            const primary_dns = document.getElementById('primary_dns');
            primary_dns.setAttribute('aria-invalid',  primary_dns.value != '' && !ValidateIPaddress(primary_dns.value));

            const secondary_dns = document.getElementById('secondary_dns');
            secondary_dns.setAttribute('aria-invalid',  secondary_dns.value != '' && !ValidateIPaddress(secondary_dns.value));
        }
        else
        {
            clearHiddenValidationResult('staticip');
        }
    }
    else
    {
        const ssid = document.getElementById('ssid');
        ssid.removeAttribute('aria-invalid');

        clearHiddenValidationResult('staticip');
    }

    const mqtt_server = document.getElementById('mqtt_server');
    mqtt_server.setAttribute('aria-invalid', mqtt_server.value == '' || !ValidateIPOrHostname(mqtt_server.value));

    const mqtt_username = document.getElementById('mqtt_username');
    mqtt_username.setAttribute('aria-invalid', mqtt_username.value == '');

    const mqtt_topic_name = document.getElementById('mqtt_topic_name');
    mqtt_topic_name.setAttribute('aria-invalid', mqtt_topic_name.value == '' || !ValidateMQTTTopic(mqtt_topic_name.value));

    const mqtt_use_onetopic = document.getElementById('mqtt_use_onetopic');
    if(mqtt_use_onetopic.checked)
    {
        const mqtt_onetopic_name = document.getElementById('mqtt_onetopic_name');
        mqtt_onetopic_name.setAttribute('aria-invalid', mqtt_onetopic_name.value == '' || !ValidateMQTTTopic(mqtt_onetopic_name.value));
    }
    else
    {
        clearHiddenValidationResult("onetopic");
    }

    const mqtt_port = document.getElementById('mqtt_port');
    mqtt_port.setAttribute('aria-invalid', mqtt_port.value == '' || mqtt_port.value < 0 || mqtt_port.value > 65535);

    const pin_enable_config = document.getElementById('pin_enable_config');
    pin_enable_config.setAttribute('aria-invalid', pin_enable_config.value == '');

    const x10a_enabled = document.getElementById('x10a_enabled');
    if(x10a_enabled.checked)
    {
        const pin_rx = document.getElementById('pin_rx');
        pin_rx.setAttribute('aria-invalid', pin_rx.value == '');

        const pin_tx = document.getElementById('pin_tx');
        pin_tx.setAttribute('aria-invalid', pin_tx.value == '');

        const frequency = document.getElementById('frequency');
        frequency.setAttribute('aria-invalid', frequency.value == '');
    }
    else
    {
        clearHiddenValidationResult("x10a");
    }

    const heating_enabled = document.getElementById('heating_enabled');
    if(heating_enabled.checked)
    {
        const pin_heating = document.getElementById('pin_heating');
        pin_heating.setAttribute('aria-invalid', pin_heating.value == '');
    }
    else
    {
        clearHiddenValidationResult("heating");
    }

    const cooling_enabled = document.getElementById('cooling_enabled');
    if(cooling_enabled.checked)
    {
        const pin_cooling = document.getElementById('pin_cooling');
        pin_cooling.setAttribute('aria-invalid', pin_cooling.value == '');
    }
    else
    {
        clearHiddenValidationResult("cooling");
    }

    const sg_enabled = document.getElementById('sg_enabled');
    if(sg_enabled.checked)
    {
        const pin_sg1 = document.getElementById('pin_sg1');
        pin_sg1.setAttribute('aria-invalid', pin_sg1.value == '');

        const pin_sg2 = document.getElementById('pin_sg2');
        pin_sg2.setAttribute('aria-invalid', pin_sg2.value == '');
    }
    else
    {
        clearHiddenValidationResult("smartgrid");
    }

    const can_enabled = document.getElementById('can_enabled');
    if(can_enabled.checked)
    {
        const can_ic_type = document.getElementById('can_ic_type');
        can_ic_type.setAttribute('aria-invalid', can_ic_type.value == '');

        const can_bus_type = can_ic_type.value.split('_')[0];

        clearHiddenValidationResult("can_spi_config");
        clearHiddenValidationResult("can_bt_config");
        clearHiddenValidationResult("can_uart_config");

        if(can_bus_type == 'uart')
        {
            const pin_can_rx = document.getElementById('pin_can_uart_rx');
            pin_can_rx.setAttribute('aria-invalid', pin_can_rx.value == '');

            const pin_can_tx = document.getElementById('pin_can_uart_tx');
            pin_can_tx.setAttribute('aria-invalid', pin_can_tx.value == '');
        }
        else if(can_bus_type == 'spi')
        {
            const pin_can_spi_mosi = document.getElementById('pin_can_spi_mosi');
            pin_can_spi_mosi.setAttribute('aria-invalid', pin_can_spi_mosi.value == '');

            const pin_can_spi_miso = document.getElementById('pin_can_spi_miso');
            pin_can_spi_miso.setAttribute('aria-invalid', pin_can_spi_miso.value == '');

            const pin_can_spi_sck = document.getElementById('pin_can_spi_sck');
            pin_can_spi_sck.setAttribute('aria-invalid', pin_can_spi_sck.value == '');

            const pin_can_spi_cs = document.getElementById('pin_can_spi_cs');
            pin_can_spi_cs.setAttribute('aria-invalid', pin_can_spi_cs.value == '');

            const pin_can_spi_int = document.getElementById('pin_can_spi_int');
            pin_can_spi_int.setAttribute('aria-invalid', pin_can_spi_int.value == '');

            const pin_can_spi_mhz = document.getElementById('pin_can_spi_mhz');
            pin_can_spi_mhz.setAttribute('aria-invalid', pin_can_spi_mhz.value == '');
        }
        else if(can_bus_type == 'bt')
        {
            const pin_can_bt_devicename = document.getElementById('pin_can_bt_devicename');
            pin_can_bt_devicename.setAttribute('aria-invalid', pin_can_bt_devicename.value == '');

            const pin_can_bt_use_pin = document.getElementById('pin_can_bt_use_pin').checked;

            const pin_can_bt_pin = document.getElementById('pin_can_bt_pin');
            pin_can_bt_pin.setAttribute('aria-invalid', pin_can_bt_use_pin && pin_can_bt_pin.value == '');
        }

        const can_speed_kbps = document.getElementById('can_speed_kbps');
        can_speed_kbps.setAttribute('aria-invalid', can_speed_kbps.value == '');

        const can_mqtt_topic_name = document.getElementById('can_mqtt_topic_name');
        can_mqtt_topic_name.setAttribute('aria-invalid', can_mqtt_topic_name.value == '' || !ValidateMQTTTopic(can_mqtt_topic_name.value));

        const can_autopoll_mode = document.querySelector('input[name="can_autopoll_mode"]:checked').value;
        switch(can_autopoll_mode)
        {
            case 0:
            case 1:
                clearHiddenValidationResult("can_autopoll");
                break;
            case 2:
                const can_autopoll_time = document.getElementById('can_autopoll_time');
                can_autopoll_time.setAttribute('aria-invalid', can_autopoll_time.value == '');
                break;
        }

    }
    else
    {
        clearHiddenValidationResult("can_pins");
    }

    const pinSelects = document.querySelectorAll('input[data-pins]');
    pinSelects.forEach((input) => {
        input.setAttribute('aria-invalid', input.value == '' || input.value < 0 || input.value > 255);
    });

    const validationErrorField = document.querySelector('[aria-invalid="true"]');
    if(validationErrorField)
    {
        validationErrorField.focus();
        await sleep(100);
        alert("Please fill in all required fields!");
        return false;
    }

    return true;
}

function definedCommandsToJSON(listToConvert)
{
    let convertedCommandsList = [];
    listToConvert.forEach(command => {
        const dataArray = [
            command["name"],
            command["label"],
            [],
            parseInt(command["id"]),
            parseFloat(command["divisor"]),
            command["writable"] == "true",
            command["unit"],
            command["type"]
        ];

        let hexBytes = command["command"].split(' ');
        hexBytes.forEach(byte => {
            dataArray[2].push(parseInt(byte, 16));
        });

        if(command["value_code"] != null)
        {
            dataArray.push(command["value_code"]);
        }

        convertedCommandsList.push(dataArray);
    });

    return JSON.stringify(convertedCommandsList);
}

async function sendConfigData(event)
{
    event.preventDefault();

    let valid = await validateForm();

    if(!valid)
        return;

    const form = document.getElementById("configForm");
    const formData = new FormData(form);
    formData.append("definedParameters", JSON.stringify(customParametersList));
    formData.append("definedCommands", definedCommandsToJSON(customCommandsList));

    await fetch(form.getAttribute('action'), {
        method: form.getAttribute('method'),
        body: formData
    })
    .then(function(response) {
        if(response.status != 200)
        {
            alert("Error saving config: " + response.data);
            return;
        }

        alert("Config successfully saved! ESP32 will restart now with new config. Reloading page in 3 Seconds");

        setTimeout(() => {
            document.location.reload();
        }, 3000);
    })
    .catch(function(err) {
        alert('Saving config failed! Message: ' + err);
    });
}

async function sendUpdate(event)
{
    const updateFile = document.getElementById('updateFile');
    const file = updateFile.files[0];

    updateFile.setAttribute('aria-invalid', !file);

    if(!file)
        return;

    const reader = new FileReader();
    reader.onload = async function (event) {
        const data = event.target.result;
        if (data) {
            const md5Hash = SparkMD5.ArrayBuffer.hash(data);

            const formData = new FormData();
            formData.append("type", document.querySelector('input[name="updateType"]:checked').value);
            formData.append("MD5", md5Hash);
            formData.append("file", updateFile.files[0], "update.bin");

            const request = new XMLHttpRequest();
            request.addEventListener('load', () => {
                let oatError = "";
                document.getElementById("startUpdate").disabled = false;
                if (request.status === 200) {
                    updateFile.removeAttribute('aria-invalid');
                    updateFile.value = null;

                    if(request.responseText == "INVALID")
                    {
                        oatError = "Update file is invalid or version is lower then current version!";
                    }
                    else
                    {
                        document.getElementById("otaProgress").innerText = "Finished! Reload Config page in 3 seconds";
                        setTimeout(() => {
                            document.location.reload();
                        }, 3000);
                    }
                } else if (request.status !== 500) {
                    oatError = `[HTTP ERROR] ${request.statusText}`;
                } else {
                    oatError = request.responseText;
                }

                if(oatError != "")
                {
                    document.getElementById("otaProgress").innerText = "Error!";
                    document.getElementById("oatErrorDisplay").classList.remove("hidden");
                    document.getElementById("oatErrorDisplay").innerText = oatError;
                }
              });

            request.upload.addEventListener('progress', (e) => {
                document.getElementById("otaProgress").innerText = Math.trunc((e.loaded / e.total) * 100) + "%";
            });

            try {
                document.getElementById("startUpdate").disabled = true;
                document.getElementById("oatErrorDisplay").classList.add("hidden");
                document.getElementById("progressDisplay").classList.remove('hidden');
                document.getElementById("otaProgress").innerText = "0%";

                request.open('post', '/update');
                request.send(formData);
            }
            catch(error)
            {
                document.getElementById("oatErrorDisplay").classList.remove("hidden");
                document.getElementById("oatErrorDisplay").innerText = "Unknown error while upload, check the console for details.";
            }
        }
    }
    reader.readAsArrayBuffer(file);
}

function clearHiddenValidationResult(elementName)
{
    document.getElementById(elementName).querySelectorAll("[aria-invalid]").forEach((el) => el.removeAttribute('aria-invalid'));
    document.getElementById(elementName).querySelectorAll("input[data-pins]").forEach((el) => el.value = el.defaultValue);
}

function sleep(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}

function ValidateIPaddress(ipaddress)
{
    return /^(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$/.test(ipaddress);
}

function ValidateHostname(hostname)
{
    return /^[a-zA-Z][a-zA-Z\d-]{1,22}[a-zA-Z\d]$/.test(hostname);
}

function ValidateIPOrHostname(input)
{
    return !(!ValidateIPaddress(input) && !ValidateHostname(input));
}

function ValidateMQTTTopic(topicName)
{
    return /^(?:(?:[a-zA-Z0-9_-]+)\/)*([a-zA-Z0-9_-]+\/)$/.test(topicName);
}

function show(id, force = undefined)
{
    document.getElementById(id).classList.toggle('hidden', force);
}

async function updatePresets()
{
    const modelFile = document.getElementById('language').value;

    if(modelFile == '')
    {
        selectedModelPresets = [];
        selectedModelParameters = [];
        filteredModelParameters = [];

        const presetParametersSelect = document.getElementById('presetParameters');

        presetParametersSelect.value = '';

        while (presetParametersSelect.options.length > 1)
            presetParametersSelect.remove(1);

        updateParametersTable('parametersTable', selectedModelParameters);
        updateParameters();
        return;
    }

    const formData = new FormData();
    formData.append("modelFile", modelFile);
    await fetch('/loadModel', {
        method: "POST",
        body: formData
    })
    .then(function(response) { return response.json(); })
    .then(function(data){
        selectedModelPresets = data['Presets'];
        selectedModelParameters = data['Parameters'];
        filteredModelParameters = [];

        const presetParametersSelect = document.getElementById('presetParameters');

        while (presetParametersSelect.options.length > 1)
            presetParametersSelect.remove(1);

        for (let key in selectedModelPresets) {
            if (selectedModelPresets.hasOwnProperty(key)) {
                const option = document.createElement("option");
                option.text = key;
                option.value = JSON.stringify(selectedModelPresets[key]);
                presetParametersSelect.add(option);
            }
        }

        const optionAll = document.createElement("option");
        optionAll.text = "All";
        optionAll.value = "all";
        presetParametersSelect.add(optionAll);
    })
    .catch(function(err) {
        alert('Fetching parameter data failed! Message: ' + err);
    });
}

async function updateParameters()
{
    let presetParametersSelect = document.getElementById('presetParameters').value;

    if(presetParametersSelect == '')
    {
        document.getElementById('selectModelParametersSection').classList.add('hidden');
        filteredModelParameters = [];
    }
    else
    {
        document.getElementById('selectModelParametersSection').classList.remove('hidden');
    }

    if(presetParametersSelect == 'all')
    {
        filteredModelParameters = selectedModelParameters;
    }
    else if(presetParametersSelect != '')
    {
        let presetIndexes = JSON.parse(presetParametersSelect);
        filteredModelParameters = selectedModelParameters.filter((el, i) => presetIndexes.some(j => i == j));
    }

    updateParametersTable('parametersTable', filteredModelParameters);
}

async function uploadX10AFile() {

    const parametersFile = document.getElementById('parametersFile');
    const file = parametersFile.files[0];

    parametersFile.setAttribute('aria-invalid', !file);

    if(!file)
        return;

    const formData = new FormData();
    formData.append("file", parametersFile.files[0]);
    await fetch('/upload/X10A', {
        method: "POST",
        body: formData
    })
    .then(function(response) {
        if (response.status == 200) {
            parametersFile.removeAttribute('aria-invalid');
            parametersFile.value = null;
            models = [];
            refreshModels();
        }
    })
    .catch(function(err) {
        alert('File upload failed! Message: ' + err);
    });
}

async function uploadConfig() {

    const parametersFile = document.getElementById('configFile');
    const importStatusDisplay = document.getElementById('importStatusDisplay');
    const file = parametersFile.files[0];

    parametersFile.setAttribute('aria-invalid', !file);

    if(!file)
        return;

    importStatusDisplay.innerText = "Uploading...";

    const formData = new FormData();
    formData.append("file", parametersFile.files[0]);
    await fetch('/upload/config', {
        method: "POST",
        body: formData
    })
    .then(function(response) {
        if (response.status == 200) {
            parametersFile.removeAttribute('aria-invalid');
            parametersFile.value = null;
            importStatusDisplay.innerText = "Config successfully imported! ESP32 will restart now with new config. Reloading page in 3 Seconds";

            setTimeout(() => {
                document.location.reload();
            }, 3000);
        }
    })
    .catch(function(err) {
        importStatusDisplay.innerText = 'Config upload failed! Message: ' + err;
    });
}

function AddParameter(regid, offset, convid, dataSize, dataType, dataName)
{

    for (let i in customParametersList) {
        if(customParametersList[i][0] == regid &&
           customParametersList[i][1] == offset &&
           customParametersList[i][2] == convid &&
           customParametersList[i][3] == dataSize &&
           customParametersList[i][4] == dataType)
        {
            alert("Parameter settings of '" + dataName + "' already exists. Skip adding");
            return false;
        }
    }

    const dataArray = [regid, offset, convid, dataSize, dataType, dataName];

    customParametersList.push(dataArray);
    return true;
}

function addCustomParameter()
{
    const regid = document.getElementById('regid');
    const offset = document.getElementById('offset');
    const convid = document.getElementById('convid');
    const dataSize = document.getElementById('dataSize');
    const dataType = document.getElementById('dataType');
    const dataName = document.getElementById('dataName');

    if( regid.value == '' || isNaN(regid.value) ||
        offset.value == '' || isNaN(offset.value) ||
        convid.value == '' || isNaN(convid.value) ||
        dataSize.value == '' || isNaN(dataSize.value) ||
        dataType.value == '' || isNaN(dataType.value) ||
        dataName.value.trim() == '')
    {
        alert("Please fill in all fields correctly!");
        return false;
    }

    const result = AddParameter(regid.value, offset.value, convid.value, dataSize.value, dataType.value, dataName.value.trim());

    if(!result)
        return;

    regid.value = '';
    offset.value = '';
    convid.value = '';
    dataSize.value = '';
    dataType.value = '';
    dataName.value = '';

    updateParametersTable('selectedParametersTable', customParametersList);
}

function updateParametersTable(tableId, parameters)
{
    const selectedParametersTable = document.getElementById(tableId);

    while (selectedParametersTable.rows.length > 1) {
        selectedParametersTable.deleteRow(1);
    }

    for (let i in parameters) {
        const data = parameters[i];

        const row = selectedParametersTable.insertRow(-1);
        row.setAttribute('data-row-index', i);
        row.addEventListener("click", function(event) {selectRow(tableId, i);});

        const nameCell = row.insertCell(0);

        const regidCell = row.insertCell(1);
        const offsetCell = row.insertCell(2);
        const convidCell = row.insertCell(3);
        const dataSizeCell = row.insertCell(4);
        const dataTypeCell = row.insertCell(5);

        nameCell.appendChild(document.createTextNode(data[5]));
        regidCell.appendChild(document.createTextNode(data[0]));
        offsetCell.appendChild(document.createTextNode(data[1]));
        convidCell.appendChild(document.createTextNode(data[2]));
        dataSizeCell.appendChild(document.createTextNode(data[3]));
        dataTypeCell.appendChild(document.createTextNode(data[4]));

        if(data[6] != null)
        {
            const valueCell = row.insertCell(6);
            valueCell.appendChild(document.createTextNode(data[6]));
        }
    }
}

function selectRow(tableid, value)
{
    const el = document
    .getElementById(tableid)
    .querySelector("[data-row-index='" + value + "']");

    const classes = el.classList;

    if(classes.contains('row-selected'))
        classes.remove('row-selected');
    else
        classes.add('row-selected');
}

function addSelectedPredefinedParameters()
{
    document
    .getElementById('parametersTable')
    .querySelectorAll(".row-selected")
    .forEach(function(e) {
        const id = parseInt(e.getAttribute('data-row-index'));
        const paramToAdd = filteredModelParameters[id];

        AddParameter(paramToAdd[0], paramToAdd[1], paramToAdd[2], paramToAdd[3], paramToAdd[4], paramToAdd[5]);
        e.classList.remove('row-selected');
    });

    updateParametersTable('selectedParametersTable', customParametersList);
}

function addPredefinedParameters()
{
    document
    .getElementById('parametersTable')
    .querySelectorAll("[data-row-index]")
    .forEach(function(e) {
        const id = parseInt(e.getAttribute('data-row-index'));
        const paramToAdd = filteredModelParameters[id];

        AddParameter(paramToAdd[0], paramToAdd[1], paramToAdd[2], paramToAdd[3], paramToAdd[4], paramToAdd[5]);
        e.classList.remove('row-selected');
    });

    updateParametersTable('selectedParametersTable', customParametersList);
}

function removeSelectedCustomParameters()
{
    let counterRun = 0;

    document
    .getElementById('selectedParametersTable')
    .querySelectorAll(".row-selected")
    .forEach(function(e) {
        let id = parseInt(e.getAttribute('data-row-index'));
        id -= counterRun;
        customParametersList = customParametersList.filter(function(value, index){
            let keep = index != id;

            if(!keep)
                counterRun++;

            return keep;
        });
    });

    updateParametersTable('selectedParametersTable', customParametersList);
}

function clearCustomParameters()
{
    customParametersList = [];
    updateParametersTable('selectedParametersTable', customParametersList);
}

async function finishLoadX10AData()
{
    let params;
    if (fetchDataIntervalId == 'selectedParametersTable')
        params = customParametersList;
    else
        params = selectedModelParameters;

    const buttonId = 'load' + fetchDataIntervalId.charAt(0).toUpperCase() + fetchDataIntervalId.slice(1);
    const buttonLoadValues = document.getElementById(buttonId);

    await fetch('/loadValuesResult', {
        method: "GET"
    })
    .then(function(response) {
        if(response.status != 200)
        {
            throw new Error("A!" + response.text);
        }

        return response.json();
    })
    .then(function(data){
        params.forEach((model, index) => {
            model[6] = data[index];
        });

        if (fetchDataIntervalId == 'selectedParametersTable')
        {
            customParametersList = params;
            updateParametersTable(fetchDataIntervalId, customParametersList);
        }
        else
        {
            selectedModelParameters = params;
            updateParametersTable(fetchDataIntervalId, selectedModelParameters);
        }

        clearInterval(fetchDataIntervalHandler);
        buttonLoadValues.removeAttribute('aria-busy');
        buttonLoadValues.toggleAttribute('disabled');
    })
    .catch(function(err) {
        if(err.message.startsWith("A!"))
        {
            console.log(err.message.slice(2));
        }
        else
        {
            clearInterval(fetchDataIntervalHandler);
            buttonLoadValues.removeAttribute('aria-busy');
            buttonLoadValues.toggleAttribute('disabled');
            alert('Fetching param values failed! Message: ' + err);
        }
    });
}

async function beginLoadX10AData(tableId)
{
    let params;
    if (tableId == 'selectedParametersTable')
        params = customParametersList;
    else
        params = selectedModelParameters;

    let valid = await validateForm();

    if(!valid)
        return;

    const form = document.getElementById("configForm");
    const formData = new FormData(form);
    formData.append('definedParameters', JSON.stringify(params));

    const buttonId = 'load' + tableId.charAt(0).toUpperCase() + tableId.slice(1);
    const buttonLoadValues = document.getElementById(buttonId);
    buttonLoadValues.setAttribute('aria-busy', 'true');
    buttonLoadValues.toggleAttribute('disabled');

    await fetch('/loadValues', {
        method: "POST",
        body: formData
    })
    .then(function(response) {
        if(response.status == 200)
        {
            fetchDataIntervalId = tableId;
            fetchDataIntervalHandler = setInterval(finishLoadX10AData, 5000);
        }
        else
        {
            response.text().then(function(text) {
                alert("Begin fetching param values failed! Message: " + text);
            })
            .finally(function() {
                buttonLoadValues.removeAttribute('aria-busy');
                buttonLoadValues.toggleAttribute('disabled');
            });
        }
    })
    .catch(function(err) {
        buttonLoadValues.removeAttribute('aria-busy');
        buttonLoadValues.toggleAttribute('disabled');
        alert('Begin fetching param values failed! Message: ' + err);
    });
}

async function refreshModels()
{
    await fetch('/loadModels', {
        method: "GET"
    })
    .then(function(response) { return response.json(); })
    .then(function(data){
        models = data;

        const modelSelect = document.getElementById('model');

        while (modelSelect.options.length > 1)
            modelSelect.remove(1);

        data.forEach(function(model, i) {
            let option = document.createElement("option");
            option.text = model.Model;
            option.value = i;
            modelSelect.add(option);
        });
    })
    .catch(function(err) {
        alert('Fetching models data failed! Message: ' + err);
    });
}

async function refreshLanguages()
{
    const languageSelect = document.getElementById('language');
    const selectedModel = document.getElementById('model').value;

    while (languageSelect.options.length > 1)
        languageSelect.remove(1);

    if(selectedModel != '')
    {
        const languageFiles = models[selectedModel]["Files"];
        const languageNames = Object.keys(languageFiles);

        languageNames.forEach((language, index) => {
            const option = document.createElement("option");
            option.text = language;
            option.value = languageFiles[language];
            languageSelect.add(option);
        });
    }

    await updatePresets();
}

async function addValueCode(event)
{
    event.preventDefault();

    let container = document.getElementById("canDataValueCode");

    if(container.innerHTML == "&nbsp;")
        container.innerHTML = "";

    const row = (container.querySelectorAll("input").length / 2);

    const leftNode = document.createElement("div");
    const rightNode = document.createElement("div");

    const leftTextbox = document.createElement("input");
    const rightTextbox = document.createElement("input");

    leftTextbox.type = "text";
    leftTextbox.setAttribute("data-value-code-name", "");
    leftTextbox.name = "[valueCodeLeft][" + row + "]";
    rightTextbox.type = "text";
    rightTextbox.setAttribute("data-value-code-content", "");
    rightTextbox.name = "[valueCodeRight][" + row + "]";

    leftNode.appendChild(leftTextbox);
    rightNode.appendChild(rightTextbox);

    const rowNode = document.createElement("div");
    rowNode.classList.add("grid");

    rowNode.appendChild(leftNode);
    rowNode.appendChild(rightNode);

    container.appendChild(rowNode);
}

async function removeValueCode(event)
{
    event.preventDefault();

    let container = document.getElementById("canDataValueCode");

    const rows = (container.querySelectorAll("input").length / 2);

    if(rows == 0)
        return;

    if(rows == 1)
    {
        container.innerHTML = "&nbsp;";
        return;
    }

    container.removeChild(container.lastElementChild);
}

async function uploadCANFile() {

    const canBusFile = document.getElementById('canFileUpload');
    const file = canBusFile.files[0];

    canBusFile.setAttribute('aria-invalid', !file);

    if(!file)
        return;

    const formData = new FormData();
    formData.append("file", canBusFile.files[0]);
    await fetch('/upload/CAN', {
        method: "POST",
        body: formData
    })
    .then(function(response) {
        if (response.status == 200) {
            canBusFile.removeAttribute('aria-invalid');
            canBusFile.value = null;
            canCommands = [];
            refreshCANCommands();
        }
    })
    .catch(function(err) {
        alert('File upload failed! Message: ' + err);
    });
}

async function refreshCANCommands()
{
    await fetch('/loadCommands', {
        method: "GET"
    })
    .then(function(response) { return response.json(); })
    .then(function(data){
        canCommands = data;

        const canModelSelect = document.getElementById('canModel');

        while (canModelSelect.options.length > 1)
            canModelSelect.remove(1);

        data.forEach(function(canFile, i) {
            let option = document.createElement("option");
            option.text = canFile.Model;
            option.value = i;
            canModelSelect.add(option);
        });
    })
    .catch(function(err) {
        alert('Fetching CAN Files data failed! Message: ' + err);
    });
}

async function updateCanLanguages()
{
    const languageSelect = document.getElementById('canLanguage');
    const selectedModel = document.getElementById('canModel').value;

    while (languageSelect.options.length > 1)
        languageSelect.remove(1);

    if(selectedModel != '')
    {
        const languageFiles = canCommands[selectedModel]["Files"];
        const languageNames = Object.keys(languageFiles);

        languageNames.forEach((language, index) => {
            const option = document.createElement("option");
            option.text = language;
            option.value = languageFiles[language];
            languageSelect.add(option);
        });
    }
}

async function updateCanCommands()
{
    const commandsFile = document.getElementById('canLanguage').value;

    if(commandsFile == '')
    {
        modelCommands = [];

        updateCommandsTable('fileCommandsTable', modelCommands);
        updateCommands();
        return;
    }

    const formData = new FormData();
    formData.append("commandFile", commandsFile);
    await fetch('/loadCommand', {
        method: "POST",
        body: formData
    })
    .then(function(response) { return response.json(); })
    .then(function(data){
        modelCommands = data['Commands'];
        updateCommands();
    })
    .catch(function(err) {
        alert('Fetching parameter data failed! Message: ' + err);
    });
}

async function updateCommands()
{
    let canLanguageSelect = document.getElementById('canLanguage').value;

    if(canLanguageSelect == '')
    {
        document.getElementById('selectCanBusCommandSection').classList.add('hidden');
        modelCommands = [];
    }
    else
    {
        document.getElementById('selectCanBusCommandSection').classList.remove('hidden');
    }

    updateCommandsTable('fileCommandsTable', modelCommands);
}

function addSelectedCanCommands()
{
    document
    .getElementById('fileCommandsTable')
    .querySelectorAll(".row-selected")
    .forEach(function(e) {
        const id = parseInt(e.getAttribute('data-row-index'));
        const paramToAdd = modelCommands[id];

        AddCanCommand(paramToAdd["name"], paramToAdd["label"], paramToAdd["command"], paramToAdd["id"], paramToAdd["divisor"], paramToAdd["writable"], paramToAdd["unit"], paramToAdd["type"], (paramToAdd["value_code"]) ?? null);
        e.classList.remove('row-selected');
    });

    updateCommandsTable('selectedCommandsTable', customCommandsList);
}

function addCanCommands()
{
    document
    .getElementById('fileCommandsTable')
    .querySelectorAll("[data-row-index]")
    .forEach(function(e) {
        const id = parseInt(e.getAttribute('data-row-index'));
        const paramToAdd = modelCommands[id];

        AddCanCommand(paramToAdd["name"], paramToAdd["label"], paramToAdd["command"], paramToAdd["id"], paramToAdd["divisor"], paramToAdd["writable"], paramToAdd["unit"], paramToAdd["type"], (paramToAdd["value_code"]) ?? null);
        e.classList.remove('row-selected');
    });

    updateCommandsTable('selectedCommandsTable', customCommandsList);
}

function AddCanCommand(name, label, command, id, divisor, writable, unit, type, valueCode)
{
    for (let i in customCommandsList) {
        if(customCommandsList[i]["command"] == command)
        {
            alert("Command settings of '" + label + "' already exists. Skip adding");
            return false;
        }
    }

    const dataArray = {
        "name": name,
        "label": label,
        "command": command,
        "id": id,
        "divisor": divisor,
        "writable": writable,
        "unit": unit,
        "type": type,
        "value_code": valueCode
    };

    customCommandsList.push(dataArray);
    return true;
}

function addCustomCanCommand(event)
{
    const name = document.getElementById('canDataName');
    const label = document.getElementById('canDataLabel');
    const command = document.getElementById('canDataCommand');
    const id = document.getElementById('canDataID');
    const divisor = document.getElementById('canDataDivisor');
    const writable = document.getElementById('canDataWriteable');
    const unit = document.getElementById('canDataUnit');
    const type = document.getElementById('canDataType');

    if( name.value == '' ||
        name.value.indexOf(' ') >= 0 ||
        label.value == '' ||
        command.value == '' ||
        isNaN(regid.value) ||
        isNaN(offset.value))
    {
        alert("Please fill in all fields correctly!");
        return false;
    }

    let commandValid = true;
    var hexReg =/^[0-9a-f]{2}$/i;

    const commandHexBytes = command.value.trim().split(' ');

    if(commandHexBytes.length != 7)
    {
        commandValid = false;
    }

    for(let i = 0; i < 7; i++)
    {
        if(!hexReg.test(commandHexBytes[i]))
        {
            commandValid = false;
            break;
        }
    }

    if(!commandValid)
    {
        alert("Given command is invalid. Please enter command with 7 bytes in HEX-Format with leading zeros with spaces between bytes! Ex: 00 01 02 AB 0c AF f3");
        return false;
    }

    const listValueCodeNameFields = document.querySelectorAll("input[data-value-code-name]");
    const listValueCodeContentFields = document.querySelectorAll("input[data-value-code-content]");

    let valueCodes = {};

    for (let i = 0; i < listValueCodeNameFields.length; i++) {
        let valueCodeName = listValueCodeNameFields[i].value;
        let valueCodeContent = listValueCodeContentFields[i].value;

        if(valueCodeName == '' || valueCodeContent == '')
        {
            alert("Please fill in all value code fields correctly!");
            return false;
        }

        valueCodes[valueCodeName] = valueCodeContent;
    }

    const result = AddCanCommand(name.value, label.value, command.value, id.value, divisor.value, writable.checked ? "true" : "false", unit.value, type.value, Object.keys(valueCodes).length == 0 ? null : valueCodes);

    if(!result)
        return;

    name.value = '';
    label.value = '';
    command.value = '';

    const rows = document.getElementById("canDataValueCode").querySelectorAll("input[data-value-code-name]").length;

    for(let i = 0; i < rows; i++)
        removeValueCode(event);

    updateCommandsTable('selectedCommandsTable', customCommandsList);
}

function removeSelectedCommands()
{
    let counterRun = 0;

    document
    .getElementById('selectedCommandsTable')
    .querySelectorAll(".row-selected")
    .forEach(function(e) {
        let id = parseInt(e.getAttribute('data-row-index'));
        id -= counterRun;
        customCommandsList = customCommandsList.filter(function(value, index){
            let keep = index != id;

            if(!keep)
                counterRun++;

            return keep;
        });
    });

    updateCommandsTable('selectedCommandsTable', customCommandsList);
}

function clearCommands()
{
    customCommandsList = [];
    updateCommandsTable('selectedCommandsTable', customCommandsList);
}

function updateCommandsTable(tableId, commands)
{
    const selectedCommandsTable = document.getElementById(tableId);

    while (selectedCommandsTable.rows.length > 1) {
        selectedCommandsTable.deleteRow(1);
    }

    for (let i in commands) {
        const data = commands[i];

        const row = selectedCommandsTable.insertRow(-1);
        row.setAttribute('data-row-index', i);
        row.addEventListener("click", function(event) {selectRow(tableId, i);});

        const labelCell = row.insertCell(0);
        const helpCell = row.insertCell(1);
        const commandCell = row.insertCell(2);
        const idCell = row.insertCell(3);
        const divisorCell = row.insertCell(4);
        const writableCell = row.insertCell(5);
        const unitCell = row.insertCell(6);
        const typeCell = row.insertCell(7);
        const nameCell = row.insertCell(8);
        const valueCodeCell = row.insertCell(9);

        labelCell.appendChild(document.createTextNode(data["label"]));

        if(data["description"] != null)
        {
            const helpLink = document.createElement("a");
            helpLink.href = "javascript:alert('" + data["description"] + "')";
            helpLink.text = "?";
            helpCell.appendChild(helpLink);
        }

        commandCell.appendChild(document.createTextNode(data["command"]));
        idCell.appendChild(document.createTextNode(data["id"]));
        divisorCell.appendChild(document.createTextNode(data["divisor"]));
        writableCell.appendChild(document.createTextNode(data["writable"]));
        unitCell.appendChild(document.createTextNode(data["unit"]));
        typeCell.appendChild(document.createTextNode(data["type"]));
        nameCell.appendChild(document.createTextNode(data["name"]));

        if(data["value_code"] == null)
            valueCodeCell.appendChild(document.createTextNode(""));
        else
            valueCodeCell.appendChild(document.createTextNode(JSON.stringify(data["value_code"])));

        if(data["value"] != null)
        {
            const valueCell = row.insertCell(10);
            valueCell.appendChild(document.createTextNode(data["value"]));
        }
    }
}

function updateCANConfigDisplay()
{
    const icType = document.getElementById('can_ic_type').value;

    show('can_spi_config', !icType.startsWith('spi_'));
    show('can_uart_config', !icType.startsWith('uart_'));
    show('can_bt_config', !icType.startsWith('bt_'));
}

async function beginLoadCANData(tableId)
{
    let params;
    if (tableId == 'selectedCommandsTable')
        params = customCommandsList;
    else
        params = modelCommands;

    let valid = await validateForm();

    if(!valid)
        return;

    const form = document.getElementById("configForm");
    const formData = new FormData(form);
    formData.append('definedCommands', definedCommandsToJSON(params));

    const buttonId = 'load' + tableId.charAt(0).toUpperCase() + tableId.slice(1);
    const buttonLoadValues = document.getElementById(buttonId);
    buttonLoadValues.setAttribute('aria-busy', 'true');
    buttonLoadValues.toggleAttribute('disabled');

    await fetch('/can/loadValues', {
        method: "POST",
        body: formData
    })
    .then(function(response) {
        if(response.status == 200)
        {
            fetchDataIntervalId = tableId;
            fetchDataIntervalHandler = setInterval(finishLoadCANData, 5000);
        }
        else
        {
            response.text().then(function(text) {
                alert("Begin fetching command values failed! Message: " + text);
            })
            .finally(function() {
                buttonLoadValues.removeAttribute('aria-busy');
                buttonLoadValues.toggleAttribute('disabled');
            });
        }
    })
    .catch(function(err) {
        buttonLoadValues.removeAttribute('aria-busy');
        buttonLoadValues.toggleAttribute('disabled');
        alert('Begin fetching commands values failed! Message: ' + err);
    });
}

async function finishLoadCANData()
{
    let params;
    if (fetchDataIntervalId == 'selectedCommandsTable')
        params = customCommandsList;
    else
        params = modelCommands;

    const buttonId = 'load' + fetchDataIntervalId.charAt(0).toUpperCase() + fetchDataIntervalId.slice(1);
    const buttonLoadValues = document.getElementById(buttonId);

    await fetch('/can/loadValuesResult', {
        method: "GET"
    })
    .then(function(response) {
        if(response.status != 200)
        {
            clearInterval(fetchDataIntervalHandler);
            throw Error("A!" + response.text);
        }

        return response.json();
    })
    .then(function(data){
        params.forEach((model, index) => {
            model["value"] = data[index];
        });

        if (fetchDataIntervalId == 'selectedCommandsTable')
        {
            customCommandsList = params;
            updateCommandsTable(fetchDataIntervalId, customCommandsList);
        }
        else
        {
            modelCommands = params;
            updateCommandsTable(fetchDataIntervalId, modelCommands);
        }

        clearInterval(fetchDataIntervalHandler);
        buttonLoadValues.removeAttribute('aria-busy');
        buttonLoadValues.toggleAttribute('disabled');
    })
    .catch(function(err) {
        if(err.message.startsWith("A!"))
        {
            console.log(err.message.slice(2));
        }
        else
        {
            clearInterval(fetchDataIntervalHandler);
            buttonLoadValues.removeAttribute('aria-busy');
            buttonLoadValues.toggleAttribute('disabled');
            alert('Fetching commands values failed! Message: ' + err);
        }
    });
}