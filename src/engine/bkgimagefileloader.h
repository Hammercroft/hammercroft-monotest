#ifndef BKGIMAGEFILELOADER_H
#define BKGIMAGEFILELOADER_H

#include "bkgimage.h"
#include "bkgimagearena.h"

// Function Declaration
struct BkgImage *LoadBkgImagePBM(BkgImageArena *arena, const char *filename);

#endif // BKGIMAGEFILELOADER_H
