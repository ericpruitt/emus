#define AltKey Mod1Mask
#define HyperKey Mod3Mask
#define BothShiftKeys ShiftMask, XK_Shift_R
#define SuperKey Mod4Mask

#define MODKEY HyperKey
#define TAGKEYS(KEY, TAG) \
    { MODKEY,                       KEY,      view,           {.ui = 1 << (TAG - 1)} }, \
    { Mod1Mask,                     KEY,      toggleview,     {.ui = 1 << (TAG - 1)} }, \
    { MODKEY|ControlMask,           KEY,      toggleview,     {.ui = 1 << (TAG - 1)} }, \
    { MODKEY|ShiftMask,             KEY,      tag,            {.ui = 1 << (TAG - 1)} }, \
    { MODKEY|ControlMask|ShiftMask, KEY,      toggletag,      {.ui = 1 << (TAG - 1)} },

#define UNUSED(action) { 0, 0, action, { .i = 0 } },

#define TAG(N) (1 << (N - 1))
#define TAGS2(A, B) (TAG(A) | TAG(B))
#define TAGS3(A, B, C) (TAG(A) | TAG(B) | TAG(C))

#define SHELL(cmd) { .v = (const char*[]) { "/bin/sh", "-c", cmd, NULL } }
#define EXECL(...) { .v = (const char*[]) { __VA_ARGS__, NULL } }

#define CLASS(C) (C), NULL, NULL
#define INSTANCE(I) NULL, (I), NULL
#define TITLE(T) NULL, NULL, (T)
#define CLASS_W_TITLE(C, T) (C), NULL, (T)

#include "dwm-hooks.c"

static const unsigned int borderpx = 1;
static const float mfact           = 0.5;
static const int nmaster           = 1;
static const int resizehints       = 0;
static const int showbar           = 1;
static const unsigned int snap     = 0;
static const int topbar            = 1;

static const unsigned int systraypinning = 0;
static const unsigned int systrayspacing = 4;
static const int showsystray             = 1;
static const int systraypinningfailfirst = 1;

static const char orange_red[]  = "#ff4500";
static const char blue[]        = "#224488";
static const char bright_blue[] = "#0066ff";
static const char black[]       = "#000000";
static const char gray[]        = "#bbbbbb";
static const char white[]       = "#ffffff";

static char *dmenucmd = NULL;
static char *dmenumon = NULL;

static const int defaultpriority = 50;

static const char wmname[] = "LG3D";

static const char fifopath[] = "~/.dwmfifo";

const int nonmasterpriority = TAG(9);

/**
 * Color scheme definitions
 *
 * Each entry in the array is a set of three, hexadecimal colors representing
 * the foreground, background and border color for the specified scheme.
 */
static const char *colors[][3] = {
    // Scheme           Foreground  Background  Border
    [SchemeNorm]    = { gray,       black,      black,      },
    [SchemeSel]     = { white,      blue,       bright_blue },
    [SchemeUrg]     = { white,      orange_red, orange_red  },
};

/**
 * Array of the primary font and fallback fonts
 *
 * The first array is the primary font while the rest of the fonts will be used
 * as fallbacks when preceding fonts are missing a specific glyph.
 */
static const char *fonts[] = {
    "Sans:pixelsize=14",
    "VL Gothic:pixelsize=14",
    "WenQuanYi Micro Hei:pixelsize=14",
};

/**
 * Labels used for each tag
 */
const char *tags[9] = {"1", "2", "3", "4", "5", "6", "7", "8", "Chats"};

/**
 * Window rules
 *
 * To find out a window's class, run "xprop | grep '^WM_CLASS'", then click on
 * the window. The instance is the first item in the list and the class the
 * second.
 */
static const Rule rules[] = {
    // Match conditions                                               Tags     Float  Monitor  Priority
    { TITLE("File Operation Progress"),                               0,       1,     0,       0   },
    { INSTANCE("eog"),                                                0,       1,     0,       0   },
    { INSTANCE("gpick"),                                              0,       1,     0,       0   },
    { CLASS("VirtualBox"),                                            TAG(4),  0,     0,       0   },
    { CLASS("Gimp(-.+)?"),                                            TAG(5),  0,     0,       0   },
    { CLASS("Pidgin"),                                                TAG(9),  0,     0,       20  },
    { CLASS("st-256color|xterm|rxvt"),                                TAG(1),  0,     0,       100 },

    // Firefox and Chrome both go on the same tag. I use Chromium at home and
    // Chrome at work, so Chrome and Firefox are never run together.
    { CLASS("Firefox|Iceweasel"),                                     TAG(2),  0,     0,       0   },
    { INSTANCE("google-chrome"),                                      TAG(2),  0,     0,       0   },
    { INSTANCE("chromium"),                                           TAG(3),  0,     0,       0   },

    // All Wine applications should float by default.
    { CLASS("Wine"),                                                  0,       1,     0,       0   },

    // Google Hangouts Chrome Extension; all Chrome extensions have instance
    // values of "crx_$EXTENSION_ID".
    { INSTANCE("crx_nckgahadagoaajjgafhacjanaoiihapd"),               TAG(9),  0,     0,       20  },
    { INSTANCE("crx_ackdflhoddfmjcmpgallljebbjjllepc"),               TAG(9),  0,     0,       20  },

    // All Steam windows go on the 6th tag, and anything other than the main
    // Steam window should have a reduced attachment priority. When the chat
    // windows are first created, they are created without a properly set
    // title, so dwm defaults to setting the window title to "broken."
    { CLASS_W_TITLE("Steam", "Friends|- Chat|broken|Update News"),    TAG(6),  0,     0,       10  },
    { CLASS("Steam"),                                                 TAG(6),  0,     0,       0   },
};

/**
 * Layout variable names
 *
 * These enum elements can be re-arranged to change the default layout.
 * Deletions from or additions to this list should be accompanied with changes
 * to the "layouts" variable.
 */
enum {
    monocle_layout,
    tile_layout,
    floating_layout,
};

/**
 * Layout symbols and functions
 *
 * Deletions from or additions to this list should be accompanied with changes
 * to the layout variable names enum.
 */
static const Layout layouts[] = {
    [monocle_layout] = { "[M]", monocle },
    [tile_layout] = { "[]=", tile },
    [floating_layout] = { "><>", NULL },
};

/**
 * Layout rules that trigger based on active tags
 */
static const TagRule tagrules[] = {
    // Tag          Exact  Monitor  Mn,Mx  nmaster  Layout Index        mfact
    { TAG(9),       0,     -1,      2, 0,  -1,      tile_layout,        0.668 },
    { TAG(1),       0,     -1,      2, 0,  -1,      tile_layout,        -1    },
    { TAGS2(2, 3),  0,     -1,      2, 0,  -1,      tile_layout,        -1    },
};

/**
 * Keyboard shortcuts
 */
static Key keys[] = {
    // Modifier                     Key                     Function         Argument
    { HyperKey,                     XK_d,                   incnmaster,      {.i = -1 } },
    { HyperKey,                     XK_e,                   toggleview,      {.ui = TAG(9) } },
    { HyperKey,                     XK_i,                   incnmaster,      {.i = +1 } },
    { HyperKey,                     XK_j,                   focusstack,      {.i = +1 } },
    { HyperKey,                     XK_k,                   focusstack,      {.i = -1 } },
    { HyperKey,                     XK_s,                   lastclient,      {0} },
    { HyperKey,                     XK_w,                   killclient,      {0} },
    { HyperKey,                     XK_Return,              zoom,            {0} },
    { AltKey,                       XK_Tab,                 view,            {0} },
    { HyperKey,                     XK_comma,               tagmon,          {.i = -1 } },
    { HyperKey,                     XK_period,              tagmon,          {.i = +1 } },

    // Application launchers
    { BothShiftKeys,                                        spawn,           EXECL("getpass") },
    { HyperKey,                     XK_space,               spawn,           EXECL("del") },
    { HyperKey,                     XK_q,                   spawn,           EXECL("session-control", "menu") },
    { HyperKey,                     XK_b,                   spawn,           EXECL("media-control", "bass-toggle") },
    { HyperKey,                     XK_bracketright,        spawn,           EXECL("media-control", "next-track") },
    { HyperKey,                     XK_0,                   spawn,           EXECL("media-control", "pause-or-play") },
    { HyperKey,                     XK_bracketleft,         spawn,           EXECL("media-control", "previous-track") },
    { HyperKey,                     XK_minus,               spawn,           EXECL("media-control", "volume-down") },
    { HyperKey,                     XK_equal,               spawn,           EXECL("media-control", "volume-up") },
    { 0,                            XK_Print,               spawn,           EXECL("screenshot") },
    { ControlMask|ShiftMask,        XK_l,                   spawn,           EXECL("lock-screen") },
    { HyperKey|ShiftMask,           XK_l,                   spawn,           EXECL("lock-screen") },
    { AltKey,                       XK_Print,               spawn,           EXECL("screenshot", "window") },
    { HyperKey,                     XK_BackSpace,           spawn,           EXECL("xterm", "-e", "/bin/sh") },

    // Layouts
    { HyperKey,                     XK_f,                   setlayout,       {.v = &layouts[floating_layout]} },
    { HyperKey,                     XK_m,                   setlayout,       {.v = &layouts[monocle_layout]} },
    { HyperKey,                     XK_t,                   setlayout,       {.v = &layouts[tile_layout]} },

    TAGKEYS(                        XK_1,                      1)
    TAGKEYS(                        XK_2,                      2)
    TAGKEYS(                        XK_3,                      3)
    TAGKEYS(                        XK_4,                      4)
    TAGKEYS(                        XK_5,                      5)
    TAGKEYS(                        XK_6,                      6)
    TAGKEYS(                        XK_7,                      7)
    TAGKEYS(                        XK_8,                      8)
    TAGKEYS(                        XK_9,                      9)

    UNUSED(focusmon)
    UNUSED(quit)
    UNUSED(setmfact)
    UNUSED(spawn)
    UNUSED(tagmon)
    UNUSED(togglebar)
};

/**
 * Mouse button actions
 *
 * Click can be ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, or
 * ClkRootWin.
 */
static Button buttons[] = {
    // Click                Event mask      Button          Function        Argument

    // Left-click the layout button to toggle between layouts.
    { ClkLtSymbol,          0,              Button1,        setlayout,      {0} },

    // Right-click the layout button to set monocle mode.
    { ClkLtSymbol,          0,              Button3,        setlayout,      {.v = &layouts[monocle_layout]} },

    // Middle-mouse-click a window with Alt or Hyper Key to toggle floating.
    { ClkClientWin,         AltKey,         Button2,        togglefloating, {0} },
    { ClkClientWin,         HyperKey,       Button2,        togglefloating, {0} },

    // Alt + right-click or Hyper Key + right-click and drag to resize a window.
    { ClkClientWin,         AltKey,         Button3,        resizemouse,    {0} },
    { ClkClientWin,         HyperKey,       Button3,        resizemouse,    {0} },

    // Alt + left-click and drag to move a window.
    { ClkClientWin,         HyperKey,       Button1,        movemouse,      {0} },

    // Left-click a tag to switch to that tag alone.
    { ClkTagBar,            0,              Button1,        view,           {0} },

    // Right-click a tag to toggle its status.
    { ClkTagBar,            0,              Button3,        toggleview,     {0} },

    // Hyper Key + left-click to move a window to the clicked tag.
    { ClkTagBar,            HyperKey,       Button1,        tag,            {0} },

    // Hyper Key + right-click a tag to it for the currently selected window.
    { ClkTagBar,            HyperKey,       Button3,        toggletag,      {0} },

    // Scroll wheel up and down on the bar to cycle between windows.
    { ClkWinTitle,          0,              Button5,        focusstack,     {.i = +1} },
    { ClkWinTitle,          0,              Button4,        focusstack,     {.i = -1} },

    // Middle-mouse-click on the title bar to close a window.
    { ClkWinTitle,          0,              Button2,        killclient,     {0} },
};
