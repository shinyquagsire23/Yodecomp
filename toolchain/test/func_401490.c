typedef struct Zone {
    short exists;      /* +0x00 */
    char  pad02[0x1e];
    int   field20;     /* +0x20 */
    int   field24;     /* +0x24 */
    char  pad28[0x0c];
} Zone;                /* 0x34 */

typedef struct World {
    char  pad00[0x58];
    int   totalZones;  /* +0x58 */
    char  pad5c[0x4b4 - 0x5c];
    Zone  zones[100];  /* +0x4b4 */
} World;

int __fastcall World_CalcCompletionScore(World *this)   /* ECX = this */
{
    int   y, x;
    Zone *pZone = this->zones;
    int   score = 0;
    float count = 0.0f;

    for (y = 10; y != 0; y--) {
        for (x = 10; x != 0; x--) {
            if (pZone->exists > 0 && pZone->field24 == 1 && pZone->field20 == 1)
                count = count + 1.0f;
            pZone = (Zone *)((char *)pZone + 0x34);
        }
    }
    {
        float pct = (count / (float)this->totalZones) * 100.0f;
        if      (90.1 <= pct && pct <= 100.0) score = 300;
        else if (80.1 <= pct && pct <=  90.0) score = 270;
        else if (70.1 <= pct && pct <=  80.0) score = 240;
        else if (60.1 <= pct && pct <=  70.0) score = 210;
        else if (50.1 <= pct && pct <=  60.0) score = 180;
        else if (40.1 <= pct && pct <=  50.0) score = 150;
        else if (30.1 <= pct && pct <=  40.0) score = 120;
        else if (20.1 <= pct && pct <=  30.0) score =  90;
        else if (10.1 <= pct && pct <=  20.0) score =  60;
        else if ( 0.0 <= pct && pct <=  10.0) score =  30;
    }
    return score;
}
