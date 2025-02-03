# S-Buffer

A rather unique implementation of the ubiquitous S-Buffer[^1], once a popular
alternative to Z-Buffering for solving the hidden surface removal problem in
software rendering.

The implementation uses a binary tree instead of a linked list to cut down on
the search time. It also supports self-balancing following each insertion to
keep the depth of the tree at a minimum. A single insertion takes order
$O(log n)$, where $n$ is the current number of spans pushed onto the buffer.

The spans need not be inserted in front-to-back order. The buffer can handle
arbitrary ordering as well as interpenetrating geometry.

_Despite bearing no resemblance whatsoever, this implementation was initially
inspired by the [FAQ by Paul Nettle explaining his original approach in great
detail](https://www.gamedev.net/articles/programming/graphics/s-buffer-faq-r668/)._

## Features

1. Stored in a binary tree format for fast traversal along the buffer
2. Self-balancing after each insertion to maintain optimal search and traversal
   speed
3. Supports arbitrary insertion order as well as interpenetrating geometries

## Setting up

### Option 1: Building from source

For this option, you'd need `gcc` installed on your system.

After cloning the repository, navigate to its root folder and run the following
command.

```shell
$ ./build.sh
```

This should spit out the binary `libsbuffer.so` which you can dynamically link
against at runtime for further use — an example of which using `gcc` would look
something like this:

```shell
$ gcc ./your_program.c -o ./your-executable \
      -I/path/to/the/directory/containing/s_buffer.h \
      -L/path/to/the/directory/containing/libsbuffer.so -lsbuffer

# Make sure `libsbuffer.so' is available for the loader at runtime.
$ LD_LIBRARY_PATH=/path/to/the/directory/containing/libsbuffer.so ./your-executable
```

### Option 2: Including in your source (Recommended)

Since it has originally been designed to be used as a header-only library, the
better option would be to directly include it in your source:

```c
#include "/path/to/s_buffer.h"

int main ()
{
    // Direct access to the `s-buffer' API without dynamic linking
    sbuffer_t* sbuffer = SB_Init(640, 2, 1024);
}
```

## Usage

### Initialization

```c
/*
 *  ----> +z
 *  |
 *  v       z_near (distance from the eye to the near-clipping plane)
 *  +x      <---->
 *
 *              /|  ^
 *             / |  |
 *            /  |  |
 *           /   |  |
 * eye -->  @----|  | size
 *           \   |  | (width of the buffer)
 *            \  |  |
 *             \ |  |
 *              \|  v
 */
sbuffer_t* sbuffer = SB_Init(size, z_near, max_depth);

// Initialize a buffer with a width of 640 pixels, a distance of 2 units to the
// near-clipping plane, and a maximum depth of 1024 spans.
sbuffer_t* sbuffer = SB_Init(640, 2, 1024);
```

### Rasterization

```c
// Render the contents of the buffer into `stdout'.
// (`_' denotes empty pixels in the buffer)
SB_Print(sbuffer);
```

### Insertion

```c
// Push a span onto the buffer with endpoints `(x0, w0)' and `(x1, w1)' where
// both endpoints are in perspective-correct screen space -- meaning `w0' and
// `w1' are the multiplicative inverses of their corresponding distances from
// the eye in view space. Another way to put it is that they are the reciprocals
// of the w-components in clip space coordinates:
//
// 1 / w0_clip = 1 / z0_view = w0
// 1 / w1_clip = 1 / z1_view = w1
//
// A unique `id' can be provided for debugging and identification purposes.
SB_Push(sbuffer, x0, x1, w0, w1, id);

unsigned char A = 65;
SB_Push(sbuffer, 2,   5,   1.0f/12, 1.0f/9,  A);     // SB_Print: __AAA_____
SB_Push(sbuffer, 5,   8,   1.0f/9,  1.0f/12, A + 1); // SB_Print: __AAABBB__
SB_Push(sbuffer, 2.6, 7.4, 1.0f/10, 1.0f/10, A + 2); // SB_Print: __ACABCB__
```

### Debugging

```c
// The spans in the buffer can be dumped to `stdout' in a tree-like structure to
// help in debugging. Each line follows the format `[id] [x0, x1)'.
SB_Dump(sbuffer);
// Prints:
// [A] [3.800, 5.000)
//     [C] [2.600, 3.800)
//         [A] [2.000, 2.600)
//     [C] [6.200, 7.400)
//         [B] [5.000, 6.200)
//         [B] [7.400, 8.000)
```

### Deinitialization

```c
// Free up all memory allocated by the buffer.
SB_Destroy(sbuffer);
```

[^1]: The terms "near-clipping plane", "buffer", and "S-Buffer" can be used
      interchangeably.
