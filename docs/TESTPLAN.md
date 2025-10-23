## 1 Build
For building kernel module, execute:
```
cd kernel
make clean
make
```
For building cli, execute:
```
cd user/cli
make clean
make
```
## 2 Load/Unload

To verify the process of load/unload, in a terminal can be executed:
```
dmesg -w | grep nxp_simtemp
```

To load the module, execute:
```
cd kernel
sudo insmod nxp_simtemp_drv.ko
```
Verify character device:
```
ls /dev/simtemp
```

Verify sysfs executing:
```
ls /sys/kernel/simtemp
```

Unload the module with:
```
sudo rmmod nxp_simtemp_drv
```
The kfifo may be drained and the last values in buffer may appear in dmesg before the module is unloaded.

## 3. Periodic Read
After building kernel module and cli, and loading kernel module, execute in one terminal:

```
su root //(if not root user)
cd /sys/kernel/simtemp
echo 100000 > sampling_us
```

in another terminal:
```
cd user/cli
sudo ./cli_nxp_simtemp
```
Execution may be interrupted with `Ctrl+C`. Verify that there are ~10±1 samples/sec

> **Note** 
> If desired, another terminal can monitor the kernel space messages of the module with ```dmesg -w | grep nxp_simtemp```

## 4. Threshold event
After building kernel module and cli, and loading kernel module, execute in one terminal:

```
su root //(if not root user)
cd /sys/kernel/simtemp
echo 100000 > sampling_us
echo 0 > mode
echo 19900 > threshold_mC
```
The values of the simulation can be verified with:
```
cat sampling_us
cat mode
cat threshold_mC
```
This configuration will simulate a temperature sensor with a mean temperature of 20 °C, standard deviation of 0.1°C, sampling time of 100 ms and an alarm that sets when temperature is above of 19.9 °C.
The flag may be set 4 of every 5 samples. (84% of the samples).

Execute the simulation with:
```
cd user/cli
sudo ./cli_nxp_simtemp
```
Execution may be interrupted with `Ctrl+C`.

> **Note** 
> If desired, another terminal can monitor the kernel space messages of the module with ```dmesg -w | grep nxp_simtemp```

## 4. Error Paths

After building kernel module and cli, and loading kernel module, execute in one terminal:
```
su root //(if not root user)
cd /sys/kernel/simtemp
cat sampling_us
echo garbage > sampling_us
cat sampling_us
cat stats 
```
EINVAL for sampling_us must be visualized as last error. It can be tested, input value validation:
```
echo 1 > sampling_us
cat sampling_us
cat stats 
```
OUTOFRANGE for sampling_us must be visualized as last error. Similarly:
```
cat threshold_mC
echo garbage > threshold_mC
cat threshold_mC
cat stats 
```
EINVAL for threshold_mC must be visualized as last error. It can be tested, input value validation:
```
echo 150000 > threshold_mC
cat threshold_mC
cat stats 
```
OUTOFRANGE for threshold_mC must be visualized as last error. Finally:
```
cat mode
echo 5 > mode
cat mode
cat stats 
```
EINVAL for mode must be visualized as last error.
> **Note** 
> If desired, another terminal can monitor the kernel space messages of the module with ```dmesg -w | grep nxp_simtemp```

## 5 High frequency sampling 
After building kernel module and cli, and loading kernel module, execute in one terminal:
```
su root //(if not root user)
cd /sys/kernel/simtemp
echo 100 > sampling_us
cat sampling_us
cat stats 
```
Check how even after a high frecuency sampling (0.1 ms = 10kHz),  simulation doesn´t wedge and counters increase. It can be run:
```
cd user/cli
sudo ./cli_nxp_simtemp
```
But every ms, doesn´t has 10 samples, as a 0.1 ms sampling rate may create.

> **Note** 
> If desired, another terminal can monitor the kernel space messages of the module with ```dmesg -w | grep nxp_simtemp```

## 6 Reading and writing concurrently
After building kernel module and cli, and loading kernel module, execute in one terminal:
```
su root //(if not root user)
cd /sys/kernel/simtemp
watch -n 0.1 "cat sampling_us && cat mode && cat threshold_mC"
```
In another terminal execute:
```
cd user/cli
sudo ./cli_nxp_simtemp
```
In another terminal change the values of the simulation, an example may be:
```
su root //(if not root user)
cd /sys/kernel/simtemp
echo 500000 > sampling_us
echo 2 > mode
echo 0 > threshold_mC
```
> **Note** 
> If desired, another terminal can monitor the kernel space messages of the module with ```dmesg -w | grep nxp_simtemp```