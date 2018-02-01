// This example creates a NMEA2000 device on the can bus.
// The device gets an address on the bus, but does not send anything.

var NMEA2000 = require('./index');
var can = require('socketcan');
var channel = can.createRawChannel("can0", true);

channel.start();

var nmea2000 = new NMEA2000.NMEA2000(
  [
    {
      // product:
        serialCode: "112234",
        productCode: 100,
        model:"Simple battery monitor",
        softwareVersion: "node-nmea2000 0.0.1",
        modelVersion: "node-nmea2000 0.0.1",
        loadEquivalency : 0xff,
        nmea2000Version: 0xffff,
        certificationLevel: 0xff,
      // device:
        uniqueNumber: 112234,
        manufacturerCode: 2040,
        deviceFunction: 130,
        deviceClass: 75,
        source: 42
    }
  ]);

nmea2000.setSendCanFrame(function(id, data) {
  var msg = { id: id, data: data, ext: true };
  var r = channel.send(msg);
  console.log(" -> " + id.toString(16) + '#' + data.toString('hex')
              + ' (' + r + ', ' + msg.tssec + ')');
  return r > 0;
});

channel.addListener("onMessage", function(msg) {
  console.log(" <- " + msg.id.toString(16) + '#' + msg.data.toString('hex'));
  nmea2000.pushCanFrame(msg);
});

setTimeout(function() { nmea2000.open(); }, 50);
setInterval(function() { nmea2000.parseMessages(); }, 100);
