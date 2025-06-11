# superlotis_camera_control
Sophia camera control for Superlotis


# Setting up Centos7:
Note: PICam only works on Centos7. Apparently, some people have been able to get it working on other operating systems, but they are not officially supported. 
Centos7 installation (gnome desktop, x86_64):


# Applications to install on centos7:
1. Anydesk 
2. C++
   - Install cmake
4. VScode
  - Need GLIBCXX version 3.4.25, and GLIBC 2.28 as discussed in vscode requirements. To get available GLIBCXX versions: ‘strings /usr/lib64/libstdc++.so.6 | grep GLIBCXX’, and current GLIBC version: ‘rpm -q glibc’
  - It is necessary to install a previous version of vscode, listed [here](https://code.visualstudio.com/docs/supporting/faq#_previous-release-versions). I used version 1.25 (june 2018).
  - [Also, it is *necessary* to opt out of vscode auto updates.](https://code.visualstudio.com/docs/supporting/FAQ#:~:text=You%20can%20install%20a%20previous,a%20specific%20release%20notes%20page)
4. ds9 
5. Chromium
  - centos7 is no longer supported by chromium, make [changes outlined here](https://serverfault.com/questions/904304/could-not-resolve-host-mirrorlist-centos-org-centos-7)
6. Firefox

# Picam Installation:
1. Downloaded picam_sdk.run file
2. Make picam_sdk executable, then executed it (command line), discussed [here](https://askubuntu.com/questions/18747/how-do-i-install-run-files)
3. Afters file runs, configure the firewall to allow all traffic through the ethernet adaptor



