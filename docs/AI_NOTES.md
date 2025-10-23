## Random Temperature Samples
The following prompt was used

> Can you help me to code a C function that takes as inputs 2parameters, mean and standard deviation, both of type __u32 and it returns a discrete random variable that follows a normal distribution described by the input mean and standard deviation values. Furthermore, the returned variable must be of type __u32 and calculations must not include float types or doubles, only __u32. This function must run in kernel space.

According to [openstax](https://openstax.org/books/introductory-statistics-2e/pages/7-2-the-central-limit-theorem-for-sums):
>The  central limit theorem for sums says that if you repeatedly draw samples of a given size (such as repeatedly rolling ten dice) and calculate the sum of each sample, these sums tend to follow a normal distribution. As sample sizes increase, the distribution of means more closely follows the normal distribution. The normal distribution has a mean equal to the original mean multiplied by the sample size and a standard deviation equal to the original standard deviation multiplied by the square root of the sample size.

Furthermore, the resulting function was applied and the resulting values were stores in temperature.log (for a 20,000 m°C mean, and standard deviation of 100 m°C) and in temperature_noisy.log (for a 20,000 m°C and a standard deviation of 2,000 m°C).
Some lines of temperature.log were:
> 2025-10-22T20:04:25.310Z temp=19847C alert=0
2025-10-22T20:04:25.410Z temp=19956C alert=0
2025-10-22T20:04:25.509Z temp=20204C alert=1
2025-10-22T20:04:25.610Z temp=19955C alert=0
2025-10-22T20:04:25.711Z temp=19957C alert=0

Some lines of temperature_noisy.log were:
>2025-10-22T20:10:42.032Z temp=22020C alert=1
2025-10-22T20:10:42.149Z temp=19051C alert=0
2025-10-22T20:10:42.232Z temp=20888C alert=1
2025-10-22T20:10:42.334Z temp=24827C alert=1
2025-10-22T20:10:42.432Z temp=19698C alert=0

The following script on python was applied:

    import pandas as pd
    import re
    data = []
    with open("temperature_noisy.log", "r") as f:
        for line in f:
            line = line.strip()
            if line:
                # Extract timestamp, temperature, and alert using regex
                match = re.match(r'(\S+)\s+temp=([\d.]+)C\s+alert=(\d+)', line)
                if match:
                    timestamp, temp, alert = match.groups()
                    data.append({
                        'timestamp': timestamp,
                        'temp': int(temp),
                        'alert': int(alert)
                    })
    
    df = pd.DataFrame(data)
    
    print("Temperature Statistics:")
    print(df["temp"].describe())
    print("\nTotal readings: {}".format(len(df)))
    print("Alert count: {}".format(df['alert'].sum()))
    print("Alert percentage: {:.1f}%".format(df['alert'].mean()*100))

The results obtained with temperature.log:

    Temperature Statistics:
    count      970.000000
    mean     20003.377320
    std        102.286622
    min      19701.000000
    25%      19933.000000
    50%      20004.500000
    75%      20076.000000
    max      20408.000000
    Name: temp, dtype: float64
A temperature mean of ~20,000 m°C with std  ~100 m°C was computed.

The results obtained with temperature_noisy.log:

    Temperature Statistics:
    count     1281.000000
    count     1281.000000
    mean     20050.991413
    std       1996.078777
    min      14469.000000
    25%      18637.000000
    50%      19989.000000
    75%      21440.000000
    max      26345.000000
    Name: temp, dtype: float64

A temperature mean of ~20,000 m°C with std  ~2,000 m°C was computed. And that's how the C code was validated.