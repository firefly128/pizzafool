# PizzaFool 🍕

**"I pity the fool who puts pineapple on pizza!"** — Mr. T

A Motif/X11 pizza ordering application inspired by Sun's classic [PizzaTool](https://en.wikipedia.org/wiki/NeWS) demo,
reimagined with Mr. T as your pizza ordering advisor.

Built for **Solaris 7 / CDE / Motif** on real SPARC hardware (SPARCstation 4, sun4m).

![Section 6 Man Page](https://img.shields.io/badge/man-section%206-blue)
![Solaris 7](https://img.shields.io/badge/Solaris-7-orange)
![SPARC](https://img.shields.io/badge/arch-SPARC-green)
![Motif](https://img.shields.io/badge/toolkit-Motif%2FX11-purple)

## Features

- **12 toppings** with unique Mr. T reactions — from approval to full fury
- **4 pizza sizes**: Fool-size, Sucka-size, Mr. T size, A-Team size
- **Pizza preview window** with live-updating graphical pizza rendering
- **12 Mr. T poses** in 256-color XPM, changing based on your topping choices
- **Custom XPM loader** — no libXpm dependency
- **Nearest-color allocation** — coexists gracefully with CDE's colormap on 8-bit displays
- Pizza rendering ported from the original PizzaTool NeWS/PostScript code to X11

## Screenshots

*Running on a real SPARCstation 4 with Solaris 7 and CDE*

## Building from Source

### Requirements

- Solaris 7 (or compatible) with CDE/Motif installed
- GCC (tgcware gcc47 recommended) or Sun Workshop cc
- X11 display (8-bit PseudoColor or better)

### Build

```sh
make
```

### Run

```sh
DISPLAY=:0 PIZZAFOOL_IMAGES=./images ./pizzafool
```

Or simply:

```sh
make run
```

## Installing from Package

Download the SVR4 package from the [Releases](../../releases) page:

```sh
# As root:
pkgadd -d pizzafool-1.0-sparc.pkg
```

This installs to `/opt/pizzafool`. Run with:

```sh
/opt/pizzafool/bin/pizzafool
```

To remove:

```sh
pkgrm JWpzfool
```

## How It Works

Select toppings from the checkboxes on the left. Mr. T rates each topping on a
*terribleness* scale from 0-2:

| Rating | Toppings | Mr. T's Reaction |
|--------|----------|-----------------|
| 0 (Approved) | Pepperoni, Mushrooms, Sausage, Extra Cheese, Onions | Respect |
| 1 (Questionable) | Green Peppers, Black Olives, Jalapeños, BBQ Chicken | Stern disapproval |
| 2 (TERRIBLE) | Pineapple, Anchovies, Artichoke Hearts | Full Mr. T fury |

Click **"Show Pizza, Fool!"** to open a preview window with a graphical rendering
of your pizza. The preview updates live as you change toppings and size.

Click **"Order Pizza, Fool!"** to place your order and receive Mr. T's wisdom
about tipping delivery drivers and eating every slice.

## Technical Notes

- Single-file C application (~1600 lines) with built-in XPM parser
- 200-color shared palette across all 12 images, with Floyd-Steinberg dithering
- Nearest-color fallback allocator handles full colormaps (CDE uses ~56 of 256 entries)
- Pizza toppings rendered using uniform disc distribution (`sqrt(random)` for radius)
- Double-buffered pizza preview via backing pixmap

## Heritage

[PizzaTool](https://en.wikipedia.org/wiki/NeWS) was a demo application included with
Sun's OpenWindows, originally written for the NeWS window system. It featured a spinning
pizza preview and topping selection. PizzaFool is an affectionate homage, rewritten from
scratch for Motif/X11, with Mr. T providing the commentary that PizzaTool was missing.

## License

Public domain. I pity the fool who needs a license to order pizza.

## Man Page

```
$ man pizzafool
PIZZAFOOL(6)                     Games Manual                     PIZZAFOOL(6)

NAME
       pizzafool - I pity the fool who don't use this pizza program!
...
```

See `pizzafool.6` for the full man page, written in Mr. T's voice.
