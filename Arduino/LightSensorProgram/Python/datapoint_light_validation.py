import matplotlib.pyplot as plt

FILE_TO_OPEN = "./data/202209161244080.CSV"

def get_formatted_datapoint(line):
    tokens = line.split(',')
    timestamp = tokens[0] + " " + tokens[1]
    
    is_dark = bool(int(tokens[7]))
    x = [i for i in range(340, 1010 + 5, 5)]
    y = [float(i) for i in tokens[11:-1]]

    return [x,y, timestamp, is_dark]
    



if __name__ == "__main__":
    
    firstline = True
    ind = 0
    with open (FILE_TO_OPEN, 'r') as f:
        while True:
            line = f.readline()
            
            if not line:
                break
                
            if firstline:
                firstline = False
                continue
   
            ind += 1
            
            x, y, timestamp, is_dark = get_formatted_datapoint(line)
            
           
            
            plt.plot(x,y)
            plt.title("[" + str(ind) + "] " + timestamp + " " + ("DARK" if is_dark else "OK"))
    
    plt.show()
