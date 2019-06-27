# Conway
An OpenCL/OpenGL implementation of Conway's Game of Life, written and implemented by Stephen Newman. Some small segments of code are from various online sources, which are linked at the relevant points.

Released under GNU General Public License v3.0.

This code is distributed entirely as-is. No guarantees are made as to its function or effects, and the effects of its use are solely the responsibility of the user.

The program was developed and tested in Ubuntu 18.04. It may be compatible with Windows, but it probably isn't. The changes to fix it, however, should be minor.

Dependencies:
libglfw3-dev, libglfw3, available from apt
GLAD, available from https://glad.dav1d.de/



Controls:
Arrow keys move window.
Space pauses/resumes iteration.
Scroll wheel zooms in/out.
Left click changes the cursor cell.
Right click randomizes a block of cells around the cursor.
"+" and "-" keys increase and decrease game iteration speed.
"c" clears the board.

Window may be resized by dragging on edges, if OS supports it.
