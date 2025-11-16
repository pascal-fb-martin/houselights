// Animate.js: change the properties of HTML/SVG elements based on
// the current status of points returned by the server.

var UpdateUrl = "/status";

var KnownState = new Object();

function lightsUpdateStatus (response) {

    var plugs = response.lights.plugs;
    for (var i = 0; i < plugs.length; i++) {
        var tag = plugs[i].name.replace (/ /g,'_')
        var state = plugs[i].state;
        if (KnownState[tag] && (KnownState[tag] === state)) continue;
        var symbol = document.getElementById (tag);
        if (symbol) symbol.setAttribute ("class", state);
        KnownState[tag] = state;
    }
}

function lightsStatus () {
    var command = new XMLHttpRequest();
    command.open("GET", UpdateUrl);
    command.onreadystatechange = function () {
        if (command.readyState === 4 && command.status === 200) {
            var response = JSON.parse(command.responseText);
            lightsUpdateStatus (response);
        }
    }
    command.send(null);
}

function animateStart (path) {
   UpdateUrl = path + '/status';
   lightsStatus();
   setInterval (lightsStatus, 1000);
}

