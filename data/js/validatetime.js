document.getElementById("time").addEventListener("input", function (e) {
  var input = this.value;
  var dateTimePattern = /^(\d{1,2})\.?(\d{0,2})?\.?(\d{0,4})? ?(\d{0,2})?:?(\d{0,2})?:?(\d{0,2})?$/;
  var matchResult = input.match(dateTimePattern);

  if (matchResult) {
    var groups = input.split(/[.: ]/);
    var day = matchResult[1] || '';
    var month = matchResult[2] || '';
    var year = matchResult[3] || '';
    var hour = matchResult[4] || '';
    var minute = matchResult[5] || '';
    var second = matchResult[6] || '';

    // Обработка day
	if (input.length <= 3) {
    if (day.length === 1 && input.endsWith('.')) {
      day = '0' + day + '.';
	  day = (parseInt(day) === 0) ? '01.' : day;
    }
    if (day.length === 2 && (input.endsWith('.') || month.length > 0)) {
      day = (parseInt(day) <= 31) ? day + '.' : '31.';
      day = (parseInt(day) === 0) ? '01.' : day;
    }}

    // Обработка month
	if (input.length > 3 && input.length <= 6) {
    if (month.length === 1 && input.endsWith('.')) {
      month = '0' + month + '.';
	  month = (parseInt(month) === 0) ? '01.' : month;
    }
    if (month.length === 2 && (input.endsWith('.') || year.length > 0)) {
      month = (parseInt(month) <= 12) ? month + '.' : '12.';
	  month = (parseInt(month) === 0) ? '01.' : month;	  
    }
	day = day + '.';
	}

    // Обработка year
	if (input.length > 6 && input.length <= 11) {
    if (year.length <= 4 && input.endsWith(' ') || hour.length > 0) {
		if (year < 2024) {
           year = '2024 ';
		} else {
			year = year + ' ';
		}
    }		
	day = day + '.';
	month = month + '.';	
	}

    // Обработка hour
	if (input.length > 11 && input.length <= 14) {
    if (hour.length === 1 && input.endsWith(':')) {
      hour = '0' + hour + ':';
    }
	if (hour.length === 2 && (input.endsWith(':') || minute.length > 0)) {
      hour = (parseInt(hour) > 23) ? '00:' : hour + ':'; 
    }
	day = day + '.';
	month = month + '.';
	year = year + ' ';
	}

    // Обработка minute
	if (input.length > 14 && input.length <= 17) {
    if (minute.length === 1 && input.endsWith(':')) {
      minute = '0' + minute + ':';
    }
	if (minute.length === 2 && (input.endsWith(':') || second.length > 0)) {
      minute = (parseInt(minute) > 59) ? '00:' : minute + ':'; 
    }
	day = day + '.';
	month = month + '.';
	year = year + ' ';
	hour = hour + ':';
	}

    // Обработка second
	if (input.length > 17 && input.length <= 19) {
    second = (parseInt(second) > 59) ? '00' : second; 
	day = day + '.';
	month = month + '.';
	year = year + ' ';
	hour = hour + ':';
	minute = minute + ':';
	}
	
    // Формируем строку с правильным порядком даты и времени
    filteredInput = (day.length > 0 ? day : '')
      + (month.length > 0 ? month : '')
      + (year.length > 0 ? year : '')
      + (hour.length > 0 ? hour : '')
      + (minute.length > 0 ? minute : '')
      + (second.length > 0 ? second : '');

    this.value = filteredInput;

    // Перемещаем курсор в конец строки
    this.selectionStart = this.selectionEnd = this.value.length;
  } else {
        this.value = input.slice(0, -1); // Удаляем последний символ
  }
});