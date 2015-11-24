# lockchart

Utility for verifying OS X file access synchronisation primitives against local and remote filesystems.

# Locking on local filesystem

~~~~
$ lockchart file file
           \     sharemode 
sharemode   \    Attempted mode
Current mode \   exclusive | shared        | none
                 R   W  RW |   R   W  RW   | R   W  RW
exclusive    R   x   x   x     x   x   x     .   .   .   
             W   x   x   x     x   x   x     .   .   .   
             RW  x   x   x     x   x   x     .   .   .   
----------
shared       R   x   x   x     .   .   .     .   .   .   
             W   x   x   x     .   .   .     .   .   .   
             RW  x   x   x     .   .   .     .   .   .   
----------
none         R   .   .   .     .   .   .     .   .   .   
             W   .   .   .     .   .   .     .   .   .   
             RW  .   .   .     .   .   .     .   .   .   

x = open failed
. = open succeeded
~~~~


# Locking on mounted SMB filesytem
~~~~
$ lockchart /Volumes/smb/file1 /Volumes/smb/file
           \     sharemode 
sharemode   \    Attempted mode
Current mode \   exclusive   | shared      | none
                 R   W  RW   | R   W  RW   | R   W  RW
exclusive    R   x   x   x     x   x   x     x   x   x   
             W   x   x   x     x   x   x     x   x   x   
             RW  x   x   x     x   x   x     x   x   x   
----------
shared       R   x   x   x     .   x   x     .   x   x   
             W   x   x   x     x   x   x     .   x   x   
             RW  x   x   x     x   x   x     .   x   x   
----------
none         R   x   x   x     .   .   .     .   .   .   
             W   x   x   x     x   x   x     .   .   .   
             RW  x   x   x     x   x   x     .   .   .   

x = open failed
. = open succeeded
~~~~
