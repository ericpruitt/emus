static const char *colorname[NUMCOLS] = {
    [INIT]      = "#000000",
    [INPUT]     = "#0044aa",
    [PAM_WAIT]  = "#ffff00",
    [FAILED]    = "#aa0000",
};

// Treat a cleared input like a wrong password
static const int failonclear = 1;
