#if UHD_DISPLAY
#define XFT_FONT(spec) (spec ":size=14")
#else
#define XFT_FONT(spec) (spec ":pixelsize=14")
#endif
