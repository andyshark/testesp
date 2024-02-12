var set_real_time;
var xmlHttp = createXmlHttpObject();

function createXmlHttpObject() {
  if (window.XMLHttpRequest) {
    xmlHttp = new XMLHttpRequest();
  } else {
    xmlHttp = new ActiveXObject('Microsoft.XMLHTTP');
  }
  return xmlHttp;
}

function load() {
  get_ssid();
  get_ssidAP();
  process();
}

function process() {
  if (xmlHttp.readyState == 0 || xmlHttp.readyState == 4) {
    if (!set_real_time) {
      xmlHttp.open('PUT', 'xml', true);
      xmlHttp.onreadystatechange = handleServerResponse;
      xmlHttp.send(null);
    }
  }
  setTimeout('process()', 1000);
}

function handleServerResponse() {
  if (xmlHttp.readyState == 4 && xmlHttp.status == 200) {
    xmlResponse = xmlHttp.responseXML;
    xmldoc = xmlResponse.getElementsByTagName('time');
    message = xmldoc[0].firstChild.nodeValue;
    document.getElementById('time').value = message;
  }
}

function set_time(submit) {
  server = "/setTime?time=" + document.getElementById('time').value;
  set_real_time = false;
  send_request(submit, server);
}

function edit_time(submit) {
  set_real_time = true;
}

function time_zone(submit) {
  server = "/TimeZone?timezone=" + document.getElementById('timezone').value;
  send_request(submit, server);
}

function set_time_zone(submit) {
  var set_date = new Date();
  var gmtHours = -set_date.getTimezoneOffset() / 60;
  document.getElementById('timezone').value = gmtHours;
  server = "/TimeZone?timezone=" + gmtHours;
  send_request(submit, server);
}

function load_time(submit) {
  server = "/Time";
  send_request(submit, server);
  set_real_time = false;
}

function send_request(submit, server) {
  request = new XMLHttpRequest();
  request.open("GET", server, true);
  request.send();
  save_status(submit, request);
}

function save_status(submit, request) {
  request.onreadystatechange = function () {
    if (request.readyState != 4) return;
    if (request.responseText != '') {
      submit.value = request.responseText;
    }
    setTimeout(function () {
      submit_visabled(true);
    }, 1000);
  }
  submit.value = 'Подождите...';
  submit_visabled(false);
}

function submit_visabled(request) {
  document.getElementById('infomsg').hidden = request;
}

function set_ssid(submit) {
  if (document.getElementById('ssid').value.length > 18) {
    alert("Слишком длинное имя сети");
  } else {
    if (document.getElementById('password').value.length > 18) {
      alert("Слишком длинный пароль");
    } else {
      server = "/ssid?ssid=" + document.getElementById('ssid').value + "&password=" + encodeURIComponent(document.getElementById('password').value);
      send_request(submit, server);
      alert("Измененя вступят в силу после сохранения и перезагрузки. Пожалуйта сохраните все и перезагрузите устройство.");
    }
  }
}

function get_ssid() {
  var request = new XMLHttpRequest();
  var response;
  request.open("GET", "/Gssid", true);
  request.onload = function () {
    if (request.readyState == 4 && request.status == 200) {
      response = request.responseText.split(":");
    }
    document.getElementById("ssid").value = response[0];
    document.getElementById("password").value = response[1];
    document.getElementById('time').value = "message";
  }
  request.send();
}

function set_ssid_ap(submit) {
  if (document.getElementById('ssidAp').value.length > 18) {
    alert("Слишком длинное имя сети");
  } else {
    if (document.getElementById('passwordAp'), value.length > 18) {
      alert("Слишком длинный пароль");
    } else {
      server = "/ssidap?ssidAP=" + document.getElementById('ssidAp').value + "&passwordAP=" + encodeURIComponent(document.getElementById('passwordAp').value);
      send_request(submit, server);
      alert("Измененя вступят в силу после сохранения и перезагрузки. Пожалуйта сохраните все и перезагрузите устройство.");
    }
  }
}

function get_ssidAP() {
  var request = new XMLHttpRequest();
  var response;
  request.open("GET", "/GssidAP", true);
  request.onload = function () {
    if (request.readyState == 4 && request.status == 200) {
      response = request.responseText.split(":");
    }
    document.getElementById("ssidAp").value = response[0];
    document.getElementById("passwordAp").value = response[1];
  }
  request.send();
}

function store_all(submit) {
  server = "/store_all?device=ok";
  send_request(submit, server);
  alert("Измененя вступят в силу после сохранения и перезагрузки. Пожалуйта сохраните все и перезагрузите устройство!");
}

function restart(submit, texts) {
  if (confirm(texts)) {
    server = "/restart?device=ok";
    send_request(submit, server);
    return true;
  } else {
    return false;
  }
}
