
//{{BLOCK(screen)

//======================================================================
//
//	screen, 240x160@4, 
//	+ palette 16 entries, not compressed
//	+ 600 tiles not compressed
//	Total size: 32 + 19200 = 19232
//
//	Time-stamp: 2026-03-14, 20:09:42
//	Exported by Cearn's GBA Image Transmogrifier, v0.8.3
//	( http://www.coranac.com/projects/#grit )
//
//======================================================================

#ifndef GRIT_SCREEN_H
#define GRIT_SCREEN_H

#define screenTilesLen 19200
extern const unsigned short screenTiles[9600];

#define screenPalLen 32
extern const unsigned short screenPal[16];

#endif // GRIT_SCREEN_H

//}}BLOCK(screen)
