var relay_status = 0;

function relay_state() {
  var request = new XMLHttpRequest();
  request.open("GET", "/relay_status", true);
  request.onload = function () {
    if (request.readyState == 4 && request.status == 200) {
      var response = request.responseText;
      relay_status = Number.parseInt(response);
      if (relay_status == 0) document.getElementById("relay_button").classList.add("relay_off");
      else document.getElementById("relay_button").classList.add("relay_on");
    }
  };
  request.send();
}

function relay_inverse() {
  var request = new XMLHttpRequest();
  request.open("GET", "/relay_switch", false);
  request.send();
  if (request.readyState == 4 && request.status == 200) {
    var response = request.responseText;
    if (response == "0") {
      document.getElementById("relay_button").classList.remove("relay_on");
      document.getElementById("relay_button").classList.add("relay_off");
      relay_status = 0;
    } else {
      document.getElementById("relay_button").classList.remove("relay_off");
      document.getElementById("relay_button").classList.add("relay_on");
      relay_status = 1;
    }
  }
}
document.addEventListener("DOMContentLoaded", relay_state);
document.getElementById("relay_button").addEventListener("click", relay_inverse);
