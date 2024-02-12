document.getElementById("numericInput").addEventListener("keypress", function(e) {
   var keyCode = e.which ? e.which : e.keyCode; // получаем код нажатой клавиши

   if (keyCode < 48 || keyCode > 57) {
      e.preventDefault(); // отменяем ввод, если нажатая клавиша не является цифрой
   }
});

document.getElementById("lengthInput").addEventListener("input", function(e) {
  var maxLength = parseInt(this.getAttribute("maxlength")); // получаем максимальное количество символов

  if (this.value.length > maxLength) {
    this.value = this.value.substring(0, maxLength); // обрезаем введенный текст до максимальной длины
  }
});

document.getElementById("numericInput").addEventListener("input", function(e) {
   var keyCode = e.which ? e.which : e.keyCode; // получаем код нажатой клавиши
   var maxLength = parseInt(this.getAttribute("maxlength")); // получаем максимальное количество символов

   if (keyCode < 48 || keyCode > 57 || this.value.length > maxLength) {
      e.preventDefault(); // отменяем ввод, если нажатая клавиша не является цифрой или введено больше символов, чем разрешено
   }
});

