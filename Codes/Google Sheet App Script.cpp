function doPost(e) {
  return handleRequest(e.parameter);
}

function doGet(e) {
  return handleRequest(e.parameter);
}

function handleRequest(params) {
  try {
    var ss = SpreadsheetApp.getActiveSpreadsheet();

    var name   = params.name   || "";
    var roll   = params.roll   || "";
    var date   = params.date   || "";
    var time   = params.time   || "";
    var branch = params.branch || "";

    if (name == "" && roll == "") {
      return ContentService.createTextOutput("ERROR: No data received");
    }

    // Select sheet based on branch
    var sheetName = "";
    if (branch == "CSE") sheetName = "CSE";
    else if (branch == "ECE") sheetName = "ECE";
    else if (branch == "IE")  sheetName = "IE";
    else sheetName = "General"; // default fallback

    var sheet = ss.getSheetByName(sheetName);

    // Create sheet if it doesn't exist
    if (!sheet) {
      sheet = ss.insertSheet(sheetName);
      sheet.appendRow(["SI No.", "Name", "Roll No.", "Date", "Entry Time", "Exit Time"]);
    }

    // Add headers if sheet is empty
    if (sheet.getLastRow() == 0) {
      sheet.appendRow(["SI No.", "Name", "Roll No.", "Date", "Entry Time", "Exit Time"]);
    }

    // Search for existing entry today with empty exit
    var lastRow = sheet.getLastRow();
    for (var i = 2; i <= lastRow; i++) {
      var rowRoll = sheet.getRange(i, 3).getValue();
      var rowDate = sheet.getRange(i, 4).getValue();
      var rowExit = sheet.getRange(i, 6).getValue();

      if (rowRoll == roll && rowDate == date && rowExit == "") {
        sheet.getRange(i, 6).setValue(time);
        return ContentService.createTextOutput("EXIT");
      }
    }

    // New entry
    var si = lastRow;
    sheet.appendRow([si, name, roll, branch, date, time, ""]);
    return ContentService.createTextOutput("ENTRY");

  } catch (error) {
    return ContentService.createTextOutput("ERROR: " + error.toString());
  }
}
