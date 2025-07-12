///////////////////////////////////////////////////////////////////////////////
// datacake_uplink_decoder.js
// 
// Datacake MQTT decoder for Growatt Photovoltaic Inverter Modbus Data
// from growatt2radio
//
// This script decodes MQTT messages to Datacake's "data fields".
//
// - In the Datacake dashboard, copy the script to
//   <device>->Configuration->Product & Hardware->MQTT Configuration
// - Replace the topics and device_id with your own
//
// created: 07/2025
//
//
// MIT License
//
// Copyright (c) 2025 Matthias Prinke
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
//
// History:
// 20250712 Created
//
///////////////////////////////////////////////////////////////////////////////
function Decoder(topic, payload) {
    
    if (topic == "PV-Inverter-Receiver-789ABC/data") {
        payload = JSON.parse(payload);
        
        // Serial Number or Device ID
        var device_id = "389eed90-4aa0-4dd0-98de-e9f117553a44"
        
        // Data fields
        var modbus = payload.modbus;
        var status = payload.status;
        var faultcode = payload.faultcode;
        var outputpower = payload.outputpower;
        var energytoday = payload.energytoday;
        var energytotal = payload.energytotal;
        var totalworktime = payload.totalworktime;
        var gridvoltage = payload.gridvoltage
        var gridfrequency = payload.gridfrequency;
        var tempinverter = payload.tempinverter;
        
        
        return [
            {
                device: device_id,
                field: "MODBUS",
                value: modbus
            },
            {
                device: device_id,
                field: "STATUS",
                value: status
            },
            {
                device: device_id,
                field: "FAULTCODE",
                value: faultcode
            },
            {
                device: device_id,
                field: "OUTPUTPOWER",
                value: outputpower
            },
            {
                device: device_id,
                field: "ENERGYTODAY",
                value: energytoday
            },
            {
                device: device_id,
                field: "ENERGYTOTAL",
                value: energytotal
            },
            {
                device: device_id,
                field: "TOTALWORKTIME",
                value: totalworktime
            },
            {
                device: device_id,
                field: "GRIDVOLTAGE",
                value: gridvoltage
            },
            {
                device: device_id,
                field: "GRIDFREQUENCY",
                value: gridfrequency
            },
            {
                device: device_id,
                field: "TEMPINVERTER",
                value: tempinverter
            }
        ];
    } else if (topic == "PV-Inverter-Receiver-789ABC/rssi") {
        return [
            {
                device: device_id,
                field: "RSSI",
                value: payload
            }
        ];
    }
}
