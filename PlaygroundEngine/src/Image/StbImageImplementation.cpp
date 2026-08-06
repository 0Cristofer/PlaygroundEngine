// The single translation unit that instantiates the stb single-header libraries. Nothing else
// belongs here: every caller goes through PlaygroundEngine.Image, so stb stays an implementation
// detail of one module. Configuration macros come from the StbImage target.

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <stb_image.h>
#include <stb_image_write.h>
