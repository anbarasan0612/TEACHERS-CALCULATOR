/*
  Teacher Marks Calculator - ESP32 + TFT_eSPI + 4x4 Keypad + AP Web Editor
  (SD Card Free & FS Conflict Fixed Version)

  Features:
  - 6 subject mark entry
  - Final report with pass percentage & rank calculations
  - Fail display on entry/subject screens
  - Long press # on main menu to start Access Point mode
  - Browser interface: edit student/subject names, download CSV directly
  - Press * on device to stop AP and return to menu
*/

#include <Arduino.h>
#include <FS.h>           // MUST be included before WebServer.h & TFT_eSPI.h
#include <LittleFS.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <Keypad.h>
#include <SPI.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();
Preferences prefs;
WebServer server(80);

// ---------------- WIFI ACCESS POINT SETTINGS ----------------

const char *AP_SSID = "Marks_Name_Edit";
const char *AP_PASS = "12345678";   // Minimum 8 characters

bool apRunning = false;
bool webRoutesReady = false;

// ---------------- KEYPAD SETTINGS ----------------

const byte ROWS = 4;
const byte COLS = 4;

char keypadKeys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

byte rowPins[ROWS] = {13, 14, 27, 26};
byte colPins[COLS] = {25, 33, 32, 16};

Keypad keypad = Keypad(makeKeymap(keypadKeys), rowPins, colPins, ROWS, COLS);

// ---------------- STUDENT AND SUBJECT DATA ----------------

const int MAX_STUDENT_COUNT = 50;
const int DEFAULT_STUDENT_COUNT = 30;
const int SUBJECT_COUNT = 6;
const int PASS_MARK = 35;
const int MAX_MARK = 100;
const int TOTAL_MARKS = SUBJECT_COUNT * MAX_MARK;

int studentCount = DEFAULT_STUDENT_COUNT;

String students[MAX_STUDENT_COUNT] = {
  "C.Anbarasan", "G.Aswin kumar", "S.Anbu Selvam", "S.Kailash", "K.Kanisk",
  "K.Karish Moorthy", "K.Kevin", "B.Kirthick", "C.Kunal", "K.Malai Arasan",
  "S.Manish pandi", "T.Monish", "A.Praveen Kumar", "A.Prabhakaran", "R.Praveen Kumar",
  "S.Praveen Kumar", "S.Ponsivabalan", "S.Sachin Kumar", "D.Sakthi Varman", "R.Sri hari",
  "R.Rahul", "C.Lohit", "P.Sri Balagi", "S.Vetrivel", "S.Vishnu Shylender",
  "S.Vijayakumar", "M.Vishwajit", "N.Vignesh Ayyar", "M.Yogesh", "Student 30"
};

String subjects[SUBJECT_COUNT] = {
  "Tamil", "English", "Maths", "Physics", "Chemistry", "Computer Science"
};

int marks[MAX_STUDENT_COUNT][SUBJECT_COUNT];

// ---------------- SCREEN MODES ----------------

enum ScreenMode {
  SCREEN_MENU,
  SCREEN_ENTRY,
  SCREEN_REPORT,
  SCREEN_RANK_MENU,
  SCREEN_RANK_LIST,
  SCREEN_CLEAR_CONFIRM,
  SCREEN_AP
};

ScreenMode screenMode = SCREEN_MENU;

int currentSubject = 0;
int currentStudent = 0;
int reportPage = 0;
int failedPage = 0;
int rankSubject = -1;
int rankPage = 0;

String inputMark = "";

// ---------------- BASIC HELPERS ----------------

String markKey(int studentIndex, int subjectIndex) {
  return "m" + String(studentIndex) + "_" + String(subjectIndex);
}

String studentNameKey(int studentIndex) {
  return "stu" + String(studentIndex);
}

String subjectNameKey(int subjectIndex) {
  return "sub" + String(subjectIndex);
}

String cleanText(String text, int maxLen) {
  text.trim();
  text.replace(",", " ");
  text.replace("\r", " ");
  text.replace("\n", " ");

  while (text.indexOf("  ") >= 0) {
    text.replace("  ", " ");
  }

  if (text.length() > maxLen) {
    text = text.substring(0, maxLen);
  }

  return text;
}

void fillEmptyDefaultNames() {
  for (int i = 0; i < MAX_STUDENT_COUNT; i++) {
    students[i].trim();
    if (students[i].length() == 0) {
      students[i] = "Student " + String(i + 1);
    }
  }
}

void setAllMarksEmpty() {
  for (int i = 0; i < MAX_STUDENT_COUNT; i++) {
    for (int j = 0; j < SUBJECT_COUNT; j++) {
      marks[i][j] = -1;
    }
  }
}

// ---------------- CALCULATION FUNCTIONS ----------------

bool isMarkEntered(int studentIndex, int subjectIndex) {
  return marks[studentIndex][subjectIndex] >= 0;
}

int getMarkValue(int studentIndex, int subjectIndex) {
  if (marks[studentIndex][subjectIndex] < 0) return 0;
  return marks[studentIndex][subjectIndex];
}

bool isSubjectFailed(int studentIndex, int subjectIndex) {
  return getMarkValue(studentIndex, subjectIndex) < PASS_MARK;
}

int getStudentTotal(int studentIndex) {
  int total = 0;
  for (int j = 0; j < SUBJECT_COUNT; j++) {
    total += getMarkValue(studentIndex, j);
  }
  return total;
}

float getStudentPercentage(int studentIndex) {
  return (getStudentTotal(studentIndex) / (float)TOTAL_MARKS) * 100.0;
}

bool isStudentFailed(int studentIndex) {
  for (int j = 0; j < SUBJECT_COUNT; j++) {
    if (isSubjectFailed(studentIndex, j)) return true;
  }
  return false;
}

int getFailedStudentCount() {
  int count = 0;
  for (int i = 0; i < studentCount; i++) {
    if (isStudentFailed(i)) count++;
  }
  return count;
}

int getPassedStudentCount() {
  return studentCount - getFailedStudentCount();
}

float getOverallPassPercentage() {
  if (studentCount <= 0) return 0;
  return (getPassedStudentCount() / (float)studentCount) * 100.0;
}

float getClassAverage() {
  if (studentCount <= 0) return 0;
  int grandTotal = 0;
  for (int i = 0; i < studentCount; i++) {
    grandTotal += getStudentTotal(i);
  }
  return grandTotal / (float)studentCount;
}

int getSubjectFailCount(int subjectIndex) {
  int count = 0;
  for (int i = 0; i < studentCount; i++) {
    if (isSubjectFailed(i, subjectIndex)) count++;
  }
  return count;
}

float getSubjectAverage(int subjectIndex) {
  if (studentCount <= 0) return 0;
  int total = 0;
  for (int i = 0; i < studentCount; i++) {
    total += getMarkValue(i, subjectIndex);
  }
  return total / (float)studentCount;
}

String getFailedSubjectsShort(int studentIndex) {
  String text = "";
  for (int j = 0; j < SUBJECT_COUNT; j++) {
    if (isSubjectFailed(studentIndex, j)) {
      if (text.length() > 0) text += " ";
      text += subjects[j].substring(0, 3);
    }
  }
  return (text.length() == 0) ? "-" : text;
}

void getOverallRankOrder(int order[]) {
  for (int i = 0; i < studentCount; i++) order[i] = i;
  for (int i = 0; i < studentCount - 1; i++) {
    for (int j = i + 1; j < studentCount; j++) {
      if (getStudentTotal(order[j]) > getStudentTotal(order[i])) {
        int temp = order[i];
        order[i] = order[j];
        order[j] = temp;
      }
    }
  }
}

void getSubjectRankOrder(int subjectIndex, int order[]) {
  for (int i = 0; i < studentCount; i++) order[i] = i;
  for (int i = 0; i < studentCount - 1; i++) {
    for (int j = i + 1; j < studentCount; j++) {
      if (getMarkValue(order[j], subjectIndex) > getMarkValue(order[i], subjectIndex)) {
        int temp = order[i];
        order[i] = order[j];
        order[j] = temp;
      }
    }
  }
}

int getScoreForRank(int studentIndex) {
  return (rankSubject == -1) ? getStudentTotal(studentIndex) : getMarkValue(studentIndex, rankSubject);
}

int getRankNumber(int position, int order[]) {
  int rankNumber = 1;
  for (int i = 1; i <= position; i++) {
    if (getScoreForRank(order[i]) < getScoreForRank(order[i - 1])) {
      rankNumber = i + 1;
    }
  }
  return rankNumber;
}

int getFailedPageCount() {
  int failedCount = getFailedStudentCount();
  return (failedCount == 0) ? 1 : (failedCount + 6) / 7;
}

// ---------------- PREFERENCES STORAGE ----------------

void loadStudentCountFromPrefs() {
  studentCount = prefs.getInt("stuCount", DEFAULT_STUDENT_COUNT);
  if (studentCount < 1) studentCount = 1;
  if (studentCount > MAX_STUDENT_COUNT) studentCount = MAX_STUDENT_COUNT;
}

void saveStudentCountToPrefs() {
  prefs.putInt("stuCount", studentCount);
}

void loadMarksFromPrefs() {
  for (int i = 0; i < MAX_STUDENT_COUNT; i++) {
    for (int j = 0; j < SUBJECT_COUNT; j++) {
      marks[i][j] = prefs.getInt(markKey(i, j).c_str(), -1);
    }
  }
}

void saveSingleMarkToPrefs(int studentIndex, int subjectIndex) {
  prefs.putInt(markKey(studentIndex, subjectIndex).c_str(), marks[studentIndex][subjectIndex]);
}

void saveAllMarksToPrefs() {
  for (int i = 0; i < MAX_STUDENT_COUNT; i++) {
    for (int j = 0; j < SUBJECT_COUNT; j++) {
      saveSingleMarkToPrefs(i, j);
    }
  }
}

void loadNamesFromPrefs() {
  fillEmptyDefaultNames();
  for (int i = 0; i < studentCount; i++) {
    String savedName = prefs.getString(studentNameKey(i).c_str(), students[i]);
    savedName = cleanText(savedName, 30);
    if (savedName.length() > 0) students[i] = savedName;
  }
  for (int j = 0; j < SUBJECT_COUNT; j++) {
    String savedSubject = prefs.getString(subjectNameKey(j).c_str(), subjects[j]);
    savedSubject = cleanText(savedSubject, 24);
    if (savedSubject.length() > 0) subjects[j] = savedSubject;
  }
}

void saveAllNamesToPrefs() {
  for (int i = 0; i < MAX_STUDENT_COUNT; i++) {
    prefs.putString(studentNameKey(i).c_str(), students[i]);
  }
  for (int j = 0; j < SUBJECT_COUNT; j++) {
    prefs.putString(subjectNameKey(j).c_str(), subjects[j]);
  }
  saveStudentCountToPrefs();
}

void clearMarkPrefsOnly() {
  for (int i = 0; i < MAX_STUDENT_COUNT; i++) {
    for (int j = 0; j < SUBJECT_COUNT; j++) {
      prefs.remove(markKey(i, j).c_str());
    }
  }
}

void clearAllData() {
  setAllMarksEmpty();
  clearMarkPrefsOnly();
}

// ---------------- STUDENT ADD / DELETE ----------------

void clearStudentSlot(int index) {
  if (index < 0 || index >= MAX_STUDENT_COUNT) return;
  students[index] = "Student " + String(index + 1);
  for (int j = 0; j < SUBJECT_COUNT; j++) marks[index][j] = -1;
}

bool deleteStudent(int index) {
  if (index < 0 || index >= studentCount || studentCount <= 1) return false;

  for (int i = index; i < studentCount - 1; i++) {
    students[i] = students[i + 1];
    for (int j = 0; j < SUBJECT_COUNT; j++) {
      marks[i][j] = marks[i + 1][j];
    }
  }

  clearStudentSlot(studentCount - 1);
  studentCount--;

  if (currentStudent >= studentCount) {
    currentStudent = studentCount - 1;
  }

  saveAllNamesToPrefs();
  saveAllMarksToPrefs();
  return true;
}

bool addStudent(String newName) {
  if (studentCount >= MAX_STUDENT_COUNT) return false;

  newName = cleanText(newName, 30);
  if (newName.length() == 0) {
    newName = "Student " + String(studentCount + 1);
  }

  students[studentCount] = newName;
  for (int j = 0; j < SUBJECT_COUNT; j++) marks[studentCount][j] = -1;

  studentCount++;
  saveAllNamesToPrefs();
  saveAllMarksToPrefs();
  return true;
}

// ---------------- CSV GENERATION (RAM / STREAM) ----------------

String buildCSVContent() {
  String csv = "Student";
  for (int j = 0; j < SUBJECT_COUNT; j++) {
    csv += "," + subjects[j];
  }
  csv += ",Total,Percentage,Result,Failed Subjects\n";

  for (int i = 0; i < studentCount; i++) {
    csv += students[i];
    for (int j = 0; j < SUBJECT_COUNT; j++) {
      csv += "," + String(marks[i][j]);
    }
    csv += "," + String(getStudentTotal(i));
    csv += "," + String(getStudentPercentage(i), 1);
    csv += "," + String(isStudentFailed(i) ? "FAIL" : "PASS");
    csv += "," + getFailedSubjectsShort(i) + "\n";
  }
  return csv;
}

// ---------------- DISPLAY HELPERS ----------------

void drawHeader(String title) {
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, tft.width(), 30, TFT_BLUE);
  tft.setTextColor(TFT_WHITE, TFT_BLUE);
  tft.setTextSize(2);
  tft.setCursor(8, 7);
  tft.print(title);
}

void printText(int x, int y, String text, uint16_t color, int size) {
  tft.setTextColor(color, TFT_BLACK);
  tft.setTextSize(size);
  tft.setCursor(x, y);
  tft.print(text);
}

// ---------------- WEB HTML HELPERS ----------------

const int STUDENT_WEB_PAGE_SIZE = 10;

String htmlEscape(String text) {
  text.replace("&", "&amp;");
  text.replace("\"", "&quot;");
  text.replace("'", "&#39;");
  text.replace("<", "&lt;");
  text.replace(">", "&gt;");
  return text;
}

String htmlPageStart(String title) {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>" + title + "</title><style>";
  html += "body{font-family:Arial,sans-serif;max-width:720px;margin:auto;padding:14px;background:#f6f7fb;}";
  html += "h2{margin:8px 0 4px;} h3{margin:6px 0;}";
  html += ".card{background:white;border-radius:14px;padding:14px;margin:12px 0;box-shadow:0 2px 10px #0001;}";
  html += "label{font-weight:bold;font-size:13px;display:block;margin-top:6px;}";
  html += "input{width:100%;box-sizing:border-box;padding:10px;margin:4px 0 10px;border:1px solid #bbb;border-radius:8px;}";
  html += "button,.btn{display:inline-block;background:#1769ff;color:white;border:0;border-radius:10px;padding:10px 13px;text-decoration:none;font-weight:bold;margin:4px 3px 4px 0;}";
  html += ".danger{background:#c62828;} .ok{background:#188038;} .grey{background:#666;}";
  html += ".small{color:#555;font-size:13px;}";
  html += "</style></head><body>";
  return html;
}

String htmlPageEnd() {
  return "</body></html>";
}

void sendBackHomeMessage(String title, String message) {
  String html = htmlPageStart(title);
  html += "<div class='card'><h2>" + htmlEscape(title) + "</h2>";
  html += "<p>" + message + "</p><a class='btn' href='/'>Home</a></div>";
  html += htmlPageEnd();
  server.send(200, "text/html", html);
}

int getStudentPageFromWeb() {
  int page = server.hasArg("p") ? server.arg("p").toInt() : 0;
  int maxPage = (studentCount - 1) / STUDENT_WEB_PAGE_SIZE;
  if (page < 0) page = 0;
  if (page > maxPage) page = maxPage;
  return page;
}

// ---------------- WEB ROUTES ----------------

void handleRootPage() {
  String html = htmlPageStart("Marks Calculator");
  html += "<h2>Teacher Marks Calculator</h2>";
  html += "<p class='small'>AP mode active. Press <b>*</b> on hardware keypad to exit.</p>";

  html += "<div class='card'><h3>Students</h3>";
  html += "<p class='small'>Current count: <b>" + String(studentCount) + "</b> / " + String(MAX_STUDENT_COUNT) + "</p>";
  html += "<a class='btn' href='/students?p=0'>Edit / Delete Students</a>";
  html += "<a class='btn ok' href='/add'>Add Student</a>";
  html += "</div>";

  html += "<div class='card'><h3>Subjects</h3>";
  html += "<a class='btn' href='/subjects'>Edit Subject Names</a>";
  html += "</div>";

  html += "<div class='card'><h3>Export Data</h3>";
  html += "<a class='btn ok' href='/download'>Download Marks CSV</a>";
  html += "</div>";

  html += htmlPageEnd();
  server.send(200, "text/html", html);
}

void handleStudentsPage() {
  int page = getStudentPageFromWeb();
  int maxPage = (studentCount - 1) / STUDENT_WEB_PAGE_SIZE;
  int startIndex = page * STUDENT_WEB_PAGE_SIZE;
  int endIndex = min(startIndex + STUDENT_WEB_PAGE_SIZE, studentCount);

  String html = htmlPageStart("Edit Students");
  html += "<h2>Edit / Delete Student Names</h2>";
  html += "<p class='small'>Page <b>" + String(page + 1) + "</b> / " + String(maxPage + 1) + "</p>";

  html += "<form method='POST' action='/saveStudents'>";
  html += "<input type='hidden' name='p' value='" + String(page) + "'>";

  for (int i = startIndex; i < endIndex; i++) {
    html += "<div class='card'><label>Student " + String(i + 1) + "</label>";
    html += "<input name='s" + String(i) + "' maxlength='30' value='" + htmlEscape(students[i]) + "'>";
    html += "<a class='btn danger' href='/deleteStudent?id=" + String(i) + "&p=" + String(page) + "'>Delete</a></div>";
  }

  html += "<button type='submit'>Save Page</button></form><div class='card'>";
  if (page > 0) html += "<a class='btn' href='/students?p=" + String(page - 1) + "'>Previous</a>";
  if (page < maxPage) html += "<a class='btn' href='/students?p=" + String(page + 1) + "'>Next</a>";
  html += "<a class='btn grey' href='/'>Home</a></div>";

  html += htmlPageEnd();
  server.send(200, "text/html", html);
}

void handleSaveStudents() {
  int page = getStudentPageFromWeb();
  int startIndex = page * STUDENT_WEB_PAGE_SIZE;
  int endIndex = min(startIndex + STUDENT_WEB_PAGE_SIZE, studentCount);

  for (int i = startIndex; i < endIndex; i++) {
    String argName = "s" + String(i);
    if (server.hasArg(argName)) {
      String newName = cleanText(server.arg(argName), 30);
      if (newName.length() > 0) students[i] = newName;
    }
  }

  saveAllNamesToPrefs();
  sendBackHomeMessage("Saved", "Student names updated successfully.");
}

void handleAddPage() {
  String html = htmlPageStart("Add Student");
  html += "<h2>Add New Student</h2>";
  if (studentCount < MAX_STUDENT_COUNT) {
    html += "<div class='card'><form method='POST' action='/addStudent'>";
    html += "<label>New Name</label><input name='newStudent' maxlength='30' placeholder='Enter name'>";
    html += "<button type='submit'>Add Student</button></form></div>";
  } else {
    html += "<div class='card'><p>Maximum limit reached.</p></div>";
  }
  html += "<a class='btn grey' href='/'>Home</a>" + htmlPageEnd();
  server.send(200, "text/html", html);
}

void handleAddStudent() {
  String newName = server.hasArg("newStudent") ? server.arg("newStudent") : "";
  if (!addStudent(newName)) {
    sendBackHomeMessage("Limit reached", "Cannot add more students.");
    return;
  }
  sendBackHomeMessage("Student Added", "Successfully created standard entry.");
}

void handleDeleteStudentConfirm() {
  if (!server.hasArg("id")) {
    server.sendHeader("Location", "/students");
    server.send(303);
    return;
  }

  int id = server.arg("id").toInt();
  int page = server.hasArg("p") ? server.arg("p").toInt() : 0;

  String html = htmlPageStart("Delete Student");
  html += "<div class='card'><h2>Delete Student?</h2>";
  html += "<p>Are you sure you want to delete <b>" + htmlEscape(students[id]) + "</b>?</p>";
  html += "<form method='POST' action='/doDeleteStudent'>";
  html += "<input type='hidden' name='id' value='" + String(id) + "'>";
  html += "<button class='danger' type='submit'>Yes, Delete</button></form>";
  html += "<a class='btn grey' href='/students?p=" + String(page) + "'>Cancel</a></div>";
  html += htmlPageEnd();

  server.send(200, "text/html", html);
}

void handleDoDeleteStudent() {
  if (server.hasArg("id")) {
    deleteStudent(server.arg("id").toInt());
  }
  server.sendHeader("Location", "/students");
  server.send(303);
}

void handleSubjectsPage() {
  String html = htmlPageStart("Edit Subjects");
  html += "<h2>Edit Subject Names</h2><div class='card'><form method='POST' action='/saveSubjects'>";
  for (int j = 0; j < SUBJECT_COUNT; j++) {
    html += "<label>Subject " + String(j + 1) + "</label>";
    html += "<input name='sub" + String(j) + "' maxlength='24' value='" + htmlEscape(subjects[j]) + "'>";
  }
  html += "<button type='submit'>Save Subjects</button></form></div>";
  html += "<a class='btn grey' href='/'>Home</a>" + htmlPageEnd();
  server.send(200, "text/html", html);
}

void handleSaveSubjects() {
  for (int j = 0; j < SUBJECT_COUNT; j++) {
    String argName = "sub" + String(j);
    if (server.hasArg(argName)) {
      String newName = cleanText(server.arg(argName), 24);
      if (newName.length() > 0) subjects[j] = newName;
    }
  }
  saveAllNamesToPrefs();
  sendBackHomeMessage("Saved", "Subject names updated successfully.");
}

void handleDownloadCSV() {
  String csvData = buildCSVContent();
  server.sendHeader("Content-Type", "text/csv");
  server.sendHeader("Content-Disposition", "attachment; filename=marks.csv");
  server.send(200, "text/csv", csvData);
}

// ---------------- WEB ROUTES / AP CONTROL ----------------

void setupWebRoutes() {
  if (webRoutesReady) return;

  server.on("/", HTTP_GET, handleRootPage);
  server.on("/students", HTTP_GET, handleStudentsPage);
  server.on("/saveStudents", HTTP_POST, handleSaveStudents);
  server.on("/add", HTTP_GET, handleAddPage);
  server.on("/addStudent", HTTP_POST, handleAddStudent);
  server.on("/deleteStudent", HTTP_GET, handleDeleteStudentConfirm);
  server.on("/doDeleteStudent", HTTP_POST, handleDoDeleteStudent);
  server.on("/subjects", HTTP_GET, handleSubjectsPage);
  server.on("/saveSubjects", HTTP_POST, handleSaveSubjects);
  server.on("/download", HTTP_GET, handleDownloadCSV);

  server.onNotFound(handleRootPage);
  webRoutesReady = true;
}

void drawAPScreen() {
  screenMode = SCREEN_AP;
  drawHeader("WiFi AP Mode");
  printText(10, 45, "Connect WiFi:", TFT_YELLOW, 2);
  printText(10, 75, String(AP_SSID), TFT_WHITE, 2);
  printText(10, 110, "Password:", TFT_YELLOW, 2);
  printText(10, 140, String(AP_PASS), TFT_WHITE, 2);
  printText(10, 175, "Open Browser:", TFT_YELLOW, 2);
  printText(10, 205, WiFi.softAPIP().toString(), TFT_CYAN, 2);
  printText(10, 228, "* Stop AP", TFT_GREEN, 1);
}

void startAPMode() {
  if (apRunning) {
    drawAPScreen();
    return;
  }

  drawHeader("Starting WiFi");
  printText(10, 80, "Please wait...", TFT_YELLOW, 2);

  WiFi.mode(WIFI_OFF);
  delay(200);
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);

  bool ok = WiFi.softAP(AP_SSID, AP_PASS, 1, false, 1);
  setupWebRoutes();
  server.begin();

  apRunning = ok;
  drawAPScreen();
}

void stopAPMode() {
  if (apRunning) {
    server.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    apRunning = false;
    delay(150);
  }
  drawMainMenu();
}

void handleAPKey(char key) {
  if (key == '*') stopAPMode();
  else if (key == 'D') drawAPScreen();
}

void keypadEvent(KeypadEvent key) {
  if (key == '#' && keypad.getState() == HOLD && screenMode == SCREEN_MENU && !apRunning) {
    startAPMode();
  }
}

// ---------------- MAIN MENU ----------------

void drawMainMenu() {
  screenMode = SCREEN_MENU;
  drawHeader("Marks Calculator");

  printText(10, 38, "1 Tamil", TFT_WHITE, 2);
  printText(10, 60, "2 English", TFT_WHITE, 2);
  printText(10, 82, "3 Maths", TFT_WHITE, 2);
  printText(10, 104, "4 Physics", TFT_WHITE, 2);
  printText(10, 126, "5 Chemistry", TFT_WHITE, 2);
  printText(10, 148, "6 Computer", TFT_WHITE, 2);

  printText(10, 176, "7 Final Report", TFT_YELLOW, 2);
  printText(10, 198, "8 Rank Menu", TFT_CYAN, 2);
  printText(10, 220, "9 Clear Marks", TFT_RED, 2);

  printText(155, 205, "Students:" + String(studentCount), TFT_CYAN, 1);
  printText(155, 218, "Hold # Web", TFT_GREEN, 1);
}

// ---------------- ENTRY SCREEN ----------------

void drawEntryScreen() {
  screenMode = SCREEN_ENTRY;
  drawHeader(subjects[currentSubject]);

  String studentLine = String(currentStudent + 1) + "/" + String(studentCount) + " " + students[currentStudent];
  printText(10, 45, studentLine, TFT_CYAN, 2);

  String oldMarkText = "Saved: ";
  if (marks[currentStudent][currentSubject] < 0) {
    oldMarkText += "Not entered";
  } else {
    oldMarkText += String(marks[currentStudent][currentSubject]);
    if (marks[currentStudent][currentSubject] < PASS_MARK) oldMarkText += " FAIL";
  }

  printText(10, 75, oldMarkText, TFT_WHITE, 2);
  printText(10, 110, "Enter Mark:", TFT_YELLOW, 2);

  tft.fillRect(10, 140, 150, 40, TFT_DARKGREY);
  tft.drawRect(10, 140, 150, 40, TFT_WHITE);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.setTextSize(3);
  tft.setCursor(20, 148);

  tft.print(inputMark.length() == 0 ? "_" : inputMark);

  printText(10, 195, "#/D Save  C Clear", TFT_GREEN, 1);
  printText(10, 212, "A Next  B Prev  * Back", TFT_GREEN, 1);
}

// ---------------- FINAL REPORT SCREENS ----------------

void drawReportPage0() {
  drawHeader("Final Report");
  int order[MAX_STUDENT_COUNT];
  getOverallRankOrder(order);
  int topper = order[0];

  printText(10, 40, "Topper:", TFT_YELLOW, 2);
  printText(120, 40, students[topper], TFT_WHITE, 2);
  printText(10, 65, "Marks:", TFT_YELLOW, 2);
  printText(120, 65, String(getStudentTotal(topper)) + "/" + String(TOTAL_MARKS), TFT_WHITE, 2);
  printText(10, 90, "Class Avg:", TFT_YELLOW, 2);
  printText(150, 90, String(getClassAverage(), 1), TFT_WHITE, 2);
  printText(10, 115, "Passed:", TFT_YELLOW, 2);
  printText(120, 115, String(getPassedStudentCount()) + "/" + String(studentCount), TFT_GREEN, 2);
  printText(10, 140, "Pass %:", TFT_YELLOW, 2);
  printText(120, 140, String(getOverallPassPercentage(), 1) + "%", TFT_GREEN, 2);
  printText(10, 165, "Failed:", TFT_YELLOW, 2);
  printText(120, 165, String(getFailedStudentCount()), TFT_RED, 2);

  printText(10, 190, "Top 3:", TFT_CYAN, 1);
  int y = 207;
  for (int r = 0; r < studentCount && r < 3; r++) {
    int s = order[r];
    String line = String(r + 1) + ". " + students[s] + " " + String(getStudentTotal(s)) + " " + String(getStudentPercentage(s), 1) + "%";
    printText(10, y, line, TFT_WHITE, 1);
    y += 12;
  }
  printText(210, 225, "A Next *Menu", TFT_GREEN, 1);
}

void drawReportPage1() {
  drawHeader("Subject Report");
  int y = 40;
  for (int i = 0; i < SUBJECT_COUNT; i++) {
    String line = subjects[i].substring(0, 8) + " Avg:" + String(getSubjectAverage(i), 1) + " F:" + String(getSubjectFailCount(i));
    printText(8, y, line, TFT_WHITE, 1);
    y += 25;
  }
  printText(10, 205, "Fail = mark below " + String(PASS_MARK), TFT_RED, 1);
  printText(10, 225, "A Next  B Prev  * Menu", TFT_GREEN, 1);
}

void drawReportPage2() {
  drawHeader("Failed Students");
  const int rowsPerPage = 7;
  int startFail = failedPage * rowsPerPage;
  int printed = 0, seenFail = 0, y = 42;

  for (int i = 0; i < studentCount; i++) {
    if (isStudentFailed(i)) {
      if (seenFail >= startFail && printed < rowsPerPage) {
        String line = String(i + 1) + ". " + students[i] + " " + getFailedSubjectsShort(i);
        printText(8, y, line, TFT_RED, 1);
        y += 22;
        printed++;
      }
      seenFail++;
    }
  }

  if (seenFail == 0) printText(20, 85, "No failed students", TFT_GREEN, 2);

  printText(10, 200, "Page " + String(failedPage + 1) + "/" + String(getFailedPageCount()), TFT_CYAN, 1);
  printText(10, 225, "A Next  B Prev  * Menu", TFT_GREEN, 1);
}

void drawReportScreen() {
  screenMode = SCREEN_REPORT;
  if (reportPage == 0) drawReportPage0();
  else if (reportPage == 1) drawReportPage1();
  else drawReportPage2();
}

// ---------------- RANK SCREENS ----------------

void drawRankMenu() {
  screenMode = SCREEN_RANK_MENU;
  drawHeader("Rank Menu");
  printText(10, 38, "1 Overall Rank", TFT_YELLOW, 2);
  printText(10, 60, "2 Tamil", TFT_WHITE, 2);
  printText(10, 82, "3 English", TFT_WHITE, 2);
  printText(10, 104, "4 Maths", TFT_WHITE, 2);
  printText(10, 126, "5 Physics", TFT_WHITE, 2);
  printText(10, 148, "6 Chemistry", TFT_WHITE, 2);
  printText(10, 170, "7 Computer", TFT_WHITE, 2);
  printText(10, 215, "* Back Menu", TFT_GREEN, 1);
}

void drawRankList() {
  screenMode = SCREEN_RANK_LIST;
  drawHeader((rankSubject == -1) ? "Overall Rank" : subjects[rankSubject]);

  int order[MAX_STUDENT_COUNT];
  if (rankSubject == -1) getOverallRankOrder(order);
  else getSubjectRankOrder(rankSubject, order);

  const int rowsPerPage = 8;
  int start = rankPage * rowsPerPage;
  int end = min(start + rowsPerPage, studentCount);
  int y = 38;

  for (int pos = start; pos < end; pos++) {
    int s = order[pos];
    int rankNo = getRankNumber(pos, order);

    String line = String(rankNo) + ". " + students[s].substring(0, 16) + " ";
    if (rankSubject == -1) {
      line += String(getStudentTotal(s)) + " " + String(getStudentPercentage(s), 1) + "%";
    } else {
      int mark = getMarkValue(s, rankSubject);
      line += String(mark);
      if (mark < PASS_MARK) line += " FAIL";
    }

    uint16_t color = (rankSubject != -1 && getMarkValue(s, rankSubject) < PASS_MARK) ? TFT_RED : TFT_WHITE;
    printText(5, y, line, color, 1);
    y += 22;
  }

  int pageCount = (studentCount + rowsPerPage - 1) / rowsPerPage;
  printText(10, 205, "Page " + String(rankPage + 1) + "/" + String(pageCount), TFT_CYAN, 1);
  printText(10, 225, "A Next  B Prev  C Menu  * Main", TFT_GREEN, 1);
}

// ---------------- CLEAR CONFIRM SCREEN ----------------

void drawClearConfirmScreen() {
  screenMode = SCREEN_CLEAR_CONFIRM;
  drawHeader("Clear Marks");
  printText(10, 55, "Delete all marks?", TFT_RED, 2);
  printText(10, 85, "Names stay safe.", TFT_WHITE, 2);
  printText(10, 115, "This clears:", TFT_WHITE, 2);
  printText(10, 140, "- ESP32 mark memory", TFT_WHITE, 2);
  printText(10, 200, "D or # = YES CLEAR", TFT_RED, 1);
  printText(10, 220, "B or * = Cancel", TFT_GREEN, 1);
}

// ---------------- KEY HANDLING ----------------

void handleMenuKey(char key) {
  if (key >= '1' && key <= '6') {
    currentSubject = key - '1';
    currentStudent = 0;
    inputMark = "";
    drawEntryScreen();
  } else if (key == '7') {
    reportPage = 0;
    failedPage = 0;
    drawReportScreen();
  } else if (key == '8') {
    drawRankMenu();
  } else if (key == '9') {
    drawClearConfirmScreen();
  }
}

void saveEnteredMark() {
  if (inputMark.length() == 0) return;

  int value = inputMark.toInt();
  if (value < 0 || value > 100) {
    inputMark = "";
    drawEntryScreen();
    return;
  }

  marks[currentStudent][currentSubject] = value;
  saveSingleMarkToPrefs(currentStudent, currentSubject);

  inputMark = "";
  if (currentStudent < studentCount - 1) currentStudent++;
  drawEntryScreen();
}

void handleEntryKey(char key) {
  if (key >= '0' && key <= '9') {
    if (inputMark.length() < 3) inputMark += key;
    drawEntryScreen();
  } else if (key == 'C') {
    inputMark = "";
    drawEntryScreen();
  } else if (key == '#' || key == 'D') {
    saveEnteredMark();
  } else if (key == 'A') {
    currentStudent = (currentStudent < studentCount - 1) ? currentStudent + 1 : 0;
    inputMark = "";
    drawEntryScreen();
  } else if (key == 'B') {
    currentStudent = (currentStudent > 0) ? currentStudent - 1 : studentCount - 1;
    inputMark = "";
    drawEntryScreen();
  } else if (key == '*') {
    inputMark = "";
    drawMainMenu();
  }
}

void handleReportKey(char key) {
  if (key == 'A') {
    reportPage = (reportPage < 2) ? reportPage + 1 : 0;
    if (reportPage == 2) failedPage = 0;
    drawReportScreen();
  } else if (key == 'B') {
    reportPage = (reportPage > 0) ? reportPage - 1 : 2;
    drawReportScreen();
  } else if (key == 'C') {
    drawRankMenu();
  } else if (key == '*') {
    drawMainMenu();
  } else if (key == 'D' && reportPage == 2) {
    int pageCount = getFailedPageCount();
    failedPage = (failedPage + 1) % pageCount;
    drawReportScreen();
  }
}

void handleRankMenuKey(char key) {
  if (key == '1') {
    rankSubject = -1;
    rankPage = 0;
    drawRankList();
  } else if (key >= '2' && key <= '7') {
    rankSubject = key - '2';
    rankPage = 0;
    drawRankList();
  } else if (key == '*') {
    drawMainMenu();
  }
}

void handleRankListKey(char key) {
  const int rowsPerPage = 8;
  int pageCount = (studentCount + rowsPerPage - 1) / rowsPerPage;

  if (key == 'A') {
    rankPage = (rankPage + 1) % pageCount;
    drawRankList();
  } else if (key == 'B') {
    rankPage = (rankPage > 0) ? rankPage - 1 : pageCount - 1;
    drawRankList();
  } else if (key == 'C') {
    drawRankMenu();
  } else if (key == '*') {
    drawMainMenu();
  }
}

void handleClearConfirmKey(char key) {
  if (key == 'D' || key == '#') {
    clearAllData();
    drawHeader("Cleared");
    printText(20, 90, "All marks cleared", TFT_GREEN, 2);
    delay(1000);
    drawMainMenu();
  } else if (key == 'B' || key == '*') {
    drawMainMenu();
  }
}

// ---------------- SETUP / LOOP ----------------

void setup() {
  Serial.begin(115200);
  delay(300);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed");
  }

  prefs.begin("markcalc", false);

  setAllMarksEmpty();
  fillEmptyDefaultNames();

  loadStudentCountFromPrefs();
  loadNamesFromPrefs();
  loadMarksFromPrefs();

  keypad.setHoldTime(1200);
  keypad.addEventListener(keypadEvent);

  drawMainMenu();
}

void loop() {
  char key = keypad.getKey();

  if (apRunning) {
    server.handleClient();
    if (key) handleAPKey(key);
    delay(2);
    return;
  }

  if (!key) {
    delay(2);
    return;
  }

  switch (screenMode) {
    case SCREEN_MENU: handleMenuKey(key); break;
    case SCREEN_ENTRY: handleEntryKey(key); break;
    case SCREEN_REPORT: handleReportKey(key); break;
    case SCREEN_RANK_MENU: handleRankMenuKey(key); break;
    case SCREEN_RANK_LIST: handleRankListKey(key); break;
    case SCREEN_CLEAR_CONFIRM: handleClearConfirmKey(key); break;
    case SCREEN_AP: handleAPKey(key); break;
  }
}