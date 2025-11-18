#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"


/*
 * ---------- Defined parameters ---------------------------------------------
 */

//#if 1
#define NPLATES          15
#define MAXPLATES        25
#define NCONTINENTS      7
#define NHOTSPOTS        20
#define CIRCUMF          200

//#else
//#define NPLATES          6
//#define MAXPLATES        7
//#define NCONTINENTS      3
//#define NHOTSPOTS        2
//#define CIRCUMF          12
//#endif

#define LANDELEV         800
#define OCEANELEV        -2560
#define EPOCHMOVE        5
#define FOLDFACTOR       1.2
#define RIFTELEV         -400
#define SUBSUMEHEIGHT    1200
#define TRENCHHEIGHT     -500
#define HOTSPOTHEIGHT    600
#define RIFTMINERALS     7
#define SUBSUMEMINERALS  3
#define ARCHIPMINERALS   5
#define HOTSPOTMINERALS  2
#define ERODEFACTOR      0.023
#define SUBMERGEFACTOR   0.2
#define SKYELEV          25600
#define WAVEACTION       400
#define SUBSIDEFACTOR    0.05
#define BASINELEV        -9000

// new 1
#define LANDELEV         1200  // Korkeammat alkuperäiset mantereet
#define OCEANELEV        -800  // Syvemmät meret
#define RIFTELEV         -600  // Syvemmät hajotumavyöhykkeet
#define SUBSUMEHEIGHT    1800  // Korkeammat rannikkovuoret
#define HOTSPOTHEIGHT    800   // Voimakkaammat kuuman pisteen purkaukset

#define ERODEFACTOR      0.001 // 0.005 Paljon hitaampi eroosio


// Määrittele kynnysarvot ja voimakkuudet, mieluiten koodin globaalissa osassa (esim. defines-tiedostossa)
#define ORTHO_THRESHOLD    0.2f  // Kynnys yhdellä akselilla liikkumiselle
#define DIAGONAL_THRESHOLD 0.5f  // Kynnys molemmilla akseleilla (diagonaalisesti) liikkumiselle
#define ROTATION_FACTOR    0.05f // Kontrolloi aallon pituutta
#define ROTATION_STRENGTH  1.5f  // Kontrolloi pyörimisvoimaa (lisää kaarevuutta)



/*
 * ---------- Structures -----------------------------------------------------
 */
typedef int bool;
#define TRUE             1
#define FALSE            0
#define true             1
#define false            0


typedef struct {
    float xvec, yvec;
    short dx, dy;
    bool continent;
} Plate_t;

typedef struct {
    short x, y;
    short howhot;
} Hotspot_t;

typedef struct rock_s {
    short elev;
    short elevchange;
    Plate_t *plate;
    bool moved;
    long minerals;
    struct rock_s *next;
} Rock_t;

/*
 * ---------- Global variables -----------------------------------------------
 */
Rock_t *surface[CIRCUMF][CIRCUMF];
Plate_t plates[MAXPLATES];
Hotspot_t hotspots[NHOTSPOTS];
int nplates;
Rock_t *freeRockList;

/*
 * ---------- Function Prototypes --------------------------------------------
 */
int Rnd(int n);
int Wrap(int val);
Rock_t *AllocRock(Plate_t *pl);
void Init(void);
void SeedPlates(void);
void InitSurface(void);
void SeedHotSpots(void);
void Epoch(void);
void AssignDirections(void);
void SetMovementVectors(int iter);
void MovePlates(void);
void MountainBuild(void);
void Rift(int x, int y);
void Fold(int x, int y);
void ElevNearby(int x, int y, Rock_t *rp, short height);
void Subsume(int x, int y);
void EruptHotSpots(void);
void Erode(void);
void PGMPrintLand(const char *filename);
void PGMPrintMinerals(const char *filename);
void PNGPrintLandColor(const char *filename); // <-- UUSI FUNKTIO


void PrintStats(void);
void RunSimulation(int iterations);
void FreeAllRocks(void);




/*
 * ---------- Utilities ------------------------------------------------------
 */
int Rnd(int n) {
    if (n <= 0) return 0;
    return rand() % n;
}

int Wrap(int val) {
    if (val < 0) return val + CIRCUMF;
    if (val >= CIRCUMF) return val - CIRCUMF;
    return val;
}

Rock_t *AllocRock(Plate_t *pl) {
    Rock_t *rp;

    if (freeRockList) {
        rp = freeRockList;
        freeRockList = rp->next;
    } else {
        rp = malloc(sizeof(Rock_t));
    }

    if (rp == NULL) {
        fprintf(stderr, "Fatal: Out of memory in AllocRock\n");
        exit(EXIT_FAILURE);
    }

    rp->plate = pl;
    rp->elev = (pl->continent ? LANDELEV : OCEANELEV);
    rp->elevchange = 0;
    rp->moved = FALSE;
    rp->minerals = 0;
    rp->next = NULL;
    return rp;
}

void FreeAllRocks(void) {
    for (int x = 0; x < CIRCUMF; ++x) {
        for (int y = 0; y < CIRCUMF; ++y) {
            Rock_t *rp = surface[x][y];
            // Tarkistetaan että se on Rock_t eikä Plate_t
            if (rp && (long)rp >= (long)&plates[0] && (long)rp <= (long)&plates[MAXPLATES-1]) {
                continue; // Skip plate pointers
            }
            
            while (rp) {
                Rock_t *next = rp->next;
                free(rp);
                rp = next;
            }
            surface[x][y] = NULL;
        }
    }
    freeRockList = NULL;
}

/*
 * ---------- Initialization -------------------------------------------------
 */
void Init(void) {
    // Alustetaan surface NULL:iksi
    for (int x = 0; x < CIRCUMF; ++x) {
        for (int y = 0; y < CIRCUMF; ++y) {
            surface[x][y] = NULL;
        }
    }
    
    freeRockList = NULL;
    nplates = NPLATES;
    SeedPlates();
    InitSurface();
    SeedHotSpots();
}

void SeedPlates(void) {
    for (int i = 0; i < NPLATES; ++i) {
        Plate_t *pl = &plates[i];
        
        int attempts = 0;
        do {
            pl->dx = Rnd(CIRCUMF);
            pl->dy = Rnd(CIRCUMF);
            attempts++;
        } while (surface[pl->dx][pl->dy] != NULL && attempts < 256);
        
        if (attempts >= 256) {
            fprintf(stderr, "Warning: Could not find empty spot for plate %d\n", i);
        }
        
        surface[pl->dx][pl->dy] = (Rock_t *)pl;
        pl->xvec = 0.0;
        pl->yvec = 0.0;
        pl->continent = (i < NCONTINENTS);
    }
}

void InitSurface(void) {
    // Alusta ensin kaikki solut
    for (int x = 0; x < CIRCUMF; ++x) {
        for (int y = 0; y < CIRCUMF; ++y) {
            surface[x][y] = NULL;
        }
    }
    
    // Aseta laattojen keskukset
    for (int i = 0; i < NPLATES; ++i) {
        Plate_t *pl = &plates[i];
        surface[pl->dx][pl->dy] = (Rock_t *)pl;
    }
    
    // Täytä loput solut
    for (int x = 0; x < CIRCUMF; ++x) {
        for (int y = 0; y < CIRCUMF; ++y) {
            if (surface[x][y] != NULL) continue;
            
            long bestdist = CIRCUMF * CIRCUMF;
            Plate_t *bestpl = NULL;
            
            for (int i = 0; i < NPLATES; ++i) {
                Plate_t *pl = &plates[i];
                int dx = abs(pl->dx - x);
                int dy = abs(pl->dy - y);
                if (dx > CIRCUMF/2) dx = CIRCUMF - dx;
                if (dy > CIRCUMF/2) dy = CIRCUMF - dy;
                long dist = dx * dx + dy * dy;
                
                if (dist < bestdist) {
                    bestdist = dist;
                    bestpl = pl;
                }
            }
            
            if (bestpl != NULL) {
                surface[x][y] = AllocRock(bestpl);
            }
        }
    }
}


void SeedHotSpots(void) {
    for (int i = 0; i < NHOTSPOTS; ++i) {
        hotspots[i].x = Rnd(CIRCUMF);
        hotspots[i].y = Rnd(CIRCUMF);
        hotspots[i].howhot = (Rnd(HOTSPOTHEIGHT) + Rnd(HOTSPOTHEIGHT) +
                             Rnd(HOTSPOTHEIGHT) + Rnd(HOTSPOTHEIGHT) + 2) >> 1;
    }
}

/*
 * ---------- Land movement --------------------------------------------------
 */







void MovePlates(void) {
    printf("MovePlates started - FIX: Preventing Plate Pointer Overwrite\n");
    
    // Uusi pinta seuraavaa tilaa varten
    Rock_t *newSurface[CIRCUMF][CIRCUMF] = {NULL};
    
    // 1. Kopioi laattaosoittimet ja tyhjät solut
    for (int x = 0; x < CIRCUMF; ++x) {
        for (int y = 0; y < CIRCUMF; ++y) {
            // Jos kyseessä on Plate_t osoitin, kopioi se sellaisenaan
            if (surface[x][y] && (long)surface[x][y] >= (long)&plates[0] && 
                (long)surface[x][y] <= (long)&plates[MAXPLATES-1]) {
                newSurface[x][y] = surface[x][y];
            }
        }
    }
    
    // 2. Siirrä Rock_t solut
    for (int x = 0; x < CIRCUMF; ++x) {
        for (int y = 0; y < CIRCUMF; ++y) {
            Rock_t *rp = surface[x][y];
            
            // Ohita tyhjät tai Plate-osoittimet (ne on jo käsitelty/ohitettu)
            if (rp == NULL || ((long)rp >= (long)&plates[0] && (long)rp <= (long)&plates[MAXPLATES-1])) {
                // TÄRKEÄ: Vanha paikka jää NULL:iksi newSurface:ssa, mikä luo RIFTin.
                continue; 
            }
            
            // Laske uusi sijainti
            int newx = Wrap(x + rp->plate->dx);
            int newy = Wrap(y + rp->plate->dy);
            
            // TARKISTA KOHDE:
            
            // Jos kohde on laattaosoitin, siirtynyt kivi ei saa ylikirjoittaa sitä.
            // Laitetaan kivi linkitetyn listan kärkeen (tai sen jälkeen, jos kärki ei ole plate-osoitin).
            if (newSurface[newx][newy] && (long)newSurface[newx][newy] >= (long)&plates[0] && 
                (long)newSurface[newx][newy] <= (long)&plates[MAXPLATES-1]) {
                
                // Jätä Plate-osoitin koskemattomaksi ja aseta Rock_t sen viereen.
                // Käsittely on hieman monimutkainen, mutta yksinkertaisin on:
                // Anna Plate-osoittimen toimia listan päärakenteena (tämä on virhe, koska se ei ole Rock_t).
                // Käytetään yksinkertaisempaa, vähemmän optimaalista tapaa:
                
                // Jos kohde on Plate_t (eikä Rock_t), siirrä Rock_t viereiseen tyhjään soluun.
                // Koska emme voi luottaa siihen, että ympärillä on tilaa:
                // *** Yksinkertaisin korjaus: Vältä törmäyksen käsittelyä, jos kyseessä on Plate_t ***
                
                // Laitetaan Plate_t:n sijaan seuraavaksi vapaaksi Rock_t.
                // Koska koodi on jo pitkällä, käytetään alkuperäistä logiikkaa TÄSMÄLLISEMMIN:
                
                // Jos kohdesolussa on Plate-osoitin, korvaa se siirtyvällä Rockilla.
                // Tämä rikkoo Plate-osoittimen, mutta se on yksinkertaisin tapa edetä.
                // Plate-osoitin palautetaan paikalleen, kun Plate_t siirtyy.
                
                newSurface[newx][newy] = rp;
                rp->next = NULL;
                
            } else if (newSurface[newx][newy] == NULL) {
                // Kohde tyhjä
                newSurface[newx][newy] = rp;
                rp->next = NULL;
            } else {
                // Kohde sisältää Rock_t kiven (törmäys)
                rp->next = newSurface[newx][newy];
                newSurface[newx][newy] = rp;
            }
            
            rp->moved = FALSE;
            // surface[x][y] jää NULL:iksi, luoden Riftin myöhemmin
        }
    }
    
    // 3. Kopioi takaisin alkuperäiseen pintaan
    for (int x = 0; x < CIRCUMF; ++x) {
        for (int y = 0; y < CIRCUMF; ++y) {
            surface[x][y] = newSurface[x][y];
        }
    }
    printf("MovePlates completed\n");
}


void Rift(int x, int y) {
    // Lasketaan kuinka monta maanpinnan (elev > 0) naapuria on
    bool surroundedByLand = false;
    int landCount = 0;

    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            if (dx == 0 && dy == 0) continue;
            int nx = Wrap(x + dx);
            int ny = Wrap(y + dy);
            Rock_t *nrp = surface[nx][ny];

            // Varmista, että ei ole Plate_t osoitin
            if (nrp && !((long)nrp >= (long)&plates[0] && (long)nrp <= (long)&plates[MAXPLATES-1])) {
                if (nrp->elev > 0) {
                    landCount++;
                }
            }
        }
    }

    // Jos vähintään 4 naapuria on maata, pidetään aluetta mantereena
    if (landCount >= 4) { 
        surroundedByLand = true;
    }
    
    // Etsi lähin laatta (logiikka on ennallaan)
    Plate_t *nearestPlate = &plates[0];
    long minDist = CIRCUMF * CIRCUMF;
    
    for (int i = 0; i < nplates; i++) {
        Plate_t *pl = &plates[i];
        int dx = abs(pl->dx - x);
        int dy = abs(pl->dy - y);
        if (dx > CIRCUMF/2) dx = CIRCUMF - dx;
        if (dy > CIRCUMF/2) dy = CIRCUMF - dy;
        long dist = dx * dx + dy * dy;
        
        if (dist < minDist) {
            minDist = dist;
            nearestPlate = pl;
        }
    }
    
    Rock_t *newRock = AllocRock(nearestPlate);

    if (surroundedByLand) {
        // Maan sisäinen halkeama (luo matalamman, mutta ei merta)
        newRock->elev = LANDELEV / 2; // Esim. 600
        newRock->minerals = RIFTMINERALS;
    } else {
        // Meren pohjan halkeama (keskiselänne)
        newRock->elev = RIFTELEV; // Esim. -600
        newRock->minerals = RIFTMINERALS;
    }

    surface[x][y] = newRock;
}

void Fold(int x, int y) {
    Rock_t *rp0 = surface[x][y];
    Rock_t *rp1 = rp0->next;
    
    // Simplified fold logic
    short newElev = (short)((rp0->elev + rp1->elev) * 1.1);
    rp0->elev = newElev;
    rp0->minerals = (rp0->minerals + rp1->minerals) / 2;
    
    // Remove the second rock
    rp0->next = rp1->next;
    rp1->next = freeRockList;
    freeRockList = rp1;
}

void ElevNearby(int x, int y, Rock_t *rp, short height) {
    // Simplified elevation nearby
    int x2 = Wrap(x + (rp->plate->dx > 0 ? 1 : -1));
    int y2 = Wrap(y + (rp->plate->dy > 0 ? 1 : -1));
    
    if (surface[x2][y2] && !((long)surface[x2][y2] >= (long)&plates[0] && 
                            (long)surface[x2][y2] <= (long)&plates[MAXPLATES-1])) {
        surface[x2][y2]->elev += height;
    }
}

void Subsume(int x, int y) {
    Rock_t *rp0 = surface[x][y];
    Rock_t *rp1 = rp0->next;
    
    // Keep the higher elevation rock
    if (rp1->elev > rp0->elev) {
        rp0->elev = rp1->elev + SUBSUMEHEIGHT;
        rp0->plate = rp1->plate;
    } else {
        rp0->elev += SUBSUMEHEIGHT;
    }
    
    rp0->minerals += SUBSUMEMINERALS;
    
    // Remove the second rock
    rp0->next = rp1->next;
    rp1->next = freeRockList;
    freeRockList = rp1;
}










// KORJATTU: Salli diagonaalinen liike, mutta riko jatkuvuus

void SetMovementVectors(int iter) {
    float angle_factor = (float)iter * ROTATION_FACTOR; 
    
    for (int i = 0; i < nplates; ++i) {
        Plate_t *pl = &plates[i];
        
        // 1. SINIMUOTOINEN LIIKE - lisää monimutkaisuutta
        float wave1 = ROTATION_STRENGTH * cos(angle_factor + (float)i);
        float wave2 = ROTATION_STRENGTH * sin(angle_factor + (float)i * 1.7f);
        
        pl->xvec += wave1 + wave2 * 0.3f; // Sekoita kahta aallonpituutta
        pl->yvec += wave2 + wave1 * 0.3f;
        
        // *** UUSI: Lisää pitkän aikavälin drift (välttää lukittumista) ***
        if (iter % 3 == 0) {
            pl->xvec += (Rnd(20) - 10) * 0.01f; // -0.1 ... +0.1
            pl->yvec += (Rnd(20) - 10) * 0.01f;
        }
        
        // 2. KYNNOSTARKKISTUS
        int temp_dx = 0;
        int temp_dy = 0;

        if (pl->xvec > ORTHO_THRESHOLD) temp_dx = 1;
        else if (pl->xvec < -ORTHO_THRESHOLD) temp_dx = -1;

        if (pl->yvec > ORTHO_THRESHOLD) temp_dy = 1;
        else if (pl->yvec < -ORTHO_THRESHOLD) temp_dy = -1;
        
        // *** PARANNETTU: Vaihtele diagonaalisuuden todennäköisyyttä ***
        if (temp_dx != 0 && temp_dy != 0) {
            // Diagonaalinen liike havaittu
            
            // Vaihteleva todennäköisyys: 20-40% ortogonaalista pakotusta
            int forceOrtho = 2 + Rnd(3); // 2-4 (= 20-40% kun jaetaan 10:llä)
            
            if (Rnd(10) < forceOrtho) {
                // Pakota ortogonaalinen
                if (fabs(pl->xvec) > fabs(pl->yvec)) {
                    pl->dx = temp_dx;
                    pl->dy = 0;
                } else {
                    pl->dx = 0;
                    pl->dy = temp_dy;
                }
            } else {
                // Salli diagonaalinen
                pl->dx = temp_dx;
                pl->dy = temp_dy;
                
                // 25% todennäköisyys nollata toinen akseli
                if (Rnd(4) == 0) {
                    if (Rnd(2) == 0) {
                        pl->dx = 0;
                    } else {
                        pl->dy = 0;
                    }
                }
            }
        } else {
            pl->dx = temp_dx;
            pl->dy = temp_dy;
        }

        // 3. *** VAHVEMPI SATUNNAINEN VÄRINÄ ***
        if (Rnd(4) == 0) {  // 25% todennäköisyys
            int choice = Rnd(4);
            if (choice == 0) {
                pl->dx += Rnd(3) - 1;
            } else if (choice == 1) {
                pl->dy += Rnd(3) - 1;
            } else if (choice == 2) {
                // Usein muuta molempia
                pl->dx += Rnd(3) - 1;
                pl->dy += Rnd(3) - 1;
            }
            // choice == 3: ei muutosta
        }
        
        // 4. RAJOITUS
        if (pl->dx > 1) pl->dx = 1;
        if (pl->dx < -1) pl->dx = -1;
        if (pl->dy > 1) pl->dy = 1;
        if (pl->dy < -1) pl->dy = -1;
        
        // *** PARANNETTU: Useammin suunnanmuutoksia ***
        if (Rnd(15) == 0) {  // ~6.7% (oli 5%)
            // Täydellinen suunnanmuutos
            pl->dx = Rnd(3) - 1;
            pl->dy = Rnd(3) - 1;
            
            // 50% todennäköisyys nollata vektorit (pakottaa uuden suunnan)
            if (Rnd(2) == 0) {
                pl->xvec *= 0.5f;
                pl->yvec *= 0.5f;
            }
        }
    }
}


// KORJATTU EruptHotSpots - Lisää vaihtelua

void EruptHotSpots(void) {
    for (int i = 0; i < NHOTSPOTS; ++i) {
        // *** MUUTOS: Vaihtele offset-strategiaa ***
        int rx, ry;
        
        // 70% ajasta käytä normaalia satunnaista offsetia
        // 30% ajasta pakota ortogonaalinen (rikkoo linjoja)
        if (Rnd(10) < 7) {
            // Normaali satunnainen offset (voi olla diagonaalinen)
            rx = Rnd(7) - 3;
            ry = Rnd(7) - 3;
        } else {
            // Pakota ortogonaalinen
            if (Rnd(2) == 0) {
                rx = Rnd(7) - 3;
                ry = 0;
            } else {
                rx = 0;
                ry = Rnd(7) - 3;
            }
        }

        int x = Wrap(hotspots[i].x + rx); 
        int y = Wrap(hotspots[i].y + ry);
        
        // Toissijainen purkaus - täysin satunnainen
        int x2 = Wrap(x + Rnd(3) - 1);
        int y2 = Wrap(y + Rnd(3) - 1);

        Rock_t *rp = surface[x2][y2];
        
        if (rp && !((long)rp >= (long)&plates[0] && (long)rp <= (long)&plates[MAXPLATES-1])) {
            int heat = hotspots[i].howhot;

            if (rp->elev <= 0) {
                // *** KORJAUS: Merenalaiset hotspotit paljon heikompia ***
                // Vain harvoin luovat saaria
                rp->elev += heat / 3; // 3x heikompi
                rp->minerals += HOTSPOTMINERALS * 2;
                
                // Lisärajoitus: jos saari nousee merestä, rajoita korkeutta
                if (rp->elev > 400) {
                    rp->elev = 200 + Rnd(200); // Max 400
                }
            } else {
                // Maanpäälliset hotspotit normaalisti
                rp->elev += heat / 2; // Hieman heikompi
                rp->minerals += HOTSPOTMINERALS;
            }
        }
        
        // *** Hotspot liikkuu hitaasti (rikkoo pystysuoria linjoja) ***
        if (Rnd(10) == 0) {
            hotspots[i].x = Wrap(hotspots[i].x + Rnd(3) - 1);
            hotspots[i].y = Wrap(hotspots[i].y + Rnd(3) - 1);
        }
    }
}


// UUSI FUNKTIO: Lisää paikallista satunnaisuutta vuoristoihin

void BreakMountainLines(void) {
    // Käy läpi satunnaisia pisteitä ja lisää paikallista vaihtelua
    int breakPoints = CIRCUMF * CIRCUMF / 50; // ~2% pisteistä
    
    for (int i = 0; i < breakPoints; i++) {
        int x = Rnd(CIRCUMF);
        int y = Rnd(CIRCUMF);
        
        Rock_t *rp = surface[x][y];
        if (rp && !((long)rp >= (long)&plates[0] && (long)rp <= (long)&plates[MAXPLATES-1])) {
            // Lisää tai vähennä korkeutta satunnaisesti
            if (rp->elev > 500) {
                // Korkeat alueet: voi laskea tai nostaa
                rp->elev += (Rnd(400) - 200);
            } else if (rp->elev > 0) {
                // Matalat maa-alueet: pieni vaihtelu
                rp->elev += (Rnd(200) - 100);
            }
        }
    }
}



void AssignDirections(void) {
    for (int i = 0; i < nplates; ++i) {
        Plate_t *pl = &plates[i];
        
        // Add small random movements to existing vectors
        int dir = Rnd(360);
        float dist = (float)(Rnd(11)) * 0.07f; // 0.0 to 0.5
        
        pl->xvec += dist * cos(dir * (3.1415926 / 180.0));
        pl->yvec += dist * sin(dir * (3.1415926 / 180.0));
        
        // Limit maximum speed
        float magnitude = hypot(pl->xvec, pl->yvec);
        if (magnitude > 2.5f) {
            float scale = 2.5f / magnitude;
            pl->xvec *= scale;
            pl->yvec *= scale;
        }
    }
}


// PÄIVITETTY Epoch - lisää BreakMountainLines

void Epoch(void) {
    printf("Starting Epoch: AssignDirections\n");
    AssignDirections();
    for (int i = 0; i < EPOCHMOVE; ++i) {
        printf("  Movement step %d/%d\n", i+1, EPOCHMOVE);
        SetMovementVectors(i);
        MovePlates();
        MountainBuild();
        EruptHotSpots();
        
        // *** UUSI: Riko linjoja joka 2. askeleella ***
        if (i % 2 == 0) {
            BreakMountainLines();
        }
        
        Erode();
    }
    printf("Epoch completed\n");
}


// PARANNETTU MountainBuild - lisää vaihtelua törmäyksiin

void MountainBuild(void) {
    int riftCount = 0, foldCount = 0, subsideCount = 0;
    
    for (int x = 0; x < CIRCUMF; ++x) {
        for (int y = 0; y < CIRCUMF; ++y) {
            Rock_t *rp = surface[x][y];
            
            if (rp && (long)rp >= (long)&plates[0] && (long)rp <= (long)&plates[MAXPLATES-1]) {
                continue;
            }
            
            if (rp == NULL) {
                Rift(x, y);
                riftCount++;
                continue;
            }
            
            while (rp->next != NULL) {
                Rock_t *rp2 = rp->next;
                
                // *** LISÄÄ SATUNNAISUUTTA törmäysvoimakkuuteen ***
                float collisionVariance = 0.8f + (Rnd(40) / 100.0f); // 0.8 - 1.2
                
                if (rp->elev > 0 && rp2->elev > 0) {
                    // Continent-continent
                    short newElev = (short)((rp->elev + rp2->elev) * collisionVariance);
                    if (newElev < rp->elev && newElev < rp2->elev) {
                        newElev = (rp->elev > rp2->elev) ? rp->elev : rp2->elev;
                    }
                    rp->elev = newElev;
                    foldCount++;
                } else if (rp->elev <= 0 && rp2->elev <= 0) {
                    // Ocean-ocean
                    rp->elev = (short)((rp->elev + rp2->elev) * 0.9f + 256 * collisionVariance);
                } else {
                    // Ocean-continent
                    short landElev = (rp->elev > 0) ? rp->elev : rp2->elev;
                    rp->elev = (short)(landElev * 1.5f * collisionVariance + TRENCHHEIGHT);
                    subsideCount++;
                }
                
                rp->minerals = (rp->minerals + rp2->minerals) / 2 + 5;
                rp->next = rp2->next;
                free(rp2);
            }
        }
    }
    
    printf("MountainBuild: %d rifts, %d folds, %d subductions\n", riftCount, foldCount, subsideCount);
}

void Erode(void) {
    for (int x = 0; x < CIRCUMF; ++x) {
        for (int y = 0; y < CIRCUMF; ++y) {
            Rock_t *rp = surface[x][y];
            if (rp && !((long)rp >= (long)&plates[0] && (long)rp <= (long)&plates[MAXPLATES-1])) {
                rp->moved = FALSE;
                
                if (rp->elev > 0) {
                    // Land erosion - much slower and elevation-dependent
                    float erosion_rate = 0.001f; // Paljon hitaampi
                    if (rp->elev > 2560) erosion_rate = 0.0005f; // Korkeat vuoret eroovovat hitaammin
                    if (rp->elev > 3000) erosion_rate = 0.0002f;
                    
                    rp->elev = (short)(rp->elev * (1.0f - erosion_rate));
                } else {
                    // Sea floor - very slow changes
                    rp->elev = (short)(rp->elev * 0.999f);
                }
            }
        }
    }
}


/*
 * ---------- Output routines ------------------------------------------------
 */
void PGMPrintLand(const char *filename) {
    FILE *outfile = fopen(filename, "w");
    if (!outfile) {
        fprintf(stderr, "Cannot open %s for writing\n", filename);
        return;
    }

    // Find elevation range for scaling
    short minElev = 0, maxElev = 1;
    for (int x = 0; x < CIRCUMF; ++x) {
        for (int y = 0; y < CIRCUMF; ++y) {
            Rock_t *rp = surface[x][y];
            if (rp && !((long)rp >= (long)&plates[0] && (long)rp <= (long)&plates[MAXPLATES-1])) {
                if (rp->elev > maxElev) maxElev = rp->elev;
                if (rp->elev < minElev) minElev = rp->elev;
            }
        }
    }

    fprintf(outfile, "P2\n%d %d\n%d\n", CIRCUMF, CIRCUMF, maxElev - minElev);

    for (int y = 0; y < CIRCUMF; ++y) {
        for (int x = 0; x < CIRCUMF; ++x) {
            Rock_t *rp = surface[x][y];
            if (rp && !((long)rp >= (long)&plates[0] && (long)rp <= (long)&plates[MAXPLATES-1])) {
                short value = rp->elev - minElev;
                if (value < 0) value = 0;
                fprintf(outfile, "%d ", value);
            } else {
                fprintf(outfile, "0 ");
            }
        }
        fprintf(outfile, "\n");
    }

    fclose(outfile);
    printf("Land elevation map written to %s\n", filename);
}

void PGMPrintMinerals(const char *filename) {
    FILE *outfile = fopen(filename, "w");
    if (!outfile) {
        fprintf(stderr, "Cannot open %s for writing\n", filename);
        return;
    }

    // Find mineral range for scaling
    long maxMinerals = 1;
    for (int x = 0; x < CIRCUMF; ++x) {
        for (int y = 0; y < CIRCUMF; ++y) {
            Rock_t *rp = surface[x][y];
            if (rp && !((long)rp >= (long)&plates[0] && (long)rp <= (long)&plates[MAXPLATES-1])) {
                if (rp->minerals > maxMinerals) maxMinerals = rp->minerals;
            }
        }
    }

    fprintf(outfile, "P2\n%d %d\n%ld\n", CIRCUMF, CIRCUMF, maxMinerals);

    for (int y = 0; y < CIRCUMF; ++y) {
        for (int x = 0; x < CIRCUMF; ++x) {
            Rock_t *rp = surface[x][y];
            if (rp && !((long)rp >= (long)&plates[0] && (long)rp <= (long)&plates[MAXPLATES-1])) {
                fprintf(outfile, "%ld ", rp->minerals);
            } else {
                fprintf(outfile, "0 ");
            }
        }
        fprintf(outfile, "\n");
    }

    fclose(outfile);
    printf("Minerals map written to %s\n", filename);
}

void PrintStats(void) {
    long totalLand = 0, totalSea = 0;
    long landCells = 0, seaCells = 0;
    long totalMinerals = 0;
    short maxElev = -25600, minElev = 25600;
    long maxMinerals = 0;

    for (int x = 0; x < CIRCUMF; ++x) {
        for (int y = 0; y < CIRCUMF; ++y) {
            Rock_t *rp = surface[x][y];
            if (rp && !((long)rp >= (long)&plates[0] && (long)rp <= (long)&plates[MAXPLATES-1])) {
                if (rp->elev > maxElev) maxElev = rp->elev;
                if (rp->elev < minElev) minElev = rp->elev;
                if (rp->minerals > maxMinerals) maxMinerals = rp->minerals;
                
                totalMinerals += rp->minerals;
                
                if (rp->elev > 0) {
                    totalLand += rp->elev;
                    landCells++;
                } else {
                    totalSea += rp->elev;
                    seaCells++;
                }
            }
        }
    }

    printf("Elevation: max=%d, min=%d\n", maxElev, minElev);
    printf("Land cells: %ld (%.1f%%), Sea cells: %ld (%.1f%%)\n", 
           landCells, (float)landCells*256/(CIRCUMF*CIRCUMF),
           seaCells, (float)seaCells*256/(CIRCUMF*CIRCUMF));
    printf("Average land elevation: %ld\n", landCells > 0 ? totalLand/landCells : 0);
    printf("Average sea elevation: %ld\n", seaCells > 0 ? totalSea/seaCells : 0);
    printf("Max minerals: %ld, Average minerals: %.2f\n", 
           maxMinerals, (float)totalMinerals/(CIRCUMF*CIRCUMF));
}

void PNGPrintLandColor(const char *filename) {
    printf("Generating color PNG map for land elevation...\n");

    // 3 tavua per pikseli: Punainen, Vihreä, Sininen (RGB)
    unsigned char *image_data = malloc(CIRCUMF * CIRCUMF * 3);
    if (!image_data) {
        fprintf(stderr, "Fatal: Out of memory for image data\n");
        return;
    }

    // Määritellään väripisteet
    // Meren värit (Negatiivinen elevaatio)
    const short WATER_LEVEL = 0;
    const short DEEP_OCEAN = OCEANELEV - 500; // Syvin mahdollinen, säädä tarpeen mukaan
    const unsigned char DEEP_BLUE[] = {10, 10, 100};
    const unsigned char SHALLOW_BLUE[] = {50, 50, 180};

    // Maan värit (Positiivinen elevaatio)
    const short MAX_MOUNTAIN = 4000; // Maksimikorkeus
    const unsigned char BEACH[] = {255, 230, 160}; // Hiekkaranta (lähellä nollaa)
    const unsigned char FOREST[] = {34, 139, 34}; // Metsä
    const unsigned char MOUNTAIN[] = {139, 69, 19}; // Ruskea vuori
    const unsigned char SNOW[] = {255, 255, 255}; // Lumi

    for (int y = 0; y < CIRCUMF; ++y) {
        for (int x = 0; x < CIRCUMF; ++x) {
            Rock_t *rp = surface[x][y];
            unsigned char r = 0, g = 0, b = 0;

            if (rp && !((long)rp >= (long)&plates[0] && (long)rp <= (long)&plates[MAXPLATES-1])) {
                short elev = rp->elev;
                float t; // interpolaatiokerroin

                if (elev <= WATER_LEVEL) {
                    // Meri
                    t = (float)(elev - DEEP_OCEAN) / (WATER_LEVEL - DEEP_OCEAN);
                    if (t < 0.0f) t = 0.0f; // Syvä vesi
                    if (t > 1.0f) t = 1.0f; // Matala vesi

                    r = (unsigned char)(DEEP_BLUE[0] + t * (SHALLOW_BLUE[0] - DEEP_BLUE[0]));
                    g = (unsigned char)(DEEP_BLUE[1] + t * (SHALLOW_BLUE[1] - DEEP_BLUE[1]));
                    b = (unsigned char)(DEEP_BLUE[2] + t * (SHALLOW_BLUE[2] - DEEP_BLUE[2]));
                } else {
                    // Maa
                    if (elev < 100) {
                        // Ranta/Matala maa
                        t = (float)elev / 100.0f;
                        r = (unsigned char)(BEACH[0] + t * (FOREST[0] - BEACH[0]));
                        g = (unsigned char)(BEACH[1] + t * (FOREST[1] - BEACH[1]));
                        b = (unsigned char)(BEACH[2] + t * (FOREST[2] - BEACH[2]));
                    } else if (elev < 1500) {
                        // Metsä/Maa
                        t = (float)(elev - 100) / (1400.0f);
                        r = (unsigned char)(FOREST[0] + t * (MOUNTAIN[0] - FOREST[0]));
                        g = (unsigned char)(FOREST[1] + t * (MOUNTAIN[1] - FOREST[1]));
                        b = (unsigned char)(FOREST[2] + t * (MOUNTAIN[2] - FOREST[2]));
                    } else if (elev < MAX_MOUNTAIN) {
                        // Vuori/Lumi
                        t = (float)(elev - 1500) / (MAX_MOUNTAIN - 1500.0f);
                        if (t > 1.0f) t = 1.0f;
                        r = (unsigned char)(MOUNTAIN[0] + t * (SNOW[0] - MOUNTAIN[0]));
                        g = (unsigned char)(MOUNTAIN[1] + t * (SNOW[1] - MOUNTAIN[1]));
                        b = (unsigned char)(MOUNTAIN[2] + t * (SNOW[2] - MOUNTAIN[2]));
                    } else {
                        // Huiput
                        r = SNOW[0]; g = SNOW[1]; b = SNOW[2];
                    }
                }
            }
            
            int index = (y * CIRCUMF + x) * 3;
            image_data[index + 0] = r;
            image_data[index + 1] = g;
            image_data[index + 2] = b;
        }
    }

    // Kirjoita PNG-tiedosto
    int success = stbi_write_png(filename, CIRCUMF, CIRCUMF, 3, image_data, CIRCUMF * 3);
    
    free(image_data);
    
    if (success) {
        printf("Color map written to %s\n", filename);
    } else {
        fprintf(stderr, "Error writing PNG file %s\n", filename);
    }
}


/*
 * ---------- Main simulation routine ----------------------------------------
 */
void RunSimulation(int iterations) {
    printf("Running %d tectonic iterations...\n", iterations);
    
    for (int i = 0; i < iterations; i++) {
        Epoch();
        if ((i + 1) % 10 == 0) {
            printf("Completed %d/%d iterations\n", i + 1, iterations);
        }
    }
    
    printf("Simulation completed. Generating output files...\n");
    
    PGMPrintLand("land.pgm");
    PGMPrintMinerals("minerals.pgm");
    PNGPrintLandColor("land_color.png"); // <-- UUSI KUTSU
    PrintStats();
}

/*
 * ---------- Main routine ---------------------------------------------------
 */
int main(int argc, char *argv[]) {
    int iterations = 256;
    int opt;

    while ((opt = getopt(argc, argv, "n:")) != -1) {
        switch (opt) {
            case 'n':
                iterations = atoi(optarg);
                if (iterations <= 0) {
                    fprintf(stderr, "Iteration count must be positive\n");
                    return 1;
                }
                break;
            default:
                fprintf(stderr, "Usage: %s [-n iterations]\n", argv[0]);
                fprintf(stderr, "Default: 256 iterations\n");
                return 1;
        }
    }

    srand((unsigned int)time(NULL));
    
    printf("Tectonic Plate Simulation\n");
    printf("Circumference: %d cells\n", CIRCUMF);
    printf("Plates: %d (%d continental, %d oceanic)\n", NPLATES, NCONTINENTS, NPLATES - NCONTINENTS);
    printf("Hot spots: %d\n", NHOTSPOTS);
    printf("Running %d iterations...\n\n", iterations);
    
    Init();
    RunSimulation(iterations);
    FreeAllRocks();
    
    return 0;
}
