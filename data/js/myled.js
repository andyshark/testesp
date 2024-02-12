var led_status = 0;

function led_state() {
  var request = new XMLHttpRequest();
  request.open("GET", "/led_status", true);
  request.onload = function () {
    if (request.readyState == 4 && request.status == 200) {
      var response = request.responseText;
      led_status = Number.parseInt(response);
      if (led_status == 0) document.getElementById("led_button").classList.add("led_off");
      else document.getElementById("led_button").classList.add("led_on");
    }
  };
  request.send();
}

function led_inverse() {
  var request = new XMLHttpRequest();
  request.open("GET", "/led_switch", false);
  request.send();
  if (request.readyState == 4 && request.status == 200) {
    var response = request.responseText;
    if (response == "0") {
      document.getElementById("led_button").classList.remove("led_on");
      document.getElementById("led_button").classList.add("led_off");
      led_status = 0;
    } else {
      document.getElementById("led_button").classList.remove("led_off");
      document.getElementById("led_button").classList.add("led_on");
      led_status = 1;
    }
  }
}
document.addEventListener("DOMContentLoaded", led_state);
document.getElementById("led_button").addEventListener("click", led_inverse);
