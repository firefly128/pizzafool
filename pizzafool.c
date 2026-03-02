/*
 * PizzaFool - Like Sun's PizzaTool, but Mr. T judges your toppings.
 * "I pity the fool who puts pineapple on pizza!"
 *
 * For Solaris 7 / CDE / Motif on SPARCstation 4
 * Now with 256-color Mr. T images loaded from XPM files!
 *
 * Compile:
 *   gcc -o pizzafool pizzafool.c -I/usr/dt/include -I/usr/openwin/include \
 *       -L/usr/dt/lib -L/usr/openwin/lib -lXm -lXt -lX11 -lm \
 *       -R/usr/dt/lib -R/usr/openwin/lib
 *
 * Run:
 *   PIZZAFOOL_IMAGES=./images ./pizzafool
 */

#include <Xm/Xm.h>
#include <Xm/MainW.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/Label.h>
#include <Xm/LabelG.h>
#include <Xm/PushB.h>
#include <Xm/ToggleB.h>
#include <Xm/RowColumn.h>
#include <Xm/Separator.h>
#include <Xm/Text.h>
#include <Xm/MessageB.h>
#include <Xm/CascadeB.h>
#include <Xm/DrawingA.h>
#include <Xm/BulletinB.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Shell.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include <sys/utsname.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ================================================================
 * SIMPLE XPM FILE LOADER - No libXpm needed!
 * Parses XPM format and creates XImage using core X11 calls.
 * ================================================================ */

typedef struct {
    XImage *ximage;
    int width;
    int height;
    int loaded;
} MrtImage;

/* Read entire file into malloc'd buffer */
static char *read_file_contents(const char *path, long *out_len)
{
    FILE *f = fopen(path, "r");
    char *buf;
    long len;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = (char *)malloc(len + 1);
    if (!buf) { fclose(f); return NULL; }
    len = fread(buf, 1, len, f);
    buf[len] = '\0';
    fclose(f);
    if (out_len) *out_len = len;
    return buf;
}

/* Extract next quoted string from XPM data, advancing *pos */
static char *next_quoted_string(const char *data, long datalen, long *pos)
{
    long i = *pos;
    long start, end;
    char *result;

    /* Find opening quote */
    while (i < datalen && data[i] != '"') i++;
    if (i >= datalen) return NULL;
    i++; /* skip opening quote */
    start = i;

    /* Find closing quote */
    while (i < datalen && data[i] != '"') i++;
    if (i >= datalen) return NULL;
    end = i;
    i++; /* skip closing quote */
    *pos = i;

    result = (char *)malloc(end - start + 1);
    memcpy(result, data + start, end - start);
    result[end - start] = '\0';
    return result;
}

/* Parse hex color value */
static int hex_val(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

/* ================================================================
 * COLOR CACHE - handles 8-bit colormap exhaustion gracefully.
 * When XAllocColor fails, finds nearest match from existing cells.
 * ================================================================ */

#define MAX_CACHED_COLORS 256
static struct {
    unsigned short r, g, b;
    unsigned long pixel;
} color_cache[MAX_CACHED_COLORS];
static int n_cached = 0;
static XColor *cmap_cells = NULL;
static int cmap_ncells = 0;

static void init_color_cache(Display *dpy, int screen)
{
    int i;
    Colormap cmap = DefaultColormap(dpy, screen);
    cmap_ncells = DisplayCells(dpy, screen);
    cmap_cells = (XColor *)malloc(cmap_ncells * sizeof(XColor));
    for (i = 0; i < cmap_ncells; i++)
        cmap_cells[i].pixel = i;
    XQueryColors(dpy, cmap, cmap_cells, cmap_ncells);
    n_cached = 0;
    printf("PizzaFool: Display has %d colormap cells (depth %d)\n",
           cmap_ncells, DefaultDepth(dpy, screen));
    fflush(stdout);
}

/* Color allocation stats for diagnostics */
static int color_alloc_new = 0, color_alloc_cached = 0, color_alloc_fallback = 0;

static unsigned long alloc_xpm_color(Display *dpy, int screen,
                                     unsigned short r, unsigned short g,
                                     unsigned short b)
{
    Colormap cmap = DefaultColormap(dpy, screen);
    XColor xc;
    int i;

    /*
     * IMPORTANT: All distance calculations use 8-bit (0-255) values.
     * On 32-bit SPARC, 'long' is 32 bits. Squaring 16-bit values
     * (max 65535) overflows: 65535^2 = 4.3 billion > 2^31.
     * In 8-bit space: max dist = 255^2 * 3 = 195,075 (fits in int).
     */
    int r8 = r >> 8, g8 = g >> 8, b8 = b >> 8;

    /* First check our cache for an exact or near-exact match */
    for (i = 0; i < n_cached; i++) {
        int dr = r8 - (color_cache[i].r >> 8);
        int dg = g8 - (color_cache[i].g >> 8);
        int db = b8 - (color_cache[i].b >> 8);
        int dist = dr*dr + dg*dg + db*db;
        /* Threshold: colors within ~1 step in 8-bit space */
        if (dist <= 3) {
            color_alloc_cached++;
            return color_cache[i].pixel;
        }
    }

    /* Try to allocate a new cell */
    xc.red = r; xc.green = g; xc.blue = b;
    xc.flags = DoRed | DoGreen | DoBlue;
    if (XAllocColor(dpy, cmap, &xc)) {
        if (n_cached < MAX_CACHED_COLORS) {
            color_cache[n_cached].r = r;
            color_cache[n_cached].g = g;
            color_cache[n_cached].b = b;
            color_cache[n_cached].pixel = xc.pixel;
            n_cached++;
        }
        color_alloc_new++;
        return xc.pixel;
    }

    /* Allocation failed - find nearest in full colormap (8-bit math) */
    {
        unsigned long best_pixel = BlackPixel(dpy, screen);
        int best_dist = 195076; /* > max possible (255^2 * 3) */

        /* Search our own cache first (known-good allocations) */
        for (i = 0; i < n_cached; i++) {
            int dr = r8 - (color_cache[i].r >> 8);
            int dg = g8 - (color_cache[i].g >> 8);
            int db = b8 - (color_cache[i].b >> 8);
            int dist = dr*dr + dg*dg + db*db;
            if (dist < best_dist) {
                best_dist = dist;
                best_pixel = color_cache[i].pixel;
            }
        }

        /* Also search the full colormap snapshot */
        if (cmap_cells) {
            for (i = 0; i < cmap_ncells; i++) {
                int dr = r8 - (cmap_cells[i].red >> 8);
                int dg = g8 - (cmap_cells[i].green >> 8);
                int db = b8 - (cmap_cells[i].blue >> 8);
                int dist = dr*dr + dg*dg + db*db;
                if (dist < best_dist) {
                    best_dist = dist;
                    best_pixel = cmap_cells[i].pixel;
                }
            }
        }

        /* Cache it so we don't re-scan for similar colors */
        if (n_cached < MAX_CACHED_COLORS) {
            color_cache[n_cached].r = r;
            color_cache[n_cached].g = g;
            color_cache[n_cached].b = b;
            color_cache[n_cached].pixel = best_pixel;
            n_cached++;
        }
        color_alloc_fallback++;
        return best_pixel;
    }
}


/* Load an XPM file into an XImage */
static int load_xpm_file(Display *dpy, int screen, const char *path, MrtImage *img)
{
    char *data;
    long datalen, pos;
    char *header_str;
    int width, height, ncolors, cpp;
    int i, x, y;

    unsigned long *color_pixels = NULL;
    char **color_keys = NULL;

    Visual *visual;
    int depth;
    XImage *ximg;
    char *imgdata;

    img->loaded = 0;
    img->ximage = NULL;

    data = read_file_contents(path, &datalen);
    if (!data) {
        fprintf(stderr, "PizzaFool: Cannot read %s\n", path);
        return 0;
    }

    pos = 0;

    /* Parse header: "width height ncolors cpp" */
    header_str = next_quoted_string(data, datalen, &pos);
    if (!header_str) { free(data); return 0; }

    if (sscanf(header_str, "%d %d %d %d", &width, &height, &ncolors, &cpp) != 4) {
        fprintf(stderr, "PizzaFool: Bad XPM header in %s: %s\n", path, header_str);
        free(header_str); free(data);
        return 0;
    }
    free(header_str);

    if (width <= 0 || height <= 0 || ncolors <= 0 || ncolors > 300 || cpp <= 0 || cpp > 4) {
        fprintf(stderr, "PizzaFool: Invalid XPM dimensions in %s\n", path);
        free(data);
        return 0;
    }

    /* Parse color table */
    color_keys = (char **)calloc(ncolors, sizeof(char *));
    color_pixels = (unsigned long *)calloc(ncolors, sizeof(unsigned long));
    visual = DefaultVisual(dpy, screen);
    depth = DefaultDepth(dpy, screen);

    for (i = 0; i < ncolors; i++) {
        char *line = next_quoted_string(data, datalen, &pos);
        char *color_spec;
        int llen;

        if (!line) {
            fprintf(stderr, "PizzaFool: Truncated color table in %s at color %d/%d\n",
                    path, i, ncolors);
            /* Clean up already allocated keys */
            { int j; for (j = 0; j < i; j++) free(color_keys[j]); }
            free(color_keys); free(color_pixels); free(data);
            return 0;
        }

        llen = strlen(line);
        if (llen < cpp) {
            fprintf(stderr, "PizzaFool: Color line too short in %s: '%s'\n", path, line);
            free(line);
            { int j; for (j = 0; j < i; j++) free(color_keys[j]); }
            free(color_keys); free(color_pixels); free(data);
            return 0;
        }

        /* First cpp chars are the pixel key */
        color_keys[i] = (char *)malloc(cpp + 1);
        memcpy(color_keys[i], line, cpp);
        color_keys[i][cpp] = '\0';

        /* Find "c #RRGGBB" or "c colorname" after the key */
        color_spec = strstr(line + cpp, " c ");
        if (!color_spec) color_spec = strstr(line + cpp, "\tc ");
        if (color_spec) {
            char cleaned[64];
            int ci = 0;
            color_spec += 3; /* skip " c " */
            while (*color_spec == ' ' || *color_spec == '\t') color_spec++;

            /* Copy color spec, stripping trailing whitespace/punctuation */
            while (*color_spec && *color_spec != '\t' && *color_spec != '\n'
                   && *color_spec != ',' && ci < 62) {
                cleaned[ci++] = *color_spec++;
            }
            while (ci > 0 && (cleaned[ci-1] == ' ' || cleaned[ci-1] == '\t'))
                ci--;
            cleaned[ci] = '\0';

            if (strcasecmp(cleaned, "None") == 0) {
                color_pixels[i] = WhitePixel(dpy, screen);
            } else if (cleaned[0] == '#' && ci >= 7) {
                unsigned short r, g, b;
                r = (hex_val(cleaned[1]) * 16 + hex_val(cleaned[2])) * 257;
                g = (hex_val(cleaned[3]) * 16 + hex_val(cleaned[4])) * 257;
                b = (hex_val(cleaned[5]) * 16 + hex_val(cleaned[6])) * 257;
                color_pixels[i] = alloc_xpm_color(dpy, screen, r, g, b);
            } else if (ci > 0) {
                /* Named color */
                XColor xc, exact;
                if (XAllocNamedColor(dpy, DefaultColormap(dpy, screen),
                                     cleaned, &xc, &exact))
                    color_pixels[i] = xc.pixel;
                else
                    color_pixels[i] = BlackPixel(dpy, screen);
            } else {
                color_pixels[i] = BlackPixel(dpy, screen);
            }
        } else {
            color_pixels[i] = BlackPixel(dpy, screen);
        }
        free(line);
    }

    /* Create XImage - allocate enough for any depth */
    {
        int bpp = (depth <= 8) ? 1 : (depth <= 16) ? 2 : 4;
        int bytes_per_line = ((width * bpp + 3) / 4) * 4; /* pad to 4 bytes */
        imgdata = (char *)calloc(bytes_per_line * height, 1);
    }
    ximg = XCreateImage(dpy, visual, depth, ZPixmap, 0,
                        imgdata, width, height, 32, 0);
    if (!ximg) {
        fprintf(stderr, "PizzaFool: XCreateImage failed for %s\n", path);
        free(imgdata);
        for (i = 0; i < ncolors; i++) free(color_keys[i]);
        free(color_keys); free(color_pixels); free(data);
        return 0;
    }

    /* Parse pixel rows */
    for (y = 0; y < height; y++) {
        char *row = next_quoted_string(data, datalen, &pos);
        int rowlen;
        if (!row) break;
        rowlen = strlen(row);

        for (x = 0; x < width; x++) {
            int offset = x * cpp;
            unsigned long pixel = BlackPixel(dpy, screen);

            if (offset + cpp > rowlen) break; /* safety: don't read past row */

            /* Find matching color key */
            for (i = 0; i < ncolors; i++) {
                if (memcmp(row + offset, color_keys[i], cpp) == 0) {
                    pixel = color_pixels[i];
                    break;
                }
            }
            XPutPixel(ximg, x, y, pixel);
        }
        free(row);
    }

    /* Cleanup */
    for (i = 0; i < ncolors; i++) free(color_keys[i]);
    free(color_keys);
    free(color_pixels);
    free(data);

    img->ximage = ximg;
    img->width = width;
    img->height = height;
    img->loaded = 1;

    return 1;
}

static void free_mrt_image(MrtImage *img)
{
    if (img->ximage) {
        XDestroyImage(img->ximage); /* frees imgdata too */
        img->ximage = NULL;
    }
    img->loaded = 0;
}


/* ================================================================
 * IMAGE SET - 12 poses mapped to toppings & sizes
 * ================================================================ */

#define NUM_IMAGES 12

/* Image filenames (without .xpm extension) */
static const char *image_files[NUM_IMAGES] = {
    "mrt_point1",       /* 0  - default / neutral */
    "mrt_point2",       /* 1  - warning */
    "mrt_thumbsup1",    /* 2  - approval */
    "mrt_angry1",       /* 3  - angry */
    "mrt_crossed1",     /* 4  - arms crossed / stern */
    "mrt_happy1",       /* 5  - happy */
    "mrt_point3",       /* 6  - pointing variant */
    "mrt_thumbsup2",    /* 7  - thumbs up variant */
    "mrt_angry2",       /* 8  - angry variant */
    "mrt_happy2",       /* 9  - happy variant */
    "mrt_point4",       /* 10 - pointing variant */
    "mrt_thumbsup3",    /* 11 - thumbs up variant */
};

static MrtImage images[NUM_IMAGES];
static int images_loaded = 0;

static void load_all_images(Display *dpy, int screen, const char *img_dir)
{
    int i, ok = 0;
    char path[512];

    for (i = 0; i < NUM_IMAGES; i++) {
        sprintf(path, "%s/%s.xpm", img_dir, image_files[i]);
        if (load_xpm_file(dpy, screen, path, &images[i])) {
            ok++;
        } else {
            fprintf(stderr, "PizzaFool: Warning: Could not load %s\n", path);
        }
    }
    images_loaded = ok;
    printf("PizzaFool: Loaded %d/%d Mr. T images from %s\n", ok, NUM_IMAGES, img_dir);
    printf("PizzaFool: Colors - %d new allocs, %d cache hits, %d fallbacks, %d cached total\n",
           color_alloc_new, color_alloc_cached, color_alloc_fallback, n_cached);
    printf("PizzaFool: WhitePixel=%lu BlackPixel=%lu\n",
           WhitePixel(dpy, screen), BlackPixel(dpy, screen));
    fflush(stdout);
}


/* ================================================================
 * PIZZA DATA & MR T INSULTS
 * ================================================================ */

typedef struct {
    const char *name;
    int is_terrible;   /* 0=ok, 1=questionable, 2=Mr T goes OFF */
    const char *insult;
    int image_idx;     /* which image to show for this topping */
} Topping;

static Topping toppings[] = {
    {"Pepperoni",       0,
     "Pepperoni? That's what a REAL man puts on pizza! I approve, fool!",
     2},  /* thumbsup1 */
    {"Mushrooms",       0,
     "Mushrooms are cool. Mr. T respects the fungus.",
     7},  /* thumbsup2 */
    {"Sausage",         0,
     "Italian sausage! Now you're talking, sucka!",
     5},  /* happy1 */
    {"Extra Cheese",    0,
     "Extra cheese? Mr. T ALWAYS gets extra cheese. You ain't so foolish after all!",
     11}, /* thumbsup3 */
    {"Onions",          0,
     "Onions are fine. They make you cry, but Mr. T don't cry for NOBODY!",
     9},  /* happy2 */
    {"Green Peppers",   1,
     "Green peppers?! What is this, a SALAD?! I pity you, fool!",
     0},  /* point1 - warning */
    {"Black Olives",    1,
     "Olives?! Mr. T didn't fight in Rocky III to eat OLIVES on pizza!",
     4},  /* crossed1 - stern */
    {"Pineapple",       2,
     "PINEAPPLE?! I PITY THE FOOL who puts pineapple on pizza!! That ain't pizza, that's a CRIME, sucka!",
     3},  /* angry1 */
    {"Anchovies",       2,
     "ANCHOVIES?! You want FISH on your PIZZA?! Mr. T is gonna throw you outta this pizzeria, FOOL!",
     8},  /* angry2 */
    {"Jalape\361os",    1,
     "Jalape\361os? You think you're TOUGH?! Mr. T is the only one tough enough for hot peppers, fool!",
     1},  /* point2 - warning */
    {"BBQ Chicken",     1,
     "BBQ Chicken on pizza?! Make up your mind, fool! Is it a pizza or a BARBECUE?!",
     6},  /* point3 - questioning */
    {"Artichoke Hearts",2,
     "ARTICHOKE HEARTS?! What kind of fancy-pants FOOL eats artichoke hearts on pizza?! Mr. T pities you!",
     10}, /* point4 - stern point */
};

#define NUM_TOPPINGS (sizeof(toppings) / sizeof(toppings[0]))

/* Size options with image indices */
static const char *sizes[] = {
    "Small (Fool-size)",
    "Medium (Sucka-size)",
    "Large (Mr. T size)",
    "Family (A-Team size)"
};
static int size_images[] = { 4, 1, 5, 9 };  /* crossed, point, happy, happy2 */
#define NUM_SIZES 4

/* General Mr T quotes for order time */
static const char *order_quotes[] = {
    "I pity the fool who don't tip the delivery driver!",
    "Mr. T orders his pizza and he orders it NOW, sucka!",
    "You better eat every slice, fool! Mr. T don't waste food!",
    "This pizza better be here in 30 minutes or Mr. T is comin' for you!",
    "First name: Mister. Last name: T. Pizza: ORDERED, fool!",
    "Mr. T says: eat your pizza, drink your milk, stay in school!",
};
#define NUM_ORDER_QUOTES 6

static const char *empty_quotes[] = {
    "You ain't selected NO toppings, fool! What kind of pizza is that?!",
    "A pizza with NOTHING on it?! I pity you, sucka! Pick some toppings!",
    "Mr. T don't eat no PLAIN pizza! Get some toppings on there, fool!",
};
#define NUM_EMPTY_QUOTES 3


/* ================================================================
 * GLOBALS
 * ================================================================ */

static Widget toplevel_w, insult_text, drawing_area;
static Widget topping_toggles[12]; /* NUM_TOPPINGS */
static int selected_size = 2; /* default: Mr. T size */
static Display *dpy;
static int screen_num;
static GC draw_gc;
static int current_image = 0;  /* index into images[] */
static unsigned long bg_color, red_color;

/* Pizza preview window globals */
static Widget pizza_shell = NULL;
static Widget pizza_drawing = NULL;
static GC pizza_gc = None;
static Pixmap pizza_pixmap = None;
static int pizza_pix_w = 0, pizza_pix_h = 0;
static unsigned long pizza_colors[32]; /* allocated pizza colors */
static int pizza_colors_ready = 0;

/* Forward declarations */
static void set_insult(const char *text);

/* ================================================================
 * COLOR ALLOCATION
 * ================================================================ */

static unsigned long alloc_color(const char *name, unsigned long fallback)
{
    XColor exact, closest;
    Colormap cmap = DefaultColormap(dpy, screen_num);
    if (XAllocNamedColor(dpy, cmap, name, &closest, &exact))
        return closest.pixel;
    return fallback;
}


/* ================================================================
 * PIZZA DRAWING - Converted from Sun PizzaTool NeWS/PostScript
 *
 * The original PizzaTool used PostScript drawing primitives to
 * render a pizza with crust, sauce, cheese, and sprinkled toppings.
 * This is a faithful X11 conversion of that rendering code.
 * ================================================================ */

/* Pizza color indices */
enum {
    PC_BG = 0, PC_CRUST, PC_SAUCE, PC_CHEESE,
    PC_PEPPERONI, PC_MUSHROOM, PC_SAUSAGE, PC_OLIVE,
    PC_ONION, PC_PEPPER, PC_PINEAPPLE, PC_ANCHOVY,
    PC_JALAPENO, PC_BBQ, PC_ARTICHOKE, PC_EXTRACHS,
    PC_CRUST_DARK, PC_CRUST_LIGHT, PC_SAUCE_DARK,
    PC_COUNT
};

static const char *pizza_color_names[] = {
    "#2A2A2A",   /* BG - dark background */
    "#D9992B",   /* CRUST - golden brown */
    "#CC1A1A",   /* SAUCE - tomato red */
    "#FFDD33",   /* CHEESE - yellow */
    "#B31A33",   /* PEPPERONI - dark red circles */
    "#667755",   /* MUSHROOM - grayish green */
    "#B31A33",   /* SAUSAGE - dark reddish */
    "#003311",   /* OLIVE - very dark green */
    "#E6E6CC",   /* ONION - pale yellow-white */
    "#33B300",   /* PEPPER - bright green */
    "#E6CC00",   /* PINEAPPLE - bright yellow */
    "#33CC33",   /* ANCHOVY - (greenish, from original) */
    "#33E600",   /* JALAPENO - bright green */
    "#B31A1A",   /* BBQ CHICKEN - reddish */
    "#00B300",   /* ARTICHOKE - green */
    "#FFE64D",   /* EXTRA CHEESE - lighter yellow */
    "#AA7718",   /* CRUST_DARK - darker crust edge */
    "#EECC55",   /* CRUST_LIGHT - lighter crust highlight */
    "#991111",   /* SAUCE_DARK - darker sauce */
};

static void init_pizza_colors(void)
{
    int i;
    unsigned int r8, g8, b8;

    if (pizza_colors_ready) return;
    for (i = 0; i < PC_COUNT; i++) {
        const char *hex = pizza_color_names[i];
        /* Parse "#RRGGBB" */
        if (hex[0] == '#' &&
            sscanf(hex + 1, "%02x%02x%02x", &r8, &g8, &b8) == 3) {
            /* Use alloc_xpm_color which has nearest-color fallback
             * for 8-bit displays with exhausted colormaps */
            pizza_colors[i] = alloc_xpm_color(dpy, screen_num,
                                              r8 * 257, g8 * 257, b8 * 257);
        } else {
            pizza_colors[i] = (i == PC_BG) ? BlackPixel(dpy, screen_num) :
                              WhitePixel(dpy, screen_num);
        }
    }
    pizza_colors_ready = 1;
}

/* Simple pseudo-random for consistent pizza look, seeded per-draw */
static unsigned int pizza_rand_state;
static int pizza_random(int max)
{
    pizza_rand_state = pizza_rand_state * 1103515245 + 12345;
    return (int)((pizza_rand_state >> 16) & 0x7fff) % max;
}
static double pizza_frand(void)
{
    return pizza_random(10000) / 10000.0;
}

/* Draw a filled circle */
static void fill_circle(Drawable d, GC gc, int cx, int cy, int r)
{
    XFillArc(dpy, d, gc, cx - r, cy - r, r * 2, r * 2, 0, 360 * 64);
}

/* Draw a filled arc segment */
static void fill_arc(Drawable d, GC gc, int cx, int cy, int r,
                     int start_deg, int extent_deg)
{
    XFillArc(dpy, d, gc, cx - r, cy - r, r * 2, r * 2,
             start_deg * 64, extent_deg * 64);
}

/* Draw a circle outline */
static void draw_circle(Drawable d, GC gc, int cx, int cy, int r)
{
    XDrawArc(dpy, d, gc, cx - r, cy - r, r * 2, r * 2, 0, 360 * 64);
}

/* Sprinkle a topping: place count items randomly on the pizza surface.
 * This mirrors the NeWS Sprinkle procedure which rotates randomly,
 * translates by sqrt(random) (for uniform disc distribution),
 * then draws the shape. */
typedef void (*sprinkle_func)(Drawable d, GC gc, int cx, int cy,
                              int radius, int x, int y);

static void sprinkle_topping(Drawable d, GC gc, int cx, int cy, int radius,
                             int count, unsigned long color,
                             sprinkle_func draw_fn)
{
    int i;
    XSetForeground(dpy, gc, color);
    for (i = 0; i < count; i++) {
        double angle = pizza_frand() * 2.0 * M_PI;
        double dist = sqrt(pizza_frand()) * (radius - 4);
        int x = cx + (int)(cos(angle) * dist);
        int y = cy + (int)(sin(angle) * dist);
        draw_fn(d, gc, cx, cy, radius, x, y);
    }
}

/* --- Individual topping draw functions (from NeWS PostScript) --- */

/* Cheese: small arcs, like the NeWS version's random arc strokes */
static void draw_cheese_bit(Drawable d, GC gc, int cx, int cy,
                            int radius, int x, int y)
{
    int r = 3 + pizza_random(6);
    int start = pizza_random(360);
    int extent = 30 + pizza_random(100);
    XDrawArc(dpy, d, gc, x - r, y - r, r * 2, r * 2,
             start * 64, extent * 64);
}

/* Pepperoni: filled circles (like the NeWS "0 0 .05 0 360 arc fill") */
static void draw_pepperoni_bit(Drawable d, GC gc, int cx, int cy,
                               int radius, int x, int y)
{
    int r = 4 + pizza_random(3);
    XFillArc(dpy, d, gc, x - r, y - r, r * 2, r * 2, 0, 360 * 64);
}

/* Mushroom: filled wedge + stem (from NeWS arc + rect) */
static void draw_mushroom_bit(Drawable d, GC gc, int cx, int cy,
                              int radius, int x, int y)
{
    int r = 4 + pizza_random(3);
    /* Cap */
    XFillArc(dpy, d, gc, x - r, y - r, r * 2, r, 0, 360 * 64);
    /* Stem */
    XFillRectangle(dpy, d, gc, x - 1, y, 3, r);
}

/* Sausage: random polygon blobs (from NeWS random lineto...fill) */
static void draw_sausage_bit(Drawable d, GC gc, int cx, int cy,
                             int radius, int x, int y)
{
    XPoint pts[8];
    int i, n = 5 + pizza_random(4);
    if (n > 8) n = 8;
    for (i = 0; i < n; i++) {
        pts[i].x = x + pizza_random(8) - 4;
        pts[i].y = y + pizza_random(8) - 4;
    }
    XFillPolygon(dpy, d, gc, pts, n, Nonconvex, CoordModeOrigin);
}

/* Olive: ring (filled circle with hole, like NeWS eofill donut) */
static void draw_olive_bit(Drawable d, GC gc, int cx, int cy,
                           int radius, int x, int y)
{
    int r = 3;
    XFillArc(dpy, d, gc, x - r, y - r, r * 2, r * 2, 0, 360 * 64);
    /* Pimento hole */
    XSetForeground(dpy, gc, pizza_colors[PC_SAUCE]);
    XFillArc(dpy, d, gc, x - 1, y - 1, 2, 2, 0, 360 * 64);
    XSetForeground(dpy, gc, pizza_colors[PC_OLIVE]);
}

/* Onion: arcs (like NeWS "random arc stroke") */
static void draw_onion_bit(Drawable d, GC gc, int cx, int cy,
                           int radius, int x, int y)
{
    int r = 3 + pizza_random(8);
    int start = pizza_random(360);
    int extent = 30 + pizza_random(50);
    XDrawArc(dpy, d, gc, x - r, y - r, r * 2, r * 2,
             start * 64, extent * 64);
}

/* Bell pepper: arc segments (like NeWS "0 0 .2 0 40 arc stroke") */
static void draw_pepper_bit(Drawable d, GC gc, int cx, int cy,
                            int radius, int x, int y)
{
    int r = 5 + pizza_random(8);
    int start = pizza_random(360);
    XSetLineAttributes(dpy, gc, 2, LineSolid, CapRound, JoinRound);
    XDrawArc(dpy, d, gc, x - r, y - r, r * 2, r * 2,
             start * 64, 40 * 64);
    XSetLineAttributes(dpy, gc, 0, LineSolid, CapButt, JoinMiter);
}

/* Pineapple: wedge (like NeWS "0 0 .07 0 60 arc fill") */
static void draw_pineapple_bit(Drawable d, GC gc, int cx, int cy,
                               int radius, int x, int y)
{
    XFillArc(dpy, d, gc, x - 4, y - 4, 8, 8, 0, 60 * 64);
}

/* Anchovy: half-pill shape (from NeWS arc + lineto) */
static void draw_anchovy_bit(Drawable d, GC gc, int cx, int cy,
                             int radius, int x, int y)
{
    int angle = pizza_random(360);
    int dx = (int)(cos(angle * M_PI / 180.0) * 5);
    int dy = (int)(sin(angle * M_PI / 180.0) * 5);
    XSetLineAttributes(dpy, gc, 2, LineSolid, CapRound, JoinRound);
    XDrawLine(dpy, d, gc, x - dx, y - dy, x + dx, y + dy);
    XSetLineAttributes(dpy, gc, 0, LineSolid, CapButt, JoinMiter);
}

/* Jalapeno: small filled circle with seeds (like NeWS eofill ring) */
static void draw_jalapeno_bit(Drawable d, GC gc, int cx, int cy,
                              int radius, int x, int y)
{
    int r = 3;
    XFillArc(dpy, d, gc, x - r, y - r, r * 2, r * 2, 0, 360 * 64);
    /* Seeds */
    XSetForeground(dpy, gc, pizza_colors[PC_CHEESE]);
    XFillRectangle(dpy, d, gc, x - 1, y, 1, 2);
    XFillRectangle(dpy, d, gc, x + 1, y, 1, 2);
    XSetForeground(dpy, gc, pizza_colors[PC_JALAPENO]);
}

/* BBQ Chicken: rectangles (like NeWS ham ".07 .03 rectpath fill") */
static void draw_bbq_bit(Drawable d, GC gc, int cx, int cy,
                         int radius, int x, int y)
{
    XFillRectangle(dpy, d, gc, x - 3, y - 2, 7, 3);
}

/* Artichoke heart: small wedge (like NeWS "0 0 .07 0 100 arc fill") */
static void draw_artichoke_bit(Drawable d, GC gc, int cx, int cy,
                               int radius, int x, int y)
{
    XFillArc(dpy, d, gc, x - 5, y - 5, 10, 10, 0, 100 * 64);
}

/* Extra cheese: bigger arcs (like NeWS cheese but thicker strokes) */
static void draw_extracheese_bit(Drawable d, GC gc, int cx, int cy,
                                 int radius, int x, int y)
{
    int r = 4 + pizza_random(8);
    int start = pizza_random(360);
    int extent = 30 + pizza_random(120);
    XSetLineAttributes(dpy, gc, 2, LineSolid, CapRound, JoinRound);
    XDrawArc(dpy, d, gc, x - r, y - r, r * 2, r * 2,
             start * 64, extent * 64);
    XSetLineAttributes(dpy, gc, 0, LineSolid, CapButt, JoinMiter);
}

/* Table mapping topping index -> draw function, color, sprinkle count */
typedef struct {
    sprinkle_func draw;
    int color_idx;
    int count;  /* base sprinkle count, scaled by pizza size */
} ToppingVisual;

/* Indexed same as the toppings[] array */
static ToppingVisual topping_visuals[] = {
    { draw_pepperoni_bit, PC_PEPPERONI,  60 },  /* Pepperoni */
    { draw_mushroom_bit,  PC_MUSHROOM,   50 },  /* Mushrooms */
    { draw_sausage_bit,   PC_SAUSAGE,    50 },  /* Sausage */
    { draw_extracheese_bit,PC_EXTRACHS,  80 },  /* Extra Cheese */
    { draw_onion_bit,     PC_ONION,      60 },  /* Onions */
    { draw_pepper_bit,    PC_PEPPER,     50 },  /* Green Peppers */
    { draw_olive_bit,     PC_OLIVE,      50 },  /* Black Olives */
    { draw_pineapple_bit, PC_PINEAPPLE,  50 },  /* Pineapple */
    { draw_anchovy_bit,   PC_ANCHOVY,    40 },  /* Anchovies */
    { draw_jalapeno_bit,  PC_JALAPENO,   50 },  /* Jalapenos */
    { draw_bbq_bit,       PC_BBQ,        50 },  /* BBQ Chicken */
    { draw_artichoke_bit, PC_ARTICHOKE,  50 },  /* Artichoke Hearts */
};

/* Pizza size -> radius fraction (from NeWS: {.5555 .7778 .8889 1}) */
static double pizza_size_frac[] = { 0.5555, 0.7778, 0.8889, 1.0 };
/* Pizza size labels for the title */
static const char *pizza_size_label[] = { "10\"", "14\"", "16\"", "18\"" };

/* Draw the full pizza into a pixmap/drawable */
static void draw_pizza(Drawable d, int w, int h)
{
    int cx = w / 2, cy = h / 2;
    int max_r = (w < h ? w : h) / 2 - 6;
    int pizza_r, crust_outer, sauce_r;
    double frac;
    int i, count;

    init_pizza_colors();

    /* Seed the random so the pizza looks consistent while resizing */
    pizza_rand_state = 42;

    frac = pizza_size_frac[selected_size];
    pizza_r = (int)(max_r * frac);
    crust_outer = (int)(pizza_r * 1.02);
    sauce_r = (int)(pizza_r * 0.90);

    /* Clear background */
    XSetForeground(dpy, pizza_gc, pizza_colors[PC_BG]);
    XFillRectangle(dpy, d, pizza_gc, 0, 0, w, h);

    /* Draw "plate" / table surface hint */
    XSetForeground(dpy, pizza_gc, pizza_colors[PC_CRUST_DARK]);
    fill_circle(d, pizza_gc, cx, cy, crust_outer + 4);

    /* Crust ring (from NeWS: CrustColor eofill between 1.02 and .895 arcs) */
    XSetForeground(dpy, pizza_gc, pizza_colors[PC_CRUST]);
    fill_circle(d, pizza_gc, cx, cy, crust_outer);

    /* Crust highlight */
    XSetForeground(dpy, pizza_gc, pizza_colors[PC_CRUST_LIGHT]);
    XDrawArc(dpy, d, pizza_gc,
             cx - crust_outer + 2, cy - crust_outer + 2,
             (crust_outer - 2) * 2, (crust_outer - 2) * 2,
             30 * 64, 120 * 64);

    /* Sauce (from NeWS: SauceColor fill, radius .9) */
    XSetForeground(dpy, pizza_gc, pizza_colors[PC_SAUCE]);
    fill_circle(d, pizza_gc, cx, cy, sauce_r);

    /* Sauce shadow ring */
    XSetForeground(dpy, pizza_gc, pizza_colors[PC_SAUCE_DARK]);
    draw_circle(d, pizza_gc, cx, cy, sauce_r - 1);

    /* Cheese base layer (from NeWS: 150 sprinkles) */
    sprinkle_topping(d, pizza_gc, cx, cy, sauce_r - 2,
                     100 + (int)(50 * frac),
                     pizza_colors[PC_CHEESE], draw_cheese_bit);

    /* Draw selected toppings */
    for (i = 0; i < (int)NUM_TOPPINGS; i++) {
        if (XmToggleButtonGetState(topping_toggles[i])) {
            ToppingVisual *tv = &topping_visuals[i];
            count = (int)(tv->count * frac * 0.7);
            if (count < 15) count = 15;
            sprinkle_topping(d, pizza_gc, cx, cy, sauce_r - 4,
                             count, pizza_colors[tv->color_idx],
                             tv->draw);
        }
    }

    /* Size label in corner */
    XSetForeground(dpy, pizza_gc, pizza_colors[PC_CRUST_LIGHT]);
    {
        char label[32];
        sprintf(label, "%s %s", pizza_size_label[selected_size],
                sizes[selected_size]);
        XDrawString(dpy, d, pizza_gc, 6, h - 6, label, strlen(label));
    }
}

/* Redraw the pizza preview */
static void redraw_pizza_preview(void)
{
    Dimension w, h;
    char title[128];

    if (!pizza_shell || !XtIsRealized(pizza_shell)) return;
    if (!pizza_drawing) return;

    XtVaGetValues(pizza_drawing, XmNwidth, &w, XmNheight, &h, NULL);
    if (w == 0 || h == 0) return;

    /* Allocate or resize backing pixmap */
    if (!pizza_pixmap || pizza_pix_w != (int)w || pizza_pix_h != (int)h) {
        if (pizza_pixmap)
            XFreePixmap(dpy, pizza_pixmap);
        pizza_pixmap = XCreatePixmap(dpy, XtWindow(pizza_drawing),
                                     w, h, DefaultDepth(dpy, screen_num));
        pizza_pix_w = w;
        pizza_pix_h = h;
    }

    if (!pizza_gc) {
        pizza_gc = XCreateGC(dpy, pizza_pixmap, 0, NULL);
    }

    /* Draw pizza into backing pixmap */
    draw_pizza(pizza_pixmap, (int)w, (int)h);

    /* Copy to window */
    XCopyArea(dpy, pizza_pixmap, XtWindow(pizza_drawing), pizza_gc,
              0, 0, w, h, 0, 0);

    /* Update window title with current size */
    sprintf(title, "Pizza Preview - %s %s",
            pizza_size_label[selected_size], sizes[selected_size]);
    XtVaSetValues(pizza_shell, XmNtitle, title, NULL);
}

static void pizza_expose_cb(Widget w, XtPointer client, XtPointer call)
{
    if (pizza_pixmap) {
        Dimension ww, hh;
        XtVaGetValues(w, XmNwidth, &ww, XmNheight, &hh, NULL);
        if (pizza_gc)
            XCopyArea(dpy, pizza_pixmap, XtWindow(w), pizza_gc,
                      0, 0, ww, hh, 0, 0);
    } else {
        redraw_pizza_preview();
    }
}

static void pizza_resize_cb(Widget w, XtPointer client, XtPointer call)
{
    redraw_pizza_preview();
}

static void create_pizza_window(void)
{
    if (pizza_shell) return; /* already created */

    pizza_shell = XtVaCreatePopupShell("pizza_preview",
        topLevelShellWidgetClass, toplevel_w,
        XmNtitle, "Pizza Preview - Mr. T's Kitchen",
        XmNwidth, 320,
        XmNheight, 320,
        XmNminWidth, 150,
        XmNminHeight, 150,
        XmNdeleteResponse, XmUNMAP,
        NULL);

    pizza_drawing = XtVaCreateManagedWidget("pizza_canvas",
        xmDrawingAreaWidgetClass, pizza_shell,
        NULL);

    XtAddCallback(pizza_drawing, XmNexposeCallback, pizza_expose_cb, NULL);
    XtAddCallback(pizza_drawing, XmNresizeCallback, pizza_resize_cb, NULL);
}

static void show_pizza_cb(Widget w, XtPointer client, XtPointer call)
{
    if (!pizza_shell) create_pizza_window();
    XtPopup(pizza_shell, XtGrabNone);

    /* Force initial draw after mapping */
    XtRealizeWidget(pizza_shell);
    /* Delay the first draw to let the window map */
    redraw_pizza_preview();
}


/* ================================================================
 * DRAWING CALLBACKS
 * ================================================================ */

static void draw_mrt(Widget w)
{
    Dimension width, height;
    int ix, iy;
    MrtImage *img;

    XtVaGetValues(w, XmNwidth, &width, XmNheight, &height, NULL);
    if (width == 0 || height == 0) return;

    /* Clear background */
    XSetForeground(dpy, draw_gc, bg_color);
    XFillRectangle(dpy, XtWindow(w), draw_gc, 0, 0, width, height);

    /* Draw the current Mr T image */
    img = &images[current_image];
    if (img->loaded && img->ximage) {
        /* Center the image */
        ix = ((int)width - img->width) / 2;
        iy = ((int)height - img->height) / 2;
        if (ix < 0) ix = 0;
        if (iy < 0) iy = 0;

        XPutImage(dpy, XtWindow(w), draw_gc,
                  img->ximage, 0, 0, ix, iy,
                  img->width < (int)width ? img->width : (int)width,
                  img->height < (int)height ? img->height : (int)height);
    } else {
        /* Fallback: draw text if image not loaded */
        const char *msg = "[ Mr. T image not found ]";
        XSetForeground(dpy, draw_gc, red_color);
        XDrawString(dpy, XtWindow(w), draw_gc,
                    ((int)width - (int)strlen(msg) * 7) / 2,
                    (int)height / 2, msg, strlen(msg));
    }

    /* Draw caption underneath */
    {
        const char *cap;
        int tx, ty;
        int cap_y_offset;

        if (img->loaded) {
            iy = ((int)height - img->height) / 2;
            cap_y_offset = iy + img->height + 14;
        } else {
            cap_y_offset = (int)height / 2 + 20;
        }

        /* Pick caption based on which kind of image */
        if (current_image == 3 || current_image == 8)
            cap = "\"I PITY THE FOOL!\"";
        else if (current_image == 2 || current_image == 7 || current_image == 11)
            cap = "\"Not bad, sucka!\"";
        else if (current_image == 5 || current_image == 9)
            cap = "\"Now THAT'S what I'm talkin' about!\"";
        else if (current_image == 4)
            cap = "\"Mr. T is NOT impressed.\"";
        else
            cap = "\"I'm watchin' you, fool!\"";

        XSetForeground(dpy, draw_gc, red_color);
        tx = ((int)width - (int)strlen(cap) * 7) / 2;
        ty = cap_y_offset;
        if (ty < (int)height - 4 && ty > 0)
            XDrawString(dpy, XtWindow(w), draw_gc,
                        tx > 0 ? tx : 4, ty, cap, strlen(cap));
    }
}

static void expose_cb(Widget w, XtPointer client, XtPointer call)
{
    draw_mrt(w);
}


/* ================================================================
 * TOPPING TOGGLE CALLBACK
 * ================================================================ */

static void topping_cb(Widget w, XtPointer client, XtPointer call)
{
    int idx = (int)(long)client;
    XmToggleButtonCallbackStruct *cbs = (XmToggleButtonCallbackStruct *)call;
    char buf[512];
    int worst = -1;
    int i, any_selected = 0;

    if (cbs->set) {
        /* Show insult and image for this topping */
        set_insult(toppings[idx].insult);
        current_image = toppings[idx].image_idx;
    } else {
        /* Topping deselected - check remaining */
        for (i = 0; i < (int)NUM_TOPPINGS; i++) {
            if (XmToggleButtonGetState(topping_toggles[i])) {
                any_selected = 1;
                if (worst < 0 || toppings[i].is_terrible > toppings[worst].is_terrible)
                    worst = i;
            }
        }
        if (!any_selected) {
            set_insult("Pick some toppings, fool! Mr. T ain't got all day!");
            current_image = 0; /* default pointing */
        } else {
            sprintf(buf, "Alright fool, you still got %s on there...",
                    toppings[worst].name);
            set_insult(buf);
            current_image = toppings[worst].image_idx;
        }
    }

    if (XtIsRealized(drawing_area))
        draw_mrt(drawing_area);

    /* Update pizza preview if visible */
    redraw_pizza_preview();
}


/* ================================================================
 * ORDER BUTTON
 * ================================================================ */

static void order_cb(Widget w, XtPointer client, XtPointer call)
{
    char order_buf[2048];
    int i, count = 0, worst_level = 0;
    Widget dialog;
    XmString msg, title;

    strcpy(order_buf, "");
    strcat(order_buf, "==========================================\n");
    strcat(order_buf, "   PIZZAFOOL ORDER - BY MR. T\n");
    strcat(order_buf, "==========================================\n\n");

    sprintf(order_buf + strlen(order_buf), "Size: %s\n\n", sizes[selected_size]);
    strcat(order_buf, "Toppings:\n");

    for (i = 0; i < (int)NUM_TOPPINGS; i++) {
        if (XmToggleButtonGetState(topping_toggles[i])) {
            sprintf(order_buf + strlen(order_buf), "  - %s", toppings[i].name);
            if (toppings[i].is_terrible >= 2)
                strcat(order_buf, "  (MR. T DISAPPROVES!)");
            else if (toppings[i].is_terrible >= 1)
                strcat(order_buf, "  (questionable, fool)");
            strcat(order_buf, "\n");
            count++;
            if (toppings[i].is_terrible > worst_level)
                worst_level = toppings[i].is_terrible;
        }
    }

    if (count == 0) {
        set_insult(empty_quotes[rand() % NUM_EMPTY_QUOTES]);
        current_image = 3; /* angry */
        if (XtIsRealized(drawing_area))
            draw_mrt(drawing_area);
        return;
    }

    sprintf(order_buf + strlen(order_buf),
            "\nTotal toppings: %d\n\n", count);

    if (worst_level >= 2) {
        strcat(order_buf, "MR. T SAYS: I pity this pizza! But I'm\n");
        strcat(order_buf, "orderin' it anyway because I ain't no quitter!\n");
        current_image = 8; /* angry2 */
    } else if (worst_level >= 1) {
        strcat(order_buf, "MR. T SAYS: This pizza is... acceptable.\n");
        strcat(order_buf, "But you're on thin crust, fool!\n");
        current_image = 4; /* crossed */
    } else {
        strcat(order_buf, "MR. T SAYS: Now THAT'S a pizza!\n");
        strcat(order_buf, "You ain't such a fool after all, sucka!\n");
        current_image = 9; /* happy2 */
    }

    strcat(order_buf, "\n");
    strcat(order_buf, order_quotes[rand() % NUM_ORDER_QUOTES]);

    msg = XmStringCreateLtoR(order_buf, XmFONTLIST_DEFAULT_TAG);
    title = XmStringCreateSimple("PizzaFool - Your Order, Fool!");

    dialog = XmCreateInformationDialog(w, "order_dialog", NULL, 0);
    XtVaSetValues(dialog,
        XmNmessageString, msg,
        XmNdialogTitle, title,
        NULL);
    XtUnmanageChild(XmMessageBoxGetChild(dialog, XmDIALOG_CANCEL_BUTTON));
    XtUnmanageChild(XmMessageBoxGetChild(dialog, XmDIALOG_HELP_BUTTON));
    XtManageChild(dialog);

    XmStringFree(msg);
    XmStringFree(title);

    set_insult(order_quotes[rand() % NUM_ORDER_QUOTES]);
    if (XtIsRealized(drawing_area))
        draw_mrt(drawing_area);
}


/* ================================================================
 * SIZE CALLBACKS
 * ================================================================ */

static void size_cb(Widget w, XtPointer client, XtPointer call)
{
    int idx = (int)(long)client;
    char buf[256];
    selected_size = idx;

    switch (idx) {
    case 0:
        sprintf(buf, "SMALL?! What are you, on a DIET, fool?! Mr. T eats LARGE!");
        break;
    case 1:
        sprintf(buf, "Medium is for people who can't make up their mind, sucka!");
        break;
    case 2:
        sprintf(buf, "MR. T SIZE! Now you're talkin'! That's the right choice, fool!");
        break;
    case 3:
        sprintf(buf, "A-TEAM SIZE! You must be feedin' an army! Mr. T approves, sucka!");
        break;
    }
    current_image = size_images[idx];
    set_insult(buf);
    if (XtIsRealized(drawing_area))
        draw_mrt(drawing_area);

    /* Update pizza preview if visible */
    redraw_pizza_preview();
}


/* ================================================================
 * QUIT
 * ================================================================ */

static void quit_cb(Widget w, XtPointer client, XtPointer call)
{
    Widget dialog;
    XmString msg, title;

    msg = XmStringCreateLtoR(
        "You're LEAVING?!\n\n"
        "I pity the fool who quits PizzaFool!\n\n"
        "But Mr. T respects your decision.\n"
        "Now go drink some milk, sucka!",
        XmFONTLIST_DEFAULT_TAG);
    title = XmStringCreateSimple("Mr. T Says Goodbye");

    dialog = XmCreateInformationDialog(w, "quit_dialog", NULL, 0);
    XtVaSetValues(dialog,
        XmNmessageString, msg,
        XmNdialogTitle, title,
        NULL);
    XtUnmanageChild(XmMessageBoxGetChild(dialog, XmDIALOG_CANCEL_BUTTON));
    XtUnmanageChild(XmMessageBoxGetChild(dialog, XmDIALOG_HELP_BUTTON));
    XtAddCallback(dialog, XmNokCallback,
        (XtCallbackProc)exit, (XtPointer)0);
    XtManageChild(dialog);

    XmStringFree(msg);
    XmStringFree(title);
}


/* ================================================================
 * HELPER: set insult text
 * ================================================================ */

static void set_insult(const char *text)
{
    XmTextSetString(insult_text, (char *)text);
    XmTextSetInsertionPosition(insult_text, 0);
}


/* ================================================================
 * FIND IMAGE DIRECTORY
 * ================================================================ */

static const char *find_image_dir(const char *argv0)
{
    static char buf[512];
    const char *env;
    char *slash;

    /* 1. Check PIZZAFOOL_IMAGES env var */
    env = getenv("PIZZAFOOL_IMAGES");
    if (env && env[0]) return env;

    /* 2. Try ./images relative to binary location */
    strncpy(buf, argv0, sizeof(buf) - 20);
    buf[sizeof(buf) - 20] = '\0';
    slash = strrchr(buf, '/');
    if (slash) {
        strcpy(slash + 1, "images");
    } else {
        strcpy(buf, "./images");
    }

    /* Check if directory exists */
    {
        FILE *f;
        char test[560];
        sprintf(test, "%s/mrt_point1.xpm", buf);
        f = fopen(test, "r");
        if (f) { fclose(f); return buf; }
    }

    /* 3. Fallback: try ./images */
    return "./images";
}


/* ================================================================
 * MAIN
 * ================================================================ */

int main(int argc, char *argv[])
{
    XtAppContext app;
    Widget main_form, left_frame, right_frame;
    Widget topping_rc, size_rc, button_rc;
    Widget lbl, sep, order_btn, quit_btn, pizza_btn;
    Widget left_form;
    Widget title_lbl, size_frame, size_label;
    XmString str;
    int i;
    struct utsname uts;
    char title[128];
    const char *img_dir;

    srand(time(NULL));

    toplevel_w = XtVaAppInitialize(&app, "PizzaFool",
        NULL, 0, &argc, argv, NULL,
        XmNminWidth, 640,
        XmNminHeight, 520,
        XmNwidth, 660,
        XmNheight, 540,
        NULL);

    uname(&uts);
    sprintf(title, "PizzaFool - Mr. T's Pizza Ordering System (%s)", uts.nodename);
    XtVaSetValues(toplevel_w, XmNtitle, title, NULL);

    dpy = XtDisplay(toplevel_w);
    screen_num = DefaultScreen(dpy);

    /* Allocate colors */
    bg_color = alloc_color("gray85", WhitePixel(dpy, screen_num));
    red_color = alloc_color("red3", BlackPixel(dpy, screen_num));

    /* Create GC */
    draw_gc = XCreateGC(dpy, RootWindow(dpy, screen_num), 0, NULL);

    /* Initialize color cache for 8-bit display handling */
    init_color_cache(dpy, screen_num);

    /* Load Mr T images */
    img_dir = find_image_dir(argv[0]);
    printf("PizzaFool: Looking for images in: %s\n", img_dir);
    load_all_images(dpy, screen_num, img_dir);

    /* ---- Main form ---- */
    main_form = XtVaCreateManagedWidget("main_form",
        xmFormWidgetClass, toplevel_w, NULL);

    /* ---- Title banner ---- */
    str = XmStringCreateLtoR(
        "PIZZAFOOL\n\"I pity the fool who orders bad pizza!\" - Mr. T",
        XmFONTLIST_DEFAULT_TAG);
    title_lbl = XtVaCreateManagedWidget("title",
        xmLabelWidgetClass, main_form,
        XmNlabelString, str,
        XmNtopAttachment, XmATTACH_FORM,
        XmNleftAttachment, XmATTACH_FORM,
        XmNrightAttachment, XmATTACH_FORM,
        XmNtopOffset, 4,
        XmNleftOffset, 4,
        XmNrightOffset, 4,
        XmNalignment, XmALIGNMENT_CENTER,
        NULL);
    XmStringFree(str);

    sep = XtVaCreateManagedWidget("sep1",
        xmSeparatorWidgetClass, main_form,
        XmNtopAttachment, XmATTACH_WIDGET,
        XmNtopWidget, title_lbl,
        XmNtopOffset, 2,
        XmNleftAttachment, XmATTACH_FORM,
        XmNrightAttachment, XmATTACH_FORM,
        NULL);

    /* ---- Insult text area at bottom ---- */
    insult_text = XtVaCreateManagedWidget("insult",
        xmTextWidgetClass, main_form,
        XmNbottomAttachment, XmATTACH_FORM,
        XmNleftAttachment, XmATTACH_FORM,
        XmNrightAttachment, XmATTACH_FORM,
        XmNbottomOffset, 4,
        XmNleftOffset, 4,
        XmNrightOffset, 4,
        XmNeditable, False,
        XmNcursorPositionVisible, False,
        XmNvalue, "Welcome to PizzaFool, fool! Pick your toppings... if you dare, sucka!",
        NULL);

    /* ---- Button row above insult ---- */
    button_rc = XtVaCreateManagedWidget("buttons",
        xmRowColumnWidgetClass, main_form,
        XmNorientation, XmHORIZONTAL,
        XmNbottomAttachment, XmATTACH_WIDGET,
        XmNbottomWidget, insult_text,
        XmNbottomOffset, 4,
        XmNleftAttachment, XmATTACH_FORM,
        XmNrightAttachment, XmATTACH_FORM,
        XmNleftOffset, 4,
        XmNrightOffset, 4,
        XmNpacking, XmPACK_COLUMN,
        XmNentryAlignment, XmALIGNMENT_CENTER,
        NULL);

    str = XmStringCreateSimple("Order Pizza, Fool!");
    order_btn = XtVaCreateManagedWidget("order",
        xmPushButtonWidgetClass, button_rc,
        XmNlabelString, str,
        NULL);
    XtAddCallback(order_btn, XmNactivateCallback, order_cb, NULL);
    XmStringFree(str);

    str = XmStringCreateSimple("Show Pizza, Fool!");
    pizza_btn = XtVaCreateManagedWidget("show_pizza",
        xmPushButtonWidgetClass, button_rc,
        XmNlabelString, str,
        NULL);
    XtAddCallback(pizza_btn, XmNactivateCallback, show_pizza_cb, NULL);
    XmStringFree(str);

    str = XmStringCreateSimple("Quit (Coward!)");
    quit_btn = XtVaCreateManagedWidget("quit",
        xmPushButtonWidgetClass, button_rc,
        XmNlabelString, str,
        NULL);
    XtAddCallback(quit_btn, XmNactivateCallback, quit_cb, NULL);
    XmStringFree(str);

    /* ---- Left side: toppings + size ---- */
    left_form = XtVaCreateManagedWidget("left_form",
        xmFormWidgetClass, main_form,
        XmNtopAttachment, XmATTACH_WIDGET,
        XmNtopWidget, sep,
        XmNtopOffset, 4,
        XmNleftAttachment, XmATTACH_FORM,
        XmNleftOffset, 4,
        XmNbottomAttachment, XmATTACH_WIDGET,
        XmNbottomWidget, button_rc,
        XmNbottomOffset, 4,
        XmNwidth, 280,
        NULL);

    /* Topping frame */
    left_frame = XtVaCreateManagedWidget("topping_frame",
        xmFrameWidgetClass, left_form,
        XmNtopAttachment, XmATTACH_FORM,
        XmNleftAttachment, XmATTACH_FORM,
        XmNrightAttachment, XmATTACH_FORM,
        NULL);

    str = XmStringCreateSimple("Toppings (choose wisely, fool!)");
    lbl = XtVaCreateManagedWidget("topping_label",
        xmLabelWidgetClass, left_frame,
        XmNlabelString, str,
        XmNchildType, XmFRAME_TITLE_CHILD,
        NULL);
    XmStringFree(str);

    topping_rc = XtVaCreateManagedWidget("topping_rc",
        xmRowColumnWidgetClass, left_frame,
        XmNpacking, XmPACK_TIGHT,
        XmNnumColumns, 1,
        NULL);

    for (i = 0; i < (int)NUM_TOPPINGS; i++) {
        str = XmStringCreateSimple((char *)toppings[i].name);
        topping_toggles[i] = XtVaCreateManagedWidget(toppings[i].name,
            xmToggleButtonWidgetClass, topping_rc,
            XmNlabelString, str,
            NULL);
        XtAddCallback(topping_toggles[i], XmNvalueChangedCallback,
                      topping_cb, (XtPointer)(long)i);
        XmStringFree(str);
    }

    /* Size frame */
    size_frame = XtVaCreateManagedWidget("size_frame",
        xmFrameWidgetClass, left_form,
        XmNtopAttachment, XmATTACH_WIDGET,
        XmNtopWidget, left_frame,
        XmNtopOffset, 8,
        XmNleftAttachment, XmATTACH_FORM,
        XmNrightAttachment, XmATTACH_FORM,
        NULL);

    str = XmStringCreateSimple("Size");
    size_label = XtVaCreateManagedWidget("size_label",
        xmLabelWidgetClass, size_frame,
        XmNlabelString, str,
        XmNchildType, XmFRAME_TITLE_CHILD,
        NULL);
    XmStringFree(str);

    size_rc = XtVaCreateManagedWidget("size_rc",
        xmRowColumnWidgetClass, size_frame,
        XmNradioBehavior, True,
        XmNpacking, XmPACK_TIGHT,
        NULL);

    for (i = 0; i < NUM_SIZES; i++) {
        Widget tb;
        str = XmStringCreateSimple((char *)sizes[i]);
        tb = XtVaCreateManagedWidget(sizes[i],
            xmToggleButtonWidgetClass, size_rc,
            XmNlabelString, str,
            XmNset, (i == selected_size) ? True : False,
            NULL);
        XtAddCallback(tb, XmNvalueChangedCallback, size_cb, (XtPointer)(long)i);
        XmStringFree(str);
    }

    /* ---- Right side: Mr T image area ---- */
    right_frame = XtVaCreateManagedWidget("mrt_frame",
        xmFrameWidgetClass, main_form,
        XmNtopAttachment, XmATTACH_WIDGET,
        XmNtopWidget, sep,
        XmNtopOffset, 4,
        XmNleftAttachment, XmATTACH_WIDGET,
        XmNleftWidget, left_form,
        XmNleftOffset, 8,
        XmNrightAttachment, XmATTACH_FORM,
        XmNrightOffset, 4,
        XmNbottomAttachment, XmATTACH_WIDGET,
        XmNbottomWidget, button_rc,
        XmNbottomOffset, 4,
        NULL);

    str = XmStringCreateSimple("Mr. T is watching you...");
    lbl = XtVaCreateManagedWidget("mrt_label",
        xmLabelWidgetClass, right_frame,
        XmNlabelString, str,
        XmNchildType, XmFRAME_TITLE_CHILD,
        NULL);
    XmStringFree(str);

    drawing_area = XtVaCreateManagedWidget("mrt_canvas",
        xmDrawingAreaWidgetClass, right_frame,
        XmNwidth, 300,
        XmNheight, 300,
        NULL);
    XtAddCallback(drawing_area, XmNexposeCallback, expose_cb, NULL);

    /* Realize and go */
    XtRealizeWidget(toplevel_w);

    printf("PizzaFool running on %s (%s %s) - I pity the fool!\n",
           uts.nodename, uts.sysname, uts.machine);
    fflush(stdout);

    XtAppMainLoop(app);
    return 0;
}
