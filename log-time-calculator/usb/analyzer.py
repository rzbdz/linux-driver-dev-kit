import re
from operator import itemgetter
import trace_rule

def parser(input):
    match = re.search(r'\[\s+(\d+\.\d+)\] usb ((usb)*\d(-\d(.\d)*)*): usb (.+)', input)
    if not match:
        return False, None
    time = float(match.group(1))
    usbnum = str(match.group(2))
    msg = str(match.group(6))
    return True, (usbnum,  msg ,time)

def tryadd(dicku, msg, kw, key, time):
    if kw in msg:
        if "begin" in msg:
            dicku[key][0] = time
        if "end" in msg or "down" in msg:
            dicku[key][1] = time

def print_final(dick: dict):
    for usb, cl in dick.items():
        print(usb)
        for c, times in cl.items():
            print("\t", usb, c, times[1]-times[0])
        

def gen_csv(dick):
    import csv
    with open(trace_rule.output_csv, 'w', newline='',) as file:
        writer = csv.writer(file,quoting=csv.QUOTE_ALL)
        writer.writerow(["usb", ] + [c for c in dick['1-1'].keys()])
        rows = []
        for usb, cl in dick.items():
            if "usb" in usb:
                row = [f"{usb}", ]
            else:    
                row = [f"usb{usb}", ]
            for c, times in cl.items():
                row.append(times[1]-times[0])
            rows.append(row)
            #writer.writerow(row)
                #print(f"\"{usb}\"", f"\"{c}\"", times[1]-times[0], sep=", ")
        rows.sort(key=itemgetter(0))
        writer.writerows(rows)

init_display_keys = []

def load_display_keys():
    for _, display_key in trace_rule.rule:
        init_display_keys.append(display_key)

def gen_dick_val():
    dick_val = {}
    for key in init_display_keys:
        dick_val[key] = [0, 0]
    return dick_val

if __name__=="__main__":
    with open(trace_rule.input_log, "r") as f:
        lines = f.readlines()
    load_display_keys()
    tups = []
    for line in lines:
        p =  parser(line)
        if p[0]:
            tups.append(p[1])
    dick = {}
    for tup in tups:
        usbnum = tup[0]
        msg = tup[1]
        time = tup[2]
        #print("tup", tup)
        if dick.get(usbnum, None) is None:
            dick[usbnum] = gen_dick_val()
        for item in trace_rule.rule:
            log_key, display_key = item
            tryadd(dick[usbnum], msg, log_key, display_key, time)
    #print_final(dick)
    gen_csv(dick)





        
