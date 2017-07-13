# GZIP Accelerator Repository

## Overview
This repository contains all applications that can be ran using the GZIP Accelerator provided.
GZIP FPGA Accelerator Powered by Accelize & CAST GZip IP 
Designed with QuickPlay, Protected by Algodone Embedded DRM 


## Folders organization
|--- **applications**  
|&emsp;&emsp;|---\+ gzip/
|&emsp;&emsp;|---\+ sample_files/
|--- **bitstream**  
|&emsp;&emsp;|---\+ A10PL4/  
|&emsp;&emsp;|---\+ XpressGXA10/  

## Folders content description
|Folder Name|Content Description|
|------------------|----------------------------------------------------------------
|applications      |Software Applications to use with provided bitstream            
|bitstream         |FPGA Bitstreams + JSON File                                |

## How to use it
* Clone/Fork the repository
* Program your FPGA board with the providen bitstream, reboot your platform
* Retrieve your Board ID:
  * Open a terminal in folder appications/${app_name}/Release
  * Run command : "make clean all"
  * Run command : "./${app_name}
  * An error message will be printed, explaining that you miss a license file
  * Gather the Board ID displayed in the error message
* Buy license file:
  * Connect to <https://quickstore.quickplay.io/>
  * >>> TO BE COMPLETED
  * copy the license file on your platform
  * edit the license search path in the test application code
* Run your application  


