
(1)How to Execute on C + My_device:

＄python3 test.py

(2)How to check the result:

The input is matrix with number 2 and the output is 96
Thus, you will see

Output matrix:
 [[96 96 96 96 96]
 [96 96 96 96 96]]

(3) For now, the device is tested for OpenMP

(4) For now, my_device schedule is similar to CPU device

How to add your schedule?

https://tvm.apache.org/docs/how_to/tutorials/customize_opt.html

Follow the steps in the "DLight Rules" to add your scherule in /python/tvm/dlight/my_device

(5) You can also make build and make run in the directory ./runtime_20250421 after run the 

test.py
