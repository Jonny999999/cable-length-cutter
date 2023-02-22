# Links
## connection plan
See [connection-plan.drawio.pdf](connection-plan.drawio.pdf)  

## flowchart firmware
See [function-diagram.drawio.pdf](function-diagram.drawio.pdf)  



# Installation
### Install esp-idf
Currently using ESP-IDF **v4.4.4-148** (5.x did not work due to breaking changes and CMAKE issues)
1. download esp-idf: 
```
clone the esp-idf repository from github to /opt
```
2. checkout version needed:
```
git checkout release/4.4
```
3. run install script e.g.
```
/opt/esp-idf/install.sh
```

### Clone this repo
```
git clone git@github.com:Jonny999999/cable-length-cutter.git
```



# Compilation
### Set up environment
```bash
source /opt/esp-idf/export.sh
```
(run once in terminal)

### Compile
```bash
idf.py build
#or
idf.py build flash monitor
```




# Usage
[WIP...]





# Components
## rotary encoder LPD3806-600BM-G5-24C
    - Pulses: 600 p/r (Single-phase 600 pulses /R,Two phase 4 frequency doubling to 2400 pulses)
    - Power source: DC5-24V
    - Shaft: 6*13mm/0.23*0.51"
    - Size: 38*35.5mm/1.49*1.39"
    - Output :AB 2phase output rectangular orthogonal pulse circuit, the output for the NPN open collector output type
    - Maximum mechanical speed: 5000 R / min
    - Response frequency: 0-20KHz
    - Cable length: 1.5 meter
    - size: http://domoticx.com/wp-content/uploads/2020/05/LPD3806-afmetingen.jpg
    - Wires: Green = A phase, white = B phase, red = Vcc power +, black = V0

