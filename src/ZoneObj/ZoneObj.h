// ZoneObj — a placed object / hotspot in a zone (CObject-derived record, 0x10 bytes).
// Default ctor 0x404ed0, parameterized ctor 0x404f60, dtor 0x405100, Read 0x404fe0.
#ifndef ZONEOBJ_H
#define ZONEOBJ_H
#include <afxwin.h>

class ZoneObj : public CObject
{
public:                          // +0x00  CObject vtable (0x44b1a8)
    unsigned int   type;         // +0x04  ObjType category
    short          state;        // +0x08  ==1 active/placed
    short          x;            // +0x0a
    short          y;            // +0x0c
    short          visible;      // +0x0e  (0xffff default)

    ZoneObj();                                                    // 0x00404ed0  default
    ZoneObj(unsigned int type, unsigned short x, unsigned short y);// 0x00404f60  spawn(type,x,y)
    virtual ~ZoneObj();                                           // 0x00405100
    void Read(CFile *pFile);                                      // 0x00404fe0  deserialize
};

#endif
