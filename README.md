# Tarfful
C++11 implementation of microtar library

## Basic usage
#### Writing file(s) directory(ies) to the archive:
```
  Tarfful::Tar tar(archiveName); /* Create instance of Tar class passing desired archive name to the constructor 
                                 If the archive does not yet exist, it will be created, otherwise it will be opened for adding files. */
  tar.Archive(pathToBeArchived); /* Call Archive method of tar object passing path to it. 
                                In case if path indicated directory - it's whole content will be archived recursively */
```

#### Extracting file(s) from the archive:
```
  Tarfful::Tar tar(archiveName); // Create instance of Tar class passing existing archive name to the constructor
  tar.Extract(pathToBeExtracted); // Call Extract method of tar object passing the path to it. If found - it would be extracted preserving full path
```

#### Extracting all files from the archive:
```
  Tarfful::Tar tar(archiveName); // Create instance of Tar class passing existing archive name to the constructor
  tar.ExtractAll(); // Call ExtractAll method of tar object. It will extract all archived files preserving it's full path
```

For all other functionality you'll have to wait for another release ~~which may never happen~~ or, which better - DIY -> Contribute. 
[Original microtar](https://github.com/rxi/microtar) repository README along with exploring source code of this project may help you in your endeavors.

## Q&A

#### Verbose RW-operations
Compile with `verbose` flag

#### Refactoring/UStar format/etc.
Maybe tomorrow, maybe never

#### Speedtest
Tested only once on old rusty 7200 RPM HDD with 🤡430M Node.js🤡 project so doesn't claim to be exemplary
```
GNU tar: tar cf tar-test.tar api/  0.23s user 2.27s system 1% cpu 2:24.82 total
Tarfful: ./tarfful c tarfful-test.tar api  29.01s user 2.72s system 22% cpu 2:22.08 total
```
