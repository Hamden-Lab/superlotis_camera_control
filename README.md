# superlotis_camera_control
Sophia camera control for Superlotis

## Installation/requirements

# Setting up computer:
Note: PICam only works on Centos7. Apparently, some people have been able to get it working on other operating systems, but they are not officially supported. 
# Centos7 installation (gnome desktop, x86_64):

# Applications to install on centos7:
1. Anydesk
   - (Removing ads on Anydesk)[https://www.reddit.com/r/AnyDesk/comments/11q6v5j/personal_use_being_annoyed_by_forcedwait_popups/]
3. C++
   - Install cmake
4. VScode
  - Need GLIBCXX version 3.4.25, and GLIBC 2.28 as discussed in vscode requirements. To get available GLIBCXX versions: ‘strings /usr/lib64/libstdc++.so.6 | grep GLIBCXX’, and current GLIBC version: ‘rpm -q glibc’
  - It is necessary to install a previous version of vscode, listed [here](https://code.visualstudio.com/docs/supporting/faq#_previous-release-versions). I used version 1.25 (june 2018), [here](https://code.visualstudio.com/docs/supporting/faq#_previous-release-versions)
  - [Also, it is *necessary* to opt out of vscode auto updates.](https://code.visualstudio.com/docs/supporting/FAQ#:~:text=You%20can%20install%20a%20previous,a%20specific%20release%20notes%20page)
4. ds9
   - run ./ds9 in appropriate folder
5. Chromium
  - centos7 is no longer supported by chromium, make [changes outlined here](https://serverfault.com/questions/904304/could-not-resolve-host-mirrorlist-centos-org-centos-7)
6. Firefox

# Picam Installation:
1. Downloaded picam_sdk.run file
2. Make picam_sdk executable, then executed it (command line), discussed [here](https://askubuntu.com/questions/18747/how-do-i-install-run-files)
3. Afters file runs, [configure the firewall to allow all traffic through the ethernet adaptor](https://docs.redhat.com/en/documentation/Red_Hat_Enterprise_Linux/7/html/Security_Guide/sec-Using_Firewalls.html#sec-Getting_started_with_firewalld)

To run vscode, type 'code' into the command line

# Camera control files
1. camera.cpp: all camera control commands
   - camera.h: access camera.cpp variables in server.c
2. server.c: defines commands that you can use in another computer, through socket connection
   - server.h: connects server.c, camserver.c, camera.cpp
3. camserver.c: runs server.c in loop
   - Note: server.c and camserver.c can be merged - this may be a good option in the future
4. makefile: compiles and runs camera.cpp, server.c, camserver.c
   - generates ./bin/camserver_cit (executable)
