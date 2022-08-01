# Tarfful
C++11 tar library

## Basic usage
Create instance of Tar class passing desired archive name to the constructor 
   If the archive does not yet exist, it will be created, otherwise it will be opened for adding files.
   ```
   Tarfful::Tar tar(archiveName);
   ```

#### Writing file(s) directory(ies) to the archive:  
   Use Archive method of tar object passing path to be archived.
   ```
   tar.Archive(path); 
   ```

#### Extracting file(s) from the archive: 
  Call Extract method of tar object passing the path to it. If found - it would be extracted preserving full path
  ```
  tar.Extract(pathToBeExtracted); 
  ```

#### Extracting all files from the archive:
  
  Call ExtractAll method of tar object. It will extract all archived files preserving it's full path
  ```
  tar.ExtractAll(); 
  ```
