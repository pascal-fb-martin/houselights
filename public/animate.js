// Animate.js: change the properties of HTML/SVG elements based on
// the current status of points returned by the server.

var RootUrl = "";

var KnownState = new Object();
var ChangeToReverse = new Object();

ChangeToReverse["on"] = "off";
ChangeToReverse["off"] = "on";

function lightsUpdateStatus (response) {

    var plugs = response.lights.plugs;
    for (var i = 0; i < plugs.length; i++) {
        var name = plugs[i].name;
        var tag = name.replace (/ /g,'_')
        var state = plugs[i].state;
        if (KnownState[name] && (KnownState[name] === state)) continue;
        var symbol = document.getElementById (tag);
        if (symbol) symbol.setAttribute ("class", state);
        KnownState[name] = state;
    }
}

function lightsControlRequest (id, state) {
   if (!state) return;
   var command = new XMLHttpRequest();
   command.open
      ("GET", RootUrl+"/set?device="+id+"&state="+state+"&cause=MANUAL");
   command.onreadystatechange = function () {
      if (command.readyState === 4 && command.status === 200) {
          lightsUpdateStatus (JSON.parse(command.responseText));
      }
   }
   command.send(null);
}

function controlClick (id) {
    if (KnownState[id])
       lightsControlRequest (encodeURIComponent(id), ChangeToReverse[KnownState[id]]);
}

function lightsStatus () {
    var command = new XMLHttpRequest();
    command.open("GET", RootUrl+"/status");
    command.onreadystatechange = function () {
        if (command.readyState === 4 && command.status === 200) {
            var response = JSON.parse(command.responseText);
            lightsUpdateStatus (response);
        }
    }
    command.send(null);
}

function animateStart (path) {
   RootUrl = path;
   lightsStatus();
   setInterval (lightsStatus, 1000);
}

