static int topbar = 1;
static unsigned int lines = 0;
static const char worddelimiters[] = " ";
static const char *prompt = NULL;

static const char *fonts[] = {
    "Sans:pixelsize=14",
};

static const char *colors[SchemeLast][2] = {
    [SchemeNorm] = { "#bbbbbb", "#222222" },
    [SchemeSel] = { "#eeeeee", "#5522aa" },
    [SchemeOut] = { "#000000", "#ffffff" },
    [SchemePrompt] = { "#bbbbbb", "#222222" },
};
