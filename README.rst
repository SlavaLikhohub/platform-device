TASK
####

#. Modify platform_test_dts.c driver to support data transfer in both directions from /dev/mem to kernel (already supported) and from kernel to /dev/mem.
#. This implies creation of extra memory regions for data buffer, counter and synchronization flags as well as delayed WQ that will send some data(i.e. value of jiffies) to the data buffer with some periodic intervals.
#. Develop testing application (similar to send_data) that will read the data from /dev/mem
#. Make sure your driver works properly when you create more than 1 dummy device.
