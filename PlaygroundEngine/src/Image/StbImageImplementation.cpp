// The single translation unit that instantiates stb_image. Nothing else belongs here: every caller
// goes through PlaygroundEngine.Image, so stb stays an implementation detail of one module.
// Configuration macros come from the StbImage target, so they apply here and to consumers alike.

#define STB_IMAGE_IMPLEMENTATION

#include <stb_image.h>
