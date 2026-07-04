// Dta — YodaDemo.dta chunk parsers (module @ 0x422670). See docs/dta-format.md.
// CFile::Read is the virtual at vtable slot 15 (+0x3c); model 15 leading dummy
// virtuals so Read lands at the right offset. No bodies needed (pointer use only).
#ifndef DTA_H
#define DTA_H

struct CFile {
    virtual void _v00(); virtual void _v01(); virtual void _v02(); virtual void _v03();
    virtual void _v04(); virtual void _v05(); virtual void _v06(); virtual void _v07();
    virtual void _v08(); virtual void _v09(); virtual void _v10(); virtual void _v11();
    virtual void _v12(); virtual void _v13(); virtual void _v14();
    virtual unsigned int Read(void *buf, unsigned int n);   // slot 15 -> +0x3c
};

// A zone-auxiliary record; its loader is FUN_00406490 (__thiscall(rec, pFile)).
struct ZoneAux {
    void LoadZaux(CFile *pFile);    // 0x00406270 (ZAUX record)
    void LoadZax2(CFile *pFile);    // 0x00406410 (ZAX2 record)
    void LoadZax3(CFile *pFile);    // 0x00406490 (ZAX3 record)
};

// The game document (CDocument-derived) — partial, only fields the parsers use.
struct GameDoc {
    char   _pad00[0x98];
    void **auxArray;                // +0x98  array of record pointers
    short  auxCount;                // +0x9c

    int ParseZaux(CFile *pFile);    // 0x00423110  (ZAUX)
    int ParseZax3(CFile *pFile);    // 0x00423190  (ZAX3)
    int ParseZax2(CFile *pFile);    // 0x00423210  (ZAX2)
};

#endif
