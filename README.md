# Conway

> An OpenCL/OpenGL implementation of Conway's Game of Life.

## Install

The program was developed and tested in Ubuntu 18.04. It may be compatible with Windows, but it probably isn't. The changes to fix it, however, should be minor.

### Dependencies

libglfw3-dev, libglfw3, available from apt
GLAD, available from https://glad.dav1d.de/

Build using this command:

```bash
make
```

## Usage

Controls:

Arrow keys move window.
Space pauses/resumes iteration.
Scroll wheel zooms in/out.
Left click changes the cursor cell.
Right click randomizes a block of cells around the cursor.
"+" and "-" keys increase and decrease game iteration speed.
"c" clears the board.

Window may be resized by dragging on edges, if OS supports it.

## Contributing

PRs accepted.

## License

GPLv3 © Stephen Newman

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

You should have received a copy of the GNU General Public License along with this program. If not, see http://www.gnu.org/licenses/.
