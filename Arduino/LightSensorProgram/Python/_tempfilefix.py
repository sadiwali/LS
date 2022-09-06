
from asyncore import write


skipped_first = False

with open("LOG1.CSV", "r") as read_file:
    with open("LOG1_FIXED.CSV", "w") as write_file:
        for l in read_file:
            if (skipped_first == False):
                skipped_first = True
                write_file.write(l + '\n')
                continue
            line = l.rstrip()
            ind = line.find(',', line.rfind(':')) + 2
            new_line = line[:ind] + ',' + line[ind:]
            write_file.write(new_line + '\n')