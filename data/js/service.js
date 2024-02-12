var acc = document.getElementsByClassName("accordionn");
var i;

for (i = 0; i < acc.length; i++) {
  acc[i].addEventListener("click", function() {
    this.classList.toggle("active");
    var panel = this.nextElementSibling;
    if (panel.style.maxHeight){
      panel.style.maxHeight = null;
    } else {
      panel.style.maxHeight = panel.scrollHeight + "px";
    }
  });
}

function send_request(submit, server) {
  request = new XMLHttpRequest(); 
  request.open("GET", server, true);
  request.send();
  save_status(submit, request);
}

function save_status(submit, request) {
  var response;
  var num;
  request.onreadystatechange = function () {
    if (request.readyState != 4) return;
    if (request.responseText != '') { 
      response = request.responseText.split(";");
      if(response[0] == "WS") {
        document.getElementById("setrxtoubews").value = response[1];
        document.getElementById("settxtoubews").value = response[2];
        document.getElementById("setaddrws").value = response[3];
        document.getElementById("setchanalws").value = response[4];
        document.getElementById("setmasterws").value = response[5];
        document.getElementById("settsleepws").value = response[6];
        document.getElementById("setcorrtws").value = response[7];
        document.getElementById("setcorradc1ws").value = response[8];
        document.getElementById("setcorradc2ws").value = response[9];
        num =  response[10]; 
       
        if(num & 1) {
          document.getElementById("adcws").checked = true;
        } else {  
          document.getElementById("adcws").checked = false;
        }
        if(num & 2) {
          document.getElementById("digitalws").checked = true;
        } else { 
          document.getElementById("digitalws").checked = false;
        }       
        submit.value = 'Готово'; 
      }
      if(response[0] == "WM") {
        document.getElementById("setrxtoubewm1").value = response[1];
        document.getElementById("settxtoubewm1").value = response[2];
        document.getElementById("setaddrwm").value = response[3];
        document.getElementById("setchanalwm").value = response[4];
        document.getElementById("setmasterwm").value = response[5];
        document.getElementById("settsleepwm").value = response[6];
        document.getElementById("setcorrtwm").value = response[7];
        document.getElementById("setcorradc1wm").value = response[8];
        document.getElementById("setcorradc2wm").value = response[9];
        num =  response[10];     
        if(num & 1) {
          document.getElementById("adcwm").checked = true;
        } else {  
          document.getElementById("adcwm").checked = false;
        }
        if(num & 2) {
          document.getElementById("digitalwm").checked = true;
        } else { 
          document.getElementById("digitalwm").checked = false;
        } 
        document.getElementById("setrxtoubewm2").value = response[11];
        document.getElementById("settxtoubewm2").value = response[12];
        document.getElementById("setaslvwm").value = response[13];
        document.getElementById("setnslvwm").value = response[14];
        document.getElementById("settimeoutslvwm").value = response[15];
        document.getElementById("settimeslvwm").value = response[16];
        submit.value = 'Готово'; 
      }
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

function search_watermodule(submit) {
  server = "/getWM?rx=" + document.getElementById('rxtoubewm').value + "&tx=" + document.getElementById('txtoubewm').value + "&addr=" + document.getElementById('searchaddrwm').value + "&ch=" + document.getElementById('searcchanalwm').value;
  document.getElementById('setrxtoubewm1').value ='';
  document.getElementById('settxtoubewm1').value = '';
  document.getElementById('setrxtoubewm2').value ='';
  document.getElementById('settxtoubewm2').value = '';
  document.getElementById('setaddrwm').value ='';
  document.getElementById('setchanalwm').value = ''; 
  document.getElementById('setmasterwm').value = '';
  document.getElementById('settsleepwm').value = '';
  document.getElementById('setcorrtwm').value = '';
  document.getElementById('setcorradc1wm').value = ''; 
  document.getElementById('setcorradc2wm').value = '';
  document.getElementById('setaslvwm').value = '';
  document.getElementById('setnslvwm').value = ''; 
  document.getElementById('settimeslvwm').value = '';
  document.getElementById("adcwm").checked = false;
  document.getElementById("digitalwm").checked = false;
  send_request(submit, server);
}

function search_watersensor(submit) {
  server = "/getWS?rx=" + document.getElementById('rxtoubews').value + "&tx=" + document.getElementById('txtoubews').value + "&addr=" + document.getElementById('searchaddrws').value + "&ch=" + document.getElementById('searcchanalws').value;
  document.getElementById('setrxtoubews').value ='';
  document.getElementById('settxtoubews').value = '';
  document.getElementById('setaddrws').value ='';
  document.getElementById('setchanalws').value = ''; 
  document.getElementById('setmasterws').value = '';
  document.getElementById('settsleepws').value = '';
  document.getElementById('setcorrtws').value = '';
  document.getElementById('setcorradc1ws').value = ''; 
  document.getElementById('setcorradc2ws').value = '';
  document.getElementById("adcws").checked = false;
  document.getElementById("digitalws").checked = false;
  send_request(submit, server);
}

function store_watersensor(submit, texts) { 
  if (confirm(texts)) {
    var num = 0;
    if(document.getElementById("adcws").checked) {
      num |= 1;
    }
    if(document.getElementById("digitalws").checked) {
      num |= 2;
    }  
    server = "/setWS?rx=" + document.getElementById('rxtoubews').value + "&tx=" + document.getElementById('txtoubews').value + "&addr=" + document.getElementById('searchaddrws').value + "&ch=" + document.getElementById('searcchanalws').value +
    "&srx=" + document.getElementById('setrxtoubews').value + "&stx=" + document.getElementById('settxtoubews').value + "&saddr=" + document.getElementById('setaddrws').value + "&sch=" + document.getElementById('setchanalws').value  + 
    "&ma=" + document.getElementById('setmasterws').value + "&slp=" + document.getElementById('settsleepws').value + "&cor=" + document.getElementById('setcorrtws').value + "&adc1=" + document.getElementById('setcorradc1ws').value + 
    "&adc2=" + document.getElementById('setcorradc2ws').value + "&cfg=" + String(num);
    send_request(submit, server); 
    store_watersensoreepromws(submit, texts);
    return true;
  } else {
    return false;
  }
}

function store_watersensoreepromws(submit, texts) {
  server = "storeWS?rx=" + document.getElementById('rxtoubews').value + "&tx=" + document.getElementById('txtoubews').value + 
           "&addr=" + document.getElementById('searchaddrws').value + "&ch=" + document.getElementById('searcchanalws').value;
  send_request(submit, server);
}

function store_watermodule(submit, texts) { 
  if (confirm(texts)) {
    var num = 0;
    if(document.getElementById("adcwm").checked) {
      num |= 1;
    }
    if(document.getElementById("digitalwm").checked) {
      num |= 2;
    }  
    server = "/setWM?rx=" + document.getElementById('rxtoubewm').value + "&tx=" + document.getElementById('txtoubewm').value + "&addr=" + 
    document.getElementById('searchaddrwm').value + "&ch=" + document.getElementById('searcchanalwm').value +
    "&srx1=" + document.getElementById('setrxtoubewm1').value + "&stx1=" + document.getElementById('settxtoubewm1').value +
    "&srx2=" + document.getElementById('setrxtoubewm2').value + "&stx2=" + document.getElementById('settxtoubewm2').value +  
    "&saddr=" + document.getElementById('setaddrwm').value + "&sch=" + document.getElementById('setchanalwm').value  + 
    "&ma=" + document.getElementById('setmasterwm').value + "&slp=" + document.getElementById('settsleepwm').value + "&cor=" + 
    document.getElementById('setcorrtwm').value + "&adc1=" + document.getElementById('setcorradc1wm').value + 
    "&adc2=" + document.getElementById('setcorradc2wm').value + "&cfg=" + String(num) + "&aslv1=" + document.getElementById('setaslvwm').value +
    "&nslv=" + document.getElementById('setnslvwm').value + "&toutslv=" + document.getElementById('settimeoutslvwm').value +
    "&topen=" + document.getElementById('settimeslvwm').value; 
    send_request(submit, server);
    store_watersensoreepromwm(submit, texts);
    return true;
  } else {
    return false;
  }
}

function store_watersensoreepromwm(submit, texts) {
  server = "storeWM?rx=" + document.getElementById('rxtoubewm').value + "&tx=" + document.getElementById('txtoubewm').value + 
           "&addr=" + document.getElementById('searchaddrwm').value + "&ch=" + document.getElementById('searcchanalwm').value;
  send_request(submit, server);
}