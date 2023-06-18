var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
window.addEventListener('load', onload);

function onload(event) {
    initWebSocket();
}

function getValues(){
    websocket.send("getValues");
}

function initWebSocket() {
    console.log('Trying to open a WebSocket connection…');
    websocket = new WebSocket(gateway);
    websocket.onopen = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
}

function onOpen(event) {
    console.log('Connection opened');
    getValues();
}

function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
}

function onMessage(event) {
    console.log(event.data);
    var myObj = JSON.parse(event.data);

    document.getElementById("temperature").innerHTML = myObj["temperature"] + " °C";
    document.getElementById("tilt").innerHTML = myObj["tilt"];
    document.getElementById("speed").innerHTML = myObj["speed"] + " m/s";
    document.getElementById("distance").innerHTML = myObj["distance"] + " m";
}
