var m = require('./build/Release/emojs');

hi = new m.NodeEPOCDriver();

var count = 0;

hi.connect(function(arg){
           console.log(arg);
           if (count > 5)
                hi.disconnect();
           count++;
           });

