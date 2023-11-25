# saturn-updater
An installer/updater for [Saturn](https://github.com/Llennpie/Saturn)
## Building
### Windows
1. Download and install [MSYS2](https://www.msys2.org/)
2. Run the MINGW64 shell
3. Install dependencies
   ```
   pacman -Sy git make gcc mingw-w64-x86_64-SDL2
   ```

4. Continue from Linux step 2
### Linux
1. Install dependencies
   
   1.a Debian-based distros (Ubuntu, Mint, ...)
   ```
   sudo apt -y install git make gcc libsdl2-dev libcurl4-gnutls-dev
   ```
   
   1.b Arch-based distros (Manjaro, EndeavourOS, ...)
   ```
   sudo pacman -Sy git make gcc sdl2 libcurl-gnutls
   ```
  
2. Clone the repository
   ```
   git clone https://github.com/Dominicentek/saturn-updater
   cd saturn-updater
   ```
  
3. Run `make` to compile
