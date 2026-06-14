Assignment 1

This program reads commands from stdin and performs file operations
using low level sytem calls.

----Commands----

1. get
   Input format:
       get
       filename

   The program opens the specified file and writes its contents to standard output.

2. set
   Input format:
       set
       filename
       length
       content

   The program writes exactly length bytes of content into the specified file.
   If successful, it prints "OK" to standard output.

Error Handling:

If the command format is invalid, the program prints:
    Invalid Command

If a file operation fails, the program prints:
    Operation Failed
