---
title: Python API 0.2
category: info
tags: python
---

The API contains global functions for network editing, file management, module state modification, and data object extraction and insertion.

Some functions are not available for execution within the `InterfaceWithPython` module, since they are not useful in that context and can cause instability. They are marked with a (*).

## Global functions

### Network editing
* `scirun_add_module("ModuleName")`
  * Adds a new instance of a module to the network. Returns the module ID as a string. (*)
* `scirun_remove_module("ModuleID")`
  * Removes the module specified by the ID string. (*)
* `scirun_execute_all()`
  * Executes the entire current network. (*)
* `scirun_module_ids()`
  * Returns a list of all module ID strings in the current network, sorted by module creation time.
* `scirun_connect_modules("ModuleIDFrom", fromIndex, "ModuleIDTo", toIndex)`
  * Connects ports between two modules by index. From is the output, To is the input. (*)
* `scirun_disconnect_modules("ModuleIDFrom", fromIndex, "ModuleIDTo", toIndex)`
  * Used to disconnect two modules, with the same syntax as connecting. (*)

### Network file management
* `scirun_save_network("Filename.srn5")`
  * Saves the current network file. (*)
* `scirun_load_network("Filename.srn5")`
  * Loads the specified network file. (*)
* `scirun_import_network("Filename.srn")`
  * Imports the specified v4 network file. (*)
* `scirun_quit_after_execute()`
  * Enables quitting SCIRun after the next network execution is complete. (*)
* `scirun_force_quit()`
  * Quits SCIRun immediately. (*)

### Module state editing
* `scirun_get_module_state("ModuleID", "StateVariableName")`
  * Returns the value of a the specified module state variable.
* `scirun_set_module_state("ModuleID", "StateVariableName", value)`
  * Sets the specified module state variable's value.
* `scirun_dump_module_state("ModuleID")`
  * Returns a dictionary with the entire state of the specified module.
* `scirun_get_module_transient_state("ModuleID", "StateVariableName")`
  * Returns the value of a the specified module transient state variable.
* `scirun_set_module_transient_state("ModuleID", "StateVariableName", value)`
  * Sets the specified module transient state variable's value. Used to pass data values (strings, matrices, fields [coming soon]) back to modules.

### Module/Datatype input
* `scirun_get_module_input_type("ModuleID", portIndex)`
  * Returns the type of the input data object on the specified port.
* `scirun_get_module_input_object("ModuleID", "PortName")`
  * Returns a special `PyDatatype` wrapper object containing a copy of the data on the specified input port, by name.
* `scirun_get_module_input_value("ModuleID", "PortName")`
  * Returns a Python object containing a copy of the data on the specified input port.
* `scirun_get_module_input_object_by_index("ModuleID", portIndex)`
  * Returns a special `PyDatatype` wrapper object containing a copy of the data on the specified input port, by port index.
* `scirun_get_module_input_value_by_index("ModuleID", portIndex)`
  * Returns a Python object containing a copy of the data on the specified input port, by index.

### InterfaceWithPython special syntax
* This module lets you set input/output variable names in the module UI. Once this is done (or by using the defaults), one can use assignment syntax to read input data and send output data.
  * Examples
     * `pythonOutput = "hello"` will send a string to the output port associated with `pythonOutput`
     * `field = inputField1` will extract a field object from the input port associated with `inputField1`
     * `outputString1 = "string concat " + inputString1` Input/Output can be combined on the same line.
  * Note: for output variable assignment, make sure to include spaces around the `=`.

## Installing packages
* If there is an installation of the packages with other python builds on the machine, the system path can be used to use those packages.  

There are many ways to install packages in python. pip is an easy way that is usually included with python.

### Installing pip
If pip is not installed for the scirun installation of python, follow these [directions](https://pip.pypa.io/en/stable/installing/).

Be sure to use the python in

* `"scirun_root"/bin/Externals/Install/Python_external/bin/`

or in the Mac OS X app bundle in

* `SCIRun.app/Contents/Frameworks/Python.framework/Versions/3.4/lib/python3.4/site-packages`.

### Installing numpy, scipy, and other major packages
Make sure that pip is installed.

Run in the terminal:

```
cd "scirun_root"/bin/Externals/Install/Python_external/bin/
./python3 -m pip install numpy
```

### Installing Matlab engine for python in SCIRun
Run in the terminal:

```
cd "matlab_root"/extern/engines/python
"scirun_root"/bin/Externals/Install/Python_external/bin/python3.4 setup.py build --build-base="builddir" install --prefix="installdir"
```

Full installation instructions are located [on the mathworks site](http://www.mathworks.com/help/matlab/matlab_external/install-the-matlab-engine-for-python.html).
  
## Matlab engine in SCIRun 5 (through python)

In SCIRun 5, Matlab code and functions can be run using the matlab engine for python in the python console or python interface.  To do so, make sure that the matlab engine is installed (previous section).
Full documentation can be found [here](http://www.mathworks.com/help/matlab/matlab-engine-for-python.html).
