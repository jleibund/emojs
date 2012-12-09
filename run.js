var m = require('./build/Release/emojs');

hi = new m.NodeEPOCDriver();

var count = 0;

hi.connect(function(e){
   console.log('time:', e.time,' action: ', e.cognitivAction, ' power: '+ e.cognitivPower);
   if (count > 5)
        hi.disconnect();
   count++;
});

