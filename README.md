# cranium-esp32-cluster
A project for creating small pseudo-clusters of esp32 mcu, particularly using M5-stack esp32-core and Atom lites.

This is nothing more than an experiment involving cheap hardware and how far can it be pushed.

Warning: This project has been developed under the heavy use of free tier AI. 

The Hardware:

1 M5stack esp32-core
6 M5stack Atom lite
1 PA-hub 
assorted cables

The software:
The pseudo-cluster uses a wired and wireless communication methods. The wired portion acts as a control interface between the nodes and coordinator. The  wireless acts as the means to send data to the nodes. The software currently supports a limited number of commands reboot, reset wireless and certain states of the nodes such as free ram and storage etc. 
