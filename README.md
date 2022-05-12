# Tarfful
C++11 implementation of microtar library

## Basic usage
#### Writing file(s) directory(ies) to the archive:
```
   /* Create instance of Tar class passing desired archive name to the constructor 
   If the archive does not yet exist, it will be created, otherwise it will be opened for adding files. */
   Tarfful::Tar tar(archiveName);
   
   /* Call Archive method of tar object passing path to it. 
   In case if path indicated directory - it's whole content will be archived recursively */
   tar.Archive(pathToBeArchived); 
```

#### Extracting file(s) from the archive:
```
  // Create instance of Tar class passing existing archive name to the constructor
  Tarfful::Tar tar(archiveName);
  
  // Call Extract method of tar object passing the path to it. If found - it would be extracted preserving full path
  tar.Extract(pathToBeExtracted); 
```

#### Extracting all files from the archive:
```
  // Create instance of Tar class passing existing archive name to the constructor
  Tarfful::Tar tar(archiveName); 
  
  // Call ExtractAll method of tar object. It will extract all archived files preserving it's full path
  tar.ExtractAll(); 
```

For all other functionality you'll have to wait for another release ~~which may never happen~~ or, which better - DIY -> Contribute. 
[Original microtar](https://github.com/rxi/microtar) repository README along with exploring source code of this project may help you in your endeavors.

## Q&A

#### Verbose RW-operations
Compile with `verbose` flag

#### Refactoring/UStar format/etc.
Maybe tomorrow, maybe never
