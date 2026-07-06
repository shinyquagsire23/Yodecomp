// Worldgen TU (0x41c340–0x429000): the doc class's .dta-load + worldgen + .wld-save source file
// (a third CDeskcppDoc source file, alongside GameData and WorldDoc). All methods are World::.
// This header will accrete the doc-TU World method decls as they are transcribed; see
// docs/worldgen.md for the full algorithm map.
#ifndef WORLDGEN_H
#define WORLDGEN_H
#include <afxwin.h>
#include <afxcoll.h>
#include "../Records/RecordClasses.h"

// The static 10x10 worldgen grid-order priority table (.data 0x00456630).
extern int gWorldgenGridOrderTable[100];

// Minimal World facade for the leaf functions transcribed so far (grows toward the real
// CDeskcppDoc class in WorldDoc.h as the TU fills in).
class World : public CObject
{
public:
    int GetZoneGridOrder(int x, int y);                  // 0x00421e50
};

#endif
