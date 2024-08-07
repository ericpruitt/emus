#include "xft.h"

static int topbar = 1;
static unsigned int lines = 0;
static const char worddelimiters[] = " ";
static const char *prompt = NULL;
static int fuzzy = 0;

static const char *fonts[] = {
    XFT_FONT("Sans"),
};

static const char *colors[SchemeLast][2] = {
    [SchemeNorm] = { "#bbbbbb", "#222222" },
    [SchemeSel] = { "#eeeeee", "#5522aa" },
    [SchemeOut] = { "#000000", "#ffffff" },
    [SchemePrompt] = { "#bbbbbb", "#222222" },
};
