# kernel begin/end log trace analyzer

Sometimes kernel function_graph tracing cannot work. In my case, there
are bugs cause kernel dead when using function_graph tracing.

This script help record and calculate time usage of each 
function call (notice: not precise at all, including sched out).

## TODO:
Currently can only parse usb format dev_info log.
Need to supoport custom regex format.

## Usage

Add debug print to kernel codes. in formats like this, 
make sure include `"begin"` and `"end"` keywords:
```cpp
dev_info(&udev->dev, "usb port resume begin < 0 %s\n", udev->devpath);
    your_function_to_trace()
dev_info(&udev->dev, "usb port resume end < 0 %s\n", udev->devpath);
```

Add items in `trace_rule.py`, like this:
```python
rule=[
    (
        regex match keywords in your log, 
        display column name in generated table
    ),
    ....
]
```
You also need to specific input and output with `input_log` and `output_csv` attribute.

Generated result like this:
```csv
"usb","resume-device","resume-call","generic","port","finish-port"
"usb1","6.999999999379725e-05","5.199999999661031e-05","4.6999999995023245e-05","0","0"
"usb1-1","0.30516900000000646","0.30515599999999665","0.30515099999999507","0.28519499999998743","0.2851869999999934"
"usb1-1.1","0.20902699999999186","0.20898499999999842","0.20897899999999936","0.18908900000000983","0.18908199999999908"
"usb1-1.3","0.9722529999999949","0.9722380000000044","0.9722330000000028","0.9523360000000025","0.9523289999999918"
"usb1-1.4","0.7762560000000178","0.7762419999999963","0.7762369999999947","0.7563430000000011","0.7563349999999929"
"usb2","4.799999999249849e-05","2.6999999988674972e-05","2.099999998961266e-05","0","0"
"usb2-1","0.9997340000000179","0.9997159999999923","0.9997109999999907","0.49576199999999915","0.49575299999999345"
"usb2-1.1","1.1599620000000073","1.1599469999999883","1.1599410000000034","1.1399820000000034","1.1399739999999952"
"usb2-1.4","0.8720050000000015","0.8719930000000033","0.871988000000016","0.852043000000009","0.8520330000000058"
"usb2-1.5","0.2587210000000084","0.2587079999999844","0.25870100000000207","0.23876899999999068","0.23876199999997993"
"usb3","4.6999999995023245e-05","2.6999999988674972e-05","2.099999998961266e-05","0","0"
"usb3-1","0.5997500000000002","0.5997370000000046","0.599732000000003","0.18376100000000406","0.18375199999999836"
```