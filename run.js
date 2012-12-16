var m = require('./build/Release/emojs'), fs = require('fs');

hi = new m.NodeEPOCDriver();

var count = 0;

//fs.readFile('/Users/jpleibundguth/Library/Application Support/Emotiv/Profiles/jleibund.emu', function (err, data) {
//    if (err) throw err;
//    console.log(data);
//});

hi.connect('/Users/jpleibundguth/Library/Application Support/Emotiv/Profiles/jleibund.emu',function(e){
    if (e.blink) console.log('Blink');
    if (e.lookLeft) console.log('LookLeft');
    if (e.lookRight) console.log('LookRight');
    if (e.winkLeft) console.log('WinkLeft');
    if (e.winkRight) console.log('WinkRight');
    if (e.cognitivAction >1) console.log('Cog: ',e.cognitivAction, ' Power='+ e.cognitivPower);
//   console.log('time:', e.time,' action: ', e.cognitivAction, ' power: ', e.cognitivPower,' blink: ',e.blink,' lookLeft: ', e.lookLeft, ' lookRight: ', e.lookRight);
});


//   if (count > 5)
//        hi.disconnect();
//   count++;

