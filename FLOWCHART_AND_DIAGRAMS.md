# RFID System - Complete Flowchart & State Diagram

## 1. Main System State Machine

```
┌─────────────────────────────────────────────────────────────────┐
│                      RFID ACCESS SYSTEM                         │
│                   Complete State Machine                        │
└─────────────────────────────────────────────────────────────────┘

                            ┌──────────────┐
                            │  POWER ON    │
                            └──────┬───────┘
                                   │
                        ┌──────────▼───────────┐
                        │  INITIALIZE SYSTEM  │
                        │ ├─ Load users (SD   │
                        │ │   or EEPROM)      │
                        │ ├─ Initialize I2C   │
                        │ │   (LCD, RTC)      │
                        │ ├─ Init WiFi SoftAP │
                        │ ├─ Start web server │
                        │ └─ Clear LCD        │
                        └──────────┬──────────┘
                                   │
                        ┌──────────▼──────────────────┐
                        │   IDLE STATE                │
                        │ ├─ Relay OFF (GPIO 26=LOW)  │
                        │ ├─ LCD blank or welcome     │
                        │ ├─ Wait for RFID card       │
                        │ ├─ Monitor button (LOW)     │
                        │ └─ Service web requests     │
                        └──────┬──────────┬───────────┘
                               │          │
                ┌──────────────┘          └──────────────┐
                │                                        │
        Card Tapped                            Button Press
        (but not active)                       (idle state)
                │                                        │
                ▼                                        ▼
    ┌──────────────────────────┐         ┌────────────────────┐
    │ VALIDATE CARD / USER     │         │ IGNORED             │
    │ ├─ Read RFID UID         │         │ (no session active) │
    │ ├─ Search users vector   │         └────────────────────┘
    │ └─ Check if found        │
    └───┬──────────────┬───────┘
        │              │
    Found       Not Found
        │              │
        │              ▼
        │        ┌─────────────────┐
        │        │ INVALID CARD    │
        │        │ ├─ Log to Serial │
        │        │ ├─ Stay in IDLE  │
        │        │ └─ Wait 200ms    │
        │        └─────────────────┘
        │              │
        │              └──────────────────┐
        │                                 │
        ▼                                 │
┌─────────────────────────────────┐      │
│ START SESSION                   │      │
│ ├─ Set active = true            │      │
│ ├─ Copy user data               │      │
│ ├─ Capture RTC timestamp        │      │
│ ├─ Capture millis()             │      │
│ ├─ Set relay HIGH (GPIO 26)     │      │
│ ├─ LCD: "Machine: ON" + name    │      │
│ ├─ Serial: Session started      │      │
│ ├─ Save to SD + EEPROM          │      │
│ └─ Enter SESSION_ACTIVE state   │      │
└────────────┬────────────────────┘      │
             │                           │
    ┌────────▼───────────────────────────┘
    │
    │
    ▼
┌──────────────────────────────────┐
│ SESSION ACTIVE STATE             │
│ ├─ Relay ON (GPIO 26=HIGH)       │
│ ├─ LCD: "Machine: ON" + name     │
│ ├─ User operating machine        │
│ ├─ Duration accumulating         │
│ ├─ Monitor button (LOW = end)    │
│ ├─ Monitor RFID (ignore if tapped)
│ └─ Service web requests          │
└──────┬──────────────┬────────────┘
       │              │
  Button Press    Card Tapped
  (or timeout)    (already active)
       │              │
       │              ▼
       │         ┌──────────────┐
       │         │ IGNORE CARD  │
       │         │ (stay active)│
       │         └──────────────┘
       │
       ▼
┌─────────────────────────────────┐
│ END SESSION                      │
│ ├─ Read RTC end timestamp       │
│ ├─ Calculate duration:          │
│ │   durationSec = (now_ms -     │
│ │    start_ms) / 1000           │
│ ├─ Create log entry             │
│ ├─ Save to SD (/logs.csv)       │
│ ├─ Save to EEPROM (Preferences) │
│ ├─ Set relay LOW (GPIO 26)      │
│ ├─ LCD: "Machine: OFF" + dur    │
│ ├─ Serial: Session logged       │
│ ├─ Set active = false           │
│ └─ Return to IDLE               │
└────────────┬────────────────────┘
             │
    ┌────────▼────────────────────────┐
    │ DISPLAY RESULTS (2-3 sec)       │
    │ LCD shows: "Machine: OFF"       │
    │ + "Duration: XXXs"              │
    │ Then returns to IDLE            │
    └────────┬─────────────────────────┘
             │
    ┌────────▼────────────────────────┐
    │ BACK TO IDLE STATE              │
    │ Ready for next session          │
    └────────────────────────────────┘

```

---

## 2. Web Interface State Diagram

```
                    ┌─────────────────────────┐
                    │   WEB SERVER RUNNING    │
                    │   (192.168.4.1:80)      │
                    └────────────┬────────────┘
                                 │
         ┌───┬───┬───┬───┬───┬───┴───┬────┬────┐
         │   │   │   │   │   │       │    │    │
         ▼   ▼   ▼   ▼   ▼   ▼       ▼    ▼    ▼
        GET /   /users /logs  /add-user ...  /unlogs
         │
         ▼
    ┌─────────────────────────────────┐
    │ DASHBOARD (/)                   │
    │ ├─ View machine status          │
    │ ├─ Quick links to all features  │
    │ ├─ Current RTC time             │
    │ ├─ Active session info          │
    │ └─ [NEW] Delete Last Log link   │
    └─────────────────────────────────┘
         │
         ├─ Logs ──┐
         │         ▼
         │    ┌─────────────────────────┐
         │    │ LOGS PAGE (/logs)       │
         │    │ ├─ Display log entries  │
         │    │ ├─ Show last 400 entries│
         │    │ ├─ Export CSV link      │
         │    │ └─ Delete Last Log link │
         │    └──────┬──────────────────┘
         │           │
         │      Delete Link
         │           │
         │           ▼
         │    ┌─────────────────────────┐
         │    │ /UNLOGS (GET) FORM      │
         │    │ ├─ Confirmation message │
         │    │ ├─ Submit button        │
         │    │ │  onclick="confirm()"  │
         │    │ └─ Back links           │
         │    └──────┬──────────────────┘
         │           │
         │       Submit Click
         │           │
         │           ▼
         │    ┌─────────────────────────┐
         │    │ /UNLOGS (POST) PROCESS  │
         │    │ ├─ Read logs.csv        │
         │    │ ├─ Remove last line     │
         │    │ ├─ Rewrite file         │
         │    │ ├─ Decrement log_count  │
         │    │ ├─ Redirect 302         │
         │    │ └─ Return to /logs      │
         │    └──────┬──────────────────┘
         │           │
         │           ▼
         │    ┌─────────────────────────┐
         │    │ LOGS PAGE UPDATED       │
         │    │ (last entry removed)    │
         │    └─────────────────────────┘
         │
         └─ Users ──┐
         │          ▼
         │    ┌─────────────────────────┐
         │    │ USERS PAGE (/users)     │
         │    │ ├─ List all users       │
         │    │ ├─ UID, Roll, Name      │
         │    │ ├─ Add User link        │
         │    │ └─ Export CSV link      │
         │    └─────────────────────────┘
         │
         └─ Add User ──┐
                       ▼
                  ┌─────────────────────────┐
                  │ ADD USER FORM           │
                  │ (/add-user GET)         │
                  │ ├─ UID field            │
                  │ │  (auto-fills via      │
                  │ │   /api/read-uid poll) │
                  │ ├─ Roll field           │
                  │ ├─ Name field           │
                  │ ├─ Submit button        │
                  │ └─ JavaScript polling   │
                  │    (every 500ms)        │
                  └──────┬──────────────────┘
                         │
                    User enters
                    info + Submit
                         │
                         ▼
                  ┌─────────────────────────┐
                  │ ADD USER PROCESS        │
                  │ (/add-user POST)        │
                  │ ├─ Create User struct   │
                  │ ├─ Save to SD           │
                  │ ├─ Save to EEPROM       │
                  │ ├─ Add to users vector  │
                  │ ├─ Clear lastScannedUID │
                  │ └─ Redirect to /users   │
                  └──────┬──────────────────┘
                         │
                         ▼
                  ┌─────────────────────────┐
                  │ USER ADDED              │
                  │ View updated user list  │
                  └─────────────────────────┘

```

---

## 3. Session Lifecycle Timeline

```
TIME      EVENT                    RELAY       LCD              RTC/MILLIS
────────────────────────────────────────────────────────────────────────────
0ms       System Init              OFF         [Blank]          RTC Read

T0        IDLE Waiting             OFF         [Blank]          Incrementing

T0+500    Card Tapped              OFF         [Blank]          T0+500

T0+501    UID Read                 OFF         [Blank]          T0+501
          RFID: 12AB34CD

T0+502    User Found               OFF         [Blank]          T0+502
          Roll: CSE001
          Name: John Doe

T0+503    Session Started          ON ↑        Clear LCD        T0+503 SAVED

T0+504    LCD Updated              ON          "Machine: ON"    T0+504

T0+505    Name Printed             ON          "Machine: ON"    T0+505
                                               "CSE001      "

T0+506    Session Active           ON          "Machine: ON"    T0+506
                                               "CSE001      "

... Running ...

T0+30000  Still Running            ON          "Machine: ON"    T0+30000
          (30 seconds elapsed)     (no change) "CSE001      "   (duration = 30s)

T0+30100  Button Pressed           ON          "Machine: ON"    T0+30100
          GPIO 27 = LOW                        "CSE001      "

T0+30151  Debounce Complete        ON          "Machine: ON"    T0+30151
          (50ms debounce)                      "CSE001      "

T0+30152  End RTC Read             ON          "Machine: ON"    T0+30152 SAVED
          Duration = 30152 - 503
                  = 29649 ms
                  = 29.649 s ≈ 30s

T0+30153  Log Created              ON          "Machine: ON"    T0+30153
          Entry: [T0+503, T0+30152, 30]
                                               "CSE001      "

T0+30154  Saved to SD              ON          "Machine: ON"    T0+30154
          Saved to EEPROM          (still)     "CSE001      "

T0+30155  LCD Clear                OFF ↓       Clear LCD        T0+30155

T0+30156  Relay OFF                OFF         "Machine: OFF"   T0+30156

T0+30157  Duration Formatted       OFF         "Machine: OFF"   T0+30157

T0+30158  LCD Updated              OFF         "Duration: 30s"  T0+30158
                                               (padded)

... Display Results (2-3 sec) ...

T0+33000  LCD Still Shows          OFF         "Machine: OFF"   T0+33000
          (Optional auto-clear)                "Duration: 30s"

T0+33001  Return to IDLE           OFF         [Blank]          T0+33001

T0+33002  Ready for Next Card      OFF         [Blank]          T0+33002

────────────────────────────────────────────────────────────────────────────
```

---

## 4. Data Flow Diagram

```
                         HARDWARE INPUTS
                              │
          ┌─────────┬─────────┼─────────┬──────────┐
          │         │         │         │          │
          ▼         ▼         ▼         ▼          ▼
       RFID       RTC       Button    WiFi    SD Card
      (GPIO21)   (I2C)     (GPIO27)  (SoftAP) (GPIO4)
          │         │         │         │          │
          └────┬────┴────┬────┴────┬────┴──────┬───┘
               │         │         │           │
               ▼         ▼         ▼           ▼
          ┌─────────────────────────────────────────────┐
          │         ESP32 MAIN PROCESSOR                │
          │  ┌─────────────────────────────────────┐   │
          │  │ Main Loop (loop() function)         │   │
          │  │ ├─ Check RFID (every 50ms)         │   │
          │  │ ├─ Check Button (debounced)        │   │
          │  │ ├─ Read RTC (on start/end)         │   │
          │  │ ├─ Calculate Duration              │   │
          │  │ ├─ Handle Web Requests             │   │
          │  │ └─ Update Session State            │   │
          │  └─────────────────────────────────────┘   │
          │                                             │
          │  ┌─────────────────────────────────────┐   │
          │  │ MEMORY (RAM)                        │   │
          │  │ ├─ users vector (in-memory list)    │   │
          │  │ ├─ currentSession (struct)          │   │
          │  │ ├─ lastScannedUID (web polling)    │   │
          │  │ └─ Web server buffers               │   │
          │  └─────────────────────────────────────┘   │
          │                                             │
          │  ┌─────────────────────────────────────┐   │
          │  │ STORAGE (NVS / Preferences)         │   │
          │  │ ├─ users (backup)                   │   │
          │  │ ├─ logs (backup)                    │   │
          │  │ └─ log_count (metadata)             │   │
          │  └─────────────────────────────────────┘   │
          └──────┬──────────────────────┬──────────────┘
                 │                      │
                 ▼                      ▼
          ┌────────────────┐     ┌───────────────────────┐
          │  EEPROM        │     │  SD Card Storage      │
          │  (Backup)      │     │  (Primary)            │
          │                │     │                       │
          │ ├─ user_uid_0  │     │ ├─ /users.csv         │
          │ ├─ user_roll_0 │     │ │  (registered users) │
          │ ├─ user_name_0 │     │ │                     │
          │ ├─ log_count   │     │ ├─ /logs.csv          │
          │ ├─ log_start_0 │     │ │  (session history)  │
          │ ├─ log_end_0   │     │ │                     │
          │ └─ ...         │     │ └─ config files       │
          │                │     │                       │
          │ Size: ~3MB     │     │ Size: Variable        │
          │ Max Users: 800 │     │ (depends on activity) │
          │ Speed: 5-10ms  │     │ Speed: 10-50ms        │
          └────────────────┘     └───────────────────────┘
                 │                      │
                 └──────────┬───────────┘
                            │
                    ┌───────▼────────┐
                    │  System State  │
                    │  Persistent    │
                    │  & Recoverable │
                    └────────────────┘
                            │
                            ▼
          ┌────────────────────────────────────┐
          │  WEB INTERFACE OUTPUT              │
          │  (192.168.4.1 HTTP Server)         │
          │                                    │
          │  GET /               → Dashboard   │
          │  GET /logs           → Logs        │
          │  GET /unlogs         → Del Form    │
          │  POST /unlogs        → Del Process │
          │  GET /api/read-uid   → JSON UID    │
          │  ... etc ...                       │
          └────────────────────────────────────┘
                            │
                            ▼
                    ┌──────────────┐
                    │  Browser UI  │
                    │  (Client)    │
                    └──────────────┘
```

---

## 5. Unlogs Feature - Detailed Flow

```
                   /UNLOGS FEATURE FLOW
                   ───────────────────

START: User at /logs page
                    │
                    ▼
         Click "Delete Last Log"
         Link: href="/unlogs"
                    │
                    ▼
         Browser: GET /unlogs
                    │
         ┌──────────▼──────────┐
         │  handleUnlogsForm() │
         │  (GET handler)      │
         └──────────┬──────────┘
                    │
                    ▼
        Generate HTML form:
        ├─ <h1>Delete Last Log Entry</h1>
        ├─ Warning message
        ├─ <form method="POST">
        │  └─ <button type="submit"
        │        onclick="confirm(...)">
        │     Delete Last Log
        │     </button>
        ├─ Links to /logs and /
        └─ Send 200 OK + HTML
                    │
                    ▼
        Browser displays form
        User sees confirmation
                    │
    ┌───────────────┴──────────────┐
    │                              │
 YES (Click Button)            NO (Cancel)
    │                              │
    │                     └─ Stay on form
    │                        or go back
    │
    ▼
JavaScript confirm() dialog:
  "Are you sure you want to
   delete the last log entry?"
    │
    ├─ YES → POST /unlogs
    └─ NO → Cancel (no action)
    │
    ▼
Browser: POST /unlogs
(empty body, no form data needed)
    │
    ┌──────────────▼──────────────┐
    │  handleUnlogsPost()          │
    │  (POST handler)              │
    └──────────────┬──────────────┘
                   │
    ┌──────────────▼──────────────┐
    │ Check if /logs.csv exists   │
    └──────────────┬──────────────┘
                   │
        ┌──────────┴──────────┐
        │                     │
     EXISTS                 NOT EXISTS
        │                     │
        │                     ▼
        │              Send 200 OK
        │              "No logs"
        │              (early return)
        │
        ▼
    Open /logs.csv (READ)
    Load all lines into vector
        │
        ▼
    ┌─────────────────────────┐
    │ Read CSV:               │
    │ Header: line[0]         │
    │ Entry 1: line[1]        │
    │ Entry 2: line[2]        │
    │ ...                     │
    │ Entry N: line[N]        │
    │ (N = last entry)        │
    └─────────────────────────┘
        │
        ▼
    Check if size > 1
    (header + at least 1 entry)
        │
    ┌───┴───┐
    │       │
   YES     NO
    │       │
    │       ▼
    │    Send error
    │    "No logs to delete"
    │
    ▼
 Remove last element
 lines.pop_back()
    │
 ┌──▼──────────────────────┐
 │  Serial.println(         │
 │    "Deleted last log")   │
 └──┬───────────────────────┘
    │
    ▼
 Close file handle
 SD.remove(LOGS_FILE)
    │
    ▼
 Open /logs.csv (WRITE)
    │
    ┌───▼────────────────┐
    │ For each line:     │
    │  fw.println(line)  │
    │                    │
    │ Header written     │
    │ Entries 1..N-1     │
    │ Entry N skipped    │
    │ (deleted!)         │
    └───┬────────────────┘
        │
        ▼
    Close file
    SD /logs.csv updated
        │
        ▼
 ┌──────────────────────────┐
 │ preferences.begin(       │
 │   "rfid_sys", false)     │
 │                          │
 │ logCount = getUInt(      │
 │   "log_count")           │
 │                          │
 │ if logCount > 0:         │
 │   logCount--             │
 │   putUInt("log_count",   │
 │     logCount)            │
 │                          │
 │ preferences.end()        │
 └──────────────┬───────────┘
                │
    ┌───────────▼────────────┐
    │ EEPROM Updated:        │
    │ ├─ Decremented count   │
    │ ├─ Entry keys remain   │
    │ │  (could be cleaned)  │
    │ └─ Backed up on chip   │
    └───────────┬────────────┘
                │
                ▼
    ┌──────────────────────────┐
    │ HTTP Response:           │
    │ ├─ Status: 302           │
    │ │  (redirect)            │
    │ ├─ Location: /logs       │
    │ └─ Send                  │
    └──────────────┬───────────┘
                   │
                   ▼
    Browser receives 302 redirect
    Automatically navigates to /logs
                   │
                   ▼
    ┌──────────────────────────┐
    │ handleLogs()             │
    │ (GET /logs)              │
    │                          │
    │ Read /logs.csv           │
    │ Generate HTML table      │
    │ (last entry NOT shown)   │
    │                          │
    │ Send 200 OK + HTML       │
    └──────────────┬───────────┘
                   │
                   ▼
    Browser displays updated
    logs page
    (last entry gone!)
                   │
                   ▼
    ┌──────────────────────────┐
    │ USER SEES:               │
    │ ├─ Logs page            │
    │ ├─ Table updated        │
    │ ├─ Last entry removed   │
    │ ├─ Count decremented    │
    │ └─ Both SD + EEPROM     │
    │    updated              │
    └──────────────────────────┘

END: Last log entry successfully deleted
```

---

## 6. Button Press Sequence

```
                    BUTTON PRESS HANDLING
                    (GPIO 27 low = button pressed)

                         LOOP START
                             │
                             ▼
                    ┌────────────────────┐
                    │ Check GPIO 27      │
                    │ Read pin state     │
                    └────────────┬───────┘
                                 │
                                 ▼
                    ┌────────────────────────┐
                    │ Current reading = ? |
                    │ (store in variable)    │
                    └────────────┬───────────┘
                                 │
              ┌──────────────────┼──────────────────┐
              │                  │                  │
           HIGH (not pressed)   LOW (pressed)      SAME
              │                  │                  │
              ▼                  ▼                  ▼
         ┌─────────────┐  ┌──────────────┐  ┌────────────┐
         │ Reset       │  │ Inc counter  │  │ (reading   │
         │ debounce    │  │ if > 1       │  │  maybe     │
         │ counter     │  │ stable       │  │  settling) │
         │ lastStable= │  │ Start timer  │  │            │
         │ current     │  └──────┬───────┘  └────────────┘
         │ (debounce)  │         │
         └──────┬──────┘  ┌──────▼────────────┐
                │         │ Wait for 50ms     │
                │         │ debounce time     │
                │         │ (millis check)    │
                │         └──────┬────────────┘
                │                │
                │       50ms elapsed?
                │                │
                │         ┌──────┴────────┐
                │         │               │
                │        NO              YES
                │         │               │
                │         │               ▼
                │         │         ┌──────────────┐
                │         │         │ Check state: │
                │         │         │ if LOW &&    │
                │         │         │  session     │
                │         │         │  active      │
                │         │         └──────┬───────┘
                │         │                │
                │         └────────┬───────┘
                │                  │
                │       ┌──────────┴─────────┐
                │       │                    │
                │      YES (Button pressed  NO (Not pressed
                │       & session active)    or not active)
                │       │                    │
                │       ▼                    │
                │    ┌──────────────────────┤
                │    │ Read RTC end time    │
                │    │ rtcRead(endTime)     │
                │    └────────┬─────────────┤
                │             │             │
                │             ▼             │
                │    ┌─────────────────────┤
                │    │ Calculate duration: │
                │    │ durationSec =       │
                │    │  (millis() -        │
                │    │   startMillis) /    │
                │    │  1000UL             │
                │    └────────┬────────────┤
                │             │            │
                │             ▼            │
                │    ┌─────────────────────┤
                │    │ Format timestamps   │
                │    │ startStr,endStr     │
                │    │ append to SD        │
                │    │ append to EEPROM    │
                │    └────────┬────────────┤
                │             │            │
                │             ▼            │
                │    ┌─────────────────────┤
                │    │ Clear session data  │
                │    │ active = false      │
                │    │ uid = ""            │
                │    │ roll = ""           │
                │    │ name = ""           │
                │    └────────┬────────────┤
                │             │            │
                │             ▼            │
                │    ┌─────────────────────┤
                │    │ LCD Display:        │
                │    │ ├─ Clear            │
                │    │ ├─ Line 1:          │
                │    │ │  "Machine: OFF"   │
                │    │ ├─ Line 2:          │
                │    │ │  "Duration: X s"  │
                │    │ │  (padded to 16)   │
                │    │ └─ Print all        │
                │    └────────┬────────────┤
                │             │            │
                │             ▼            │
                │    ┌─────────────────────┤
                │    │ machineOff()        │
                │    │ GPIO 26 = LOW       │
                │    │ Relay: OFF          │
                │    └────────┬────────────┤
                │             │            │
                │             ▼            │
                │    ┌─────────────────────┤
                │    │ delay(300ms)        │
                │    │ Prevent bounce      │
                │    │ retrigger           │
                │    └────────┬────────────┤
                │             │            │
    Back to LOOP              └────────────┤
         │                                │
         └────────────────────────────────┘
                        │
                        ▼
                    LOOP END
                (next iteration)
```

---

## 7. Complete Hardware Sequence

```
HARDWARE STATE TRANSITIONS

Time    GPIO26(Relay)    GPIO27(Button)    LCD Display          RTC/EEPROM
─────────────────────────────────────────────────────────────────────────────
0ms     LOW              HIGH              [Blank]              Init
        (OFF)            (not pressed)     (idle)               Read

500ms   LOW              HIGH              [Blank]              Incrementing
        (OFF)            (not pressed)     (idle)               (monitor)

502ms   ▲ RISE           HIGH              "Machine: ON"        T0+502
        └─→ HIGH         (not pressed)     "CSE001      "       [SAVED]
        (ON)                               (LCD updated)        

505ms   HIGH             HIGH              "Machine: ON"        T0+505
        (ON)             (not pressed)     "CSE001      "       Still active
        (steady)                           (no change)          

15000   HIGH             HIGH              "Machine: ON"        T0+15000
(idle)  (ON)             (not pressed)     "CSE001      "       (continuous)
        (steady)                           (no change)          

15050   HIGH             HIGH              "Machine: ON"        Still active
        (ON)             (not pressed)     "CSE001      "
                                           (no change)

15051   HIGH             ▼ FALL            "Machine: ON"        T0+15051
        (ON)             LOW               "CSE001      "       (button press
        (steady)         (pressed)         (no change yet)      detected)
                         (debounce)

15101   HIGH             LOW               "Machine: OFF"       T0+15101
        │                (pressed)         "Duration: 15s"      [END SAVED]
        │ SET TO LOW     (stable)          (LCD updated)
        ▼                                  (clear + print)

15102   LOW              LOW               "Duration: 15s"      Duration
        (OFF)            (pressed)         "Machine: OFF"       logged
        (active)         (still)           (complete)           (SD + EEPROM)

15103   LOW              ▲ RISE            "Machine: OFF"       State reset
        (OFF)            HIGH              "Duration: 15s"      Session
        (steady)         (released)        (display 2-3s)       inactive
                         (debounce)

18000   LOW              HIGH              [Blank]              Ready for
        (OFF)            (not pressed)     (idle)               next
        (ready)          (ready)           (welcome)            session

─────────────────────────────────────────────────────────────────────────────

KEY TRANSITIONS:

GPIO 26 (Relay):
  • LOW (OFF)  → Machine NOT powered
  • HIGH (ON)  → Machine powered up

GPIO 27 (Button):
  • HIGH = Not pressed (pulled up internally or externally)
  • LOW  = Pressed to ground
  • Debounce = 50ms minimum stable LOW to register

LCD Display:
  • Updates on session start (immediately)
  • Updates on session end (immediately)
  • Displays duration for ~2-3 seconds
  • Can then be cleared for next session

RTC/EEPROM:
  • Session start time captured when user tapped
  • Session end time captured when button pressed
  • Both timestamps saved (persistent storage)
  • Duration = end_time - start_time
```

---

**Flowchart Version:** 1.0  
**Last Updated:** January 2025  
**System:** RFID Access Control v1.0 (ESP32)

