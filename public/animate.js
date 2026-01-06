// Animate.js: change the properties of HTML/SVG elements based on
// the current status of points returned by the server.

var LightsLatestStatus = 0;

var RootUrl = "";

var KnownState = new Object();
var ChangeToReverse = new Object();

// These are the light states that can be controlled.
ChangeToReverse["alert"] = "clear";
ChangeToReverse["on"] = "off";
ChangeToReverse["off"] = "on";

function lightsIsControllable (mode) {
   if (!mode) return 1; // Output is the default.
   if (mode == "output") return 1;
   if (mode == "out") return 1;
   return 0;
}

function lightsUpdateStatus (response) {

    if (response.lights.latest) LightsLatestStatus = response.lights.latest;

    var plugs = response.lights.plugs;
    for (var i = 0; i < plugs.length; i++) {
        var name = plugs[i].name;
        var tag = name.replace (/ /g,'_')
        var state = plugs[i].state;
        if (KnownState[name] && (KnownState[name] === state)) continue;
        var symbol = document.getElementById (tag);
        if (symbol) {
           symbol.setAttribute ("class", state);
           if ((!KnownState[name]) && lightsIsControllable(plugs[i].mode))
              symbol.setAttribute ("onclick", "lightsControlClick('"+name+"')");
        }
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

function lightsControlClick (id) {
    if (KnownState[id])
       lightsControlRequest (encodeURIComponent(id), ChangeToReverse[KnownState[id]]);
}

function lightsStatus () {
    var url = RootUrl+"/status";
    if (LightsLatestStatus) url += "?known=" + LightsLatestStatus;
    var command = new XMLHttpRequest();
    command.open("GET", url);
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

