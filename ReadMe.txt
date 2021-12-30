Owners : Pankaj Kumar
usernames : pankajkumar
netid: pkumar58

Owners : Vyom Tayal
Username:vyom
Netid : vtayal


Purpose of the program
------------------------
The following program implements the assignment "p4" for the "Intro to OS" course by Prof. Remzi. 
The program is aimed at developing a working distributed file server.

Comments on Runs :
---------------
We have tried running multiple times and always gets (26/27) testcases.
The failing testcase is "persist2". 
Also, the last test "free" takes some more time(2-3 mins) to complete in general.

Run steps : 
--------------
Compilations command : 

make clean
make

Running tests : ~cs537-1/tests/p4/runtests -c

Files Included:
-------------------
client.c ( just for compilation)
helper.h
Makefile
mfs.c
mfs.h
partners.txt
ReadMe.txt
server.c
udp.c
udp.h

Short Summary 
-------------------

The file server is built as a stand-alone UDP-based server. It waits for a message and then process the message as need be, 
replying to the given client. The file server will store all of its data in an on-disk file which will be referred to as 
the file system image. 
This image contains the on-disk representation of your data structures; used system calls to access it: open(), read(), write(), lseek(), 
close(), fsync(), pread(), pwrite().
To access the file server, we have a client library. The interface and structs that the library supports is defined in mfs.h and helper.h. 


Functions implemented : 
MFS_Init()
MFS_Lookup()
MFS_Stat()
MFS_Write()
MFS_Read()
MFS_Creat()
MFS_Unlink()
MFS_Shutdown()

