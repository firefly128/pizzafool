# PizzaFool

"I pity the fool who puts pineapple on pizza!" — Mr. T

A Motif/X11 pizza ordering application for Solaris 7 / CDE on SPARC hardware, inspired by Sun's classic PizzaTool demo from OpenWindows/NeWS. Mr. T serves as your pizza ordering advisor.

Public repo: github.com/firefly128/pizzafool.

## Features

- 12 toppings with unique Mr. T reactions based on a terribleness scale
- 4 pizza sizes: Fool-size, Sucka-size, Mr. T size, A-Team size
- Pizza preview window with live-updating graphical rendering
- 12 Mr. T poses in 256-color XPM, changing based on topping choices
- Custom XPM loader — no libXpm dependency required
- Nearest-color allocation — coexists with CDE's colormap on 8-bit displays
- Pizza rendering adapted from the original PizzaTool NeWS/PostScript code

## Topping ratings

| Rating | Toppings | Reaction |
|--------|----------|----------|
| 0 — Approved | Pepperoni, Mushrooms, Sausage, Extra Cheese, Onions | Respect |
| 1 — Questionable | Green Peppers, Black Olives, Jalapenos, BBQ Chicken | Stern disapproval |
| 2 — TERRIBLE | Pineapple, Anchovies, Artichoke Hearts | Full fury |

Click "Show Pizza, Fool!" to open a preview window. Click "Order Pizza, Fool!" to place the order and receive Mr. T's wisdom about tipping delivery drivers.

## Building from source

### Requirements

- Solaris 7 (or compatible) with CDE/Motif installed
- GCC (TGCware gcc47 or Sunstorm GCC 11) or Sun Workshop cc

### Build

```sh
make
```

The Makefile defaults to `/usr/tgcware/gcc47/bin/gcc`. Edit the `CC` variable to use a different compiler.

### Run

```sh
DISPLAY=:0 PIZZAFOOL_IMAGES=./images ./pizzafool
```

Or:

```sh
make run
```

## Installing from package

Download the SVR4 package from the [Releases](https://github.com/firefly128/pizzafool/releases) page:

```sh
pkgadd -d SSTpzfol-1.0-sparc.pkg
```

This installs to `/opt/pizzafool`. Run with:

```sh
DISPLAY=:0 /opt/pizzafool/bin/pizzafool
```

To remove:

```sh
pkgrm SSTpzfol
```

pizzafool is also available via spm:

```sh
spm install pizzafool
```

## Technical notes

- Single-file C application (~1600 lines) with built-in XPM parser
- 200-color shared palette across all 12 images, with Floyd-Steinberg dithering
- Nearest-color fallback allocator handles full colormaps (CDE uses ~56 of 256 entries)
- Pizza toppings rendered using uniform disc distribution (`sqrt(random)` for radius)
- Double-buffered pizza preview via backing pixmap
- Links against Motif (`-lXm`), Xt, and X11 — all standard on Solaris 7 with CDE

## Heritage

PizzaTool was a demo application shipped with Sun's OpenWindows, originally written for the NeWS window system. PizzaFool is an affectionate homage, rewritten from scratch for Motif/X11.

## License

Public domain.

## Man page

See `pizzafool.6` for the man page (section 6, Games Manual), written in Mr. T's voice.
