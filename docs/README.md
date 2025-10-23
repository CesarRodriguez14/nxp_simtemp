## Implementation
This kernel device driver was built and tested on Ubuntu 16.04 and Ubuntu 20.04. It was also tested on an embedded Linux device: Quectel's Smart Module SC206EM running a custom image based on Yocto distribution dunfell. The custom layer added to this image is included in the folder `meta-nxpsimtemp`. The following steps are required for Linux desktop distributions. For SC206EM, build steps may be omitted.

## Build steps
To build kernel module and the CLI, execute:

    cd scripts 
    sudo ./build.sh

Or you may execute for the kernel module :

    cd kernel
    make clean
    make
And for the CLI:

    cd user/cli
    make clean
    make
## Run step
After the build steps, to load the kernel module, execute:

    cd kernel
    sudo insmod nxp_simtemp_drv.ko 
Before executing the CLI, **the kernel module must be loaded**. To get advice of CLI usage, execute:

     cd user/cli
     sudo ./cli_nxp_simtemp -h
     
The following help menu will be displayed:

	    nxp_simtemp CLI help menu
	**********************************************************
	*                                                        *
	* Simulation of a temperature sensor in a 20°C mean room *
	*                                                        *
	**********************************************************
	Correct usage: nxp_simtemp_cli [options]
	Options:
	-s              sampling_rate_ms limits: [0.05, 10000]
	-m              mode [d,n,r] (default, noisy, ramp)
	-t              temp_threshold_mC limits: [-50000, 100000]
	-h/--help       This help menu
	Example usage: nxp_simtemp_cli -s200 -mr -t20000
	If no options are provided, default parameters will be applied.

To execute the demo with the DT parameters or the default values of the kernel module (due to no binding), just execute:

     cd user/cli
     sudo ./cli_nxp_simtemp
The CLI demo will end with `Ctrl+C`

After finishing the CLI demo, the kernel module can be unloaded with:

    rmmod nxp_simtemp_drv

To load the kernel module, run a 1 minute CLI demo, and unload the module, execute:

    cd scripts 
    sudo ./run_demo.sh
The simulation parameters can be modified in the `run_demo.sh` in:

    sampling_rate_ms=100
    mode=d
    threshold_mC=20000
These are the default parameters of `run_demo.sh`
