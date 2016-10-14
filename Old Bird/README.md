All `.exe`, `.c`, and `.mdl` files in this directory and (recursively) its subdirectories are copyright [Old Bird, Inc.](www.oldbird.org) and are included here with permission.

Tseep-r.exe and Thrush-r.exe are the Old Bird Tseep and Thrush detector Windows executables, downloaded from http://oldbird.org/tseep.htm and http://oldbird.org/Thrush.htm, respectively, in April 2015.

The source code for each of the detectors comprises two parts, a Simulink MDL file and some C files. An MDL file (MDL is short for *model*) is a text file that describes the blocks of a Simulink model, their parameter values, and their interconnections. Each C file implements one custom Simulink block that can be used in the model of an MDL file.

The precise MDL files from which the Old Bird Tseep and Thrush detectors were built are lost, but some MDL files that are believed to be similar to those files are in the `MDL` directory. The spreadsheet `MDL File Parameter Values.ods` summarizes the parameter values of the MDL files, and includes some notes regarding what those values tell us about the values of the Tseep and Thrush detectors.
