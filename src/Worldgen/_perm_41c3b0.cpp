#include "Worldgen.h"
// FUNCTION: YODA 0x0041c3b0
int World::ZoneProvidesItem(short zoneId, short itemId)
{
    int found;
    int nCount;
    int j;
    int nObjs;
    int i;
    Zone *pZone;
    found = 0;

    pZone = GetZoneById(zoneId);

    if (pZone == NULL)
        return 0;

    nCount = pZone->genCandidateB.GetSize();

    if (itemId == -1)
    {
        found = 1;
        if (nCount <= 0)
            return 0;
    }
    else
    {
        i = 0;
        if (nCount >= 1)
        {
            do
            {
                if ((short)pZone->genCandidateB.GetAt(i) == itemId)
                {
                    found = 1;
                    break;
                }
                i++;
            } while (i < nCount);
        }
        if (found == 0)
        {
            nObjs = pZone->objects.GetSize();
            j = 0;
            if (nObjs >= 1)
            {
                do
                {
                    ZoneObj *pObj = (ZoneObj *)pZone->objects.GetAt(j);
                    if (pObj != NULL && pObj->type == 9)
                    {
                        if (pObj->visible >= 0)
                            found = ZoneProvidesItem(pObj->visible, itemId);
                        if (found == 1)
                            return 1;
                    }
                    j++;
                } while (nObjs > j);
            }
        }
    }

    return found;
}
