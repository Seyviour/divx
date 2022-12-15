import matplotlib.pyplot as plt
from matplotlib.widgets import Slider

import os
script_dir = os.path.dirname(__file__)
rel_path = "sample_reads/sample_reads3.txt"
abs_file_path = os.path.join(script_dir, rel_path)

with open(abs_file_path, "r") as f:
    samples = f.readlines()

levels = [32, 132, 125, 820, 1550, 2000,  2500, 2700,  3000]
with open(os.path.join(script_dir, "log"), "r") as g:
    log = g.readlines()

log = [levels[int(val)-1] for val in log]

x = [int(b) for b in samples]
w_size = 8
e = x[w_size: len(x)-w_size]
f = [sum(x[idx-w_size:idx+w_size])/(w_size*2) for idx in range(w_size, len(x)-w_size)]
g = [max(x[idx-w_size:idx+w_size]) for idx in range(w_size, len(x)-w_size)]
h = [min(x[idx-w_size:idx+w_size]) for idx in range(w_size, len(x)-w_size)]
# plt.scatter(list(range(len(g))), g, marker=".")

fig, ax = plt.subplots()
plt.subplots_adjust(bottom=0.25)
data = g
data2 = h
data3 = f

# l, = plt.plot(list(range(len(data))), data)
# l2, = plt.plot(list(range(len(data))), data2)
# l3, = plt.plot(list(range(len(data))), data3)
l4, = plt.plot(list(range(len(data))), e)
# l5, = plt.plot(list(range(len(log))), log)

plt.axis([0,10,0,3500])

axcolor = 'blue'

axpos = plt.axes([0.2,0.1,0.65,0.03], facecolor=axcolor)
spos = Slider(axpos, 'Pos', 0.1,10000.0)

def update(val):
    pos = spos.val
    ax.axis([pos,pos+600,0,3200])
    fig.canvas.draw_idle()

spos.on_changed(update)
plt.show()