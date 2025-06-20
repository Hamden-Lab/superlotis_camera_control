1. start the software: scheduler server will send commands to this client...which sends commands to the camera software
- start the software: define path for sicam_shell
- set ip addresss + port for socket server
- set ip addresss + port for status server
2. create log
- set path of xml file for logging
- generate xml file for logging
3. define additional variables
- debug
- str
- err_str
- time variables: now, year/month/day/hr/min/sec
4. define exposure variables
- start_exposure
- end_exposure
- expose
5. read commands from scheduler script
6. get output of "status" command (status commands from status script)
7. spawn the "status" script
- start the status script
- when script starts: make note of the time that the script was started & set counter to 0
8. set camera exposing status to idle
9. spawn the sicam shell
- analoagous to running the executable in a c++ script??
10. initialize camera
- if initialize doesn't work, send email to Peter
- read in config file (?)
11. generate test files
- turn on cooler, get/set temp
- run this loop 5 times:
    - set exptime
    - run exposure
    - save as fits file
    - sleep for 5 sec (if necessary)
12. execute scheduler commands
- loop through commands
- query status server
    - get timeleft
    - get temp
    - get pressure
    - get shutter
- read commands from scheduler
- execute commands from scheduler
- create filename for fits file
- update fits file header (header is already populated - update regularily with additional information)


