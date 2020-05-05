# Project 6: File Systems #


## Overview ##
The purpose of this project was to build a simple file system from scratch.  The file system supports calls to format, mount, inode create/delete, and read/write.  Instead of a spinning platter, the program uses a disk emulator in the form of an external file.  The file system attempts to mirror the structure found in Remzi and Andrea Arpaci-Dusseau's *Operating Systems: Three Easy Pieces*, utilizing basic bitmap mechanics and proper block handling.

## Contributions ##
All files except fs.c were provided prior to beginning the project. Within fs.c, structs, symbolic constants, and superblock debug information was also provided.

Other Contributors: Isobel Murrer, Bridget Lumb, Nicole Warren

## Caution ##
Due to limitations on which files were allowed to be edited for this project, memory is allocated for the free block bitmap when the file system is mounted, but is not freed when the program completes.  Although this is commonly considered to be bad practice, our group intentionally made this decision in order to adhere to project restrictions.

## Additional Resources ##
Project instructions and further information can be found [here](https://sakailogin.nd.edu/access/content/group/SP20-CSE-34341-02/projects/Project_6.html "Project 6: File Systems")
*At the time of push, the link above requires access to a Notre Dame account.  The project instructions and information may be provided upon request.* 