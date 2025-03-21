Go to "https://github.com/assimp/assimp"
Click Code > Download ZIP
Extract the files into a directory, e.g., C:/Assimp/



Download CMake
Open CMake GUI
Set:
Source Code Directory: C:/Assimp/
Build Directory: C:/Assimp/build
Click Configure.
Select Visual Studio 2019 or 2022.
Click Generate.

a file named assimp-vc143-mt.dll is in C:\Assimp\build\bin\Release 
copy it to YourProject/ThirdParty/Assimp/bin/ and YourProject/Binaries/Win64/

a file named assimp-vc143-mt is in  C:/Assimp/build/lib/Release/
copy it to YourProject/ThirdParty/Assimp/lib/

copy include folder from C:\Assimp\
copy it to  YourProject/ThirdParty/Assimp/




YourProject/
├── ThirdParty/
│   ├── Assimp/
│   │   ├── include/   → (Copy Assimp header files here)
│   │   ├── lib/       → (Copy `assimp-vc143-mt.lib` here)
│   │   ├── bin/       → (Copy `assimp-vc143-mt.dll` here)
│   │
├── Binaries/
│   ├── Win64/         → (Copy `assimp-vc143-mt.dll` here too)

