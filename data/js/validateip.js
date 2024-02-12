function togglePasswordVisibility(inputId, btnId) {
  var passwordInput = document.getElementById(inputId);
  var showPasswordBtn = document.getElementById(btnId);

  if (passwordInput.type === "password") {
    passwordInput.type = "text";
    showPasswordBtn.textContent = "Скрыть пароль";
  } else {
    passwordInput.type = "password";
    showPasswordBtn.textContent = "Показать пароль";
  }
}

function addDots(value) {
  var isIPAddress = /^(\d{1,3}\.){0,3}\d{0,3}$/.test(value);
  var groups = value.split('.'); // Разделяем значение на группы

  if (!isIPAddress) {
    if (groups.length === 4) {
      value = value.slice(0, -1); // Удаляем последний символ
    } else {
	  if (value === '.') return ''; 
      var lastChar = value[value.length - 1];
      if (/[0-9]/.test(lastChar)) {
        var digit = lastChar;
        value = value.slice(0, -1) + '.' + digit; // Вставляем точку перед последней введенной цифрой
      } else {
        value = value.slice(0, -1); // Удаляем последний символ, если он не является цифрой
      }
    }
  }
  var groups = value.split('.'); // Разделяем значение на группы
  var modifiedGroups = groups.map(function (group) {
    var groupValue = parseInt(group, 10); // Преобразуем значение группы в число
    if (isNaN(groupValue) || groupValue < 0 || groupValue > 255) {
      return ''; // Заменяем невалидные значения на пустую строку
    }
    return groupValue.toString(); // Преобразуем значение обратно в строку
  });

  return modifiedGroups.join('.'); // Объединяем группы снова в строку и возвращаем результат
}

function validateSSID(value) {
  var maxLength = 16;
  if (value.length > maxLength) {
    alert("Некорректное имя WiFi сети: превышена максимальная длина");
    return value.substring(0, maxLength);
  }
  return value;
}

function validatePassword(value) {
  var maxLength = 16;
  if (value.length > maxLength) {
    alert("Некорректный пароль: превышена максимальная длина (допустимо не более 16 символов)");
    return;
  }
  // Другие действия, если необходимо
}

function validateInput(event) {
  var inputElement = event.target;
  var value = inputElement.value;
      inputElement.value = addDots(value);
}

function setupInputListeners(inputId, validationFunction) {
  var inputElement = document.getElementById(inputId);
  var timeoutId;

  if (inputElement) {
    inputElement.addEventListener("input", function (e) {
      clearTimeout(timeoutId);
      timeoutId = setTimeout(function() {
        validationFunction(inputElement.value);
      }, 300); // Adjust the delay as needed (e.g., 300 milliseconds)
    });
  }
}


// Usage
togglePasswordVisibility("password", "showPasswordBtn");
togglePasswordVisibility("passwordAp", "showPasswordBtnAp");
togglePasswordVisibility("passwordsys", "showPasswordBtnsys");

setupInputListeners("ipAddress", validateInput);
setupInputListeners("subnetMask", validateInput);
setupInputListeners("gateway", validateInput);

setupInputListeners("ssid", function (value) {
  document.getElementById("ssid").value = validateSSID(value);
});

setupInputListeners("password", function (value) {
  validatePassword(value);
});

setupInputListeners("ssidAp", function (value) {
  document.getElementById("ssidAp").value = validateSSID(value);
});

setupInputListeners("passwordAp", function (value) {
  validatePassword(value);
});
