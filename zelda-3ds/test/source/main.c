// Programme de test 3DS : vérifie la fabrique (devkitARM), l'émulateur et la console.
//
// Écran du haut : un triangle coloré et un carré texturé (texture « à la GameCube » :
// bande rouge en haut, bande verte à gauche, damier bleu et blanc).
// Écran du bas : informations sur la console (modèle, mémoire, vitesse).
// Journal : sdmc:/zelda-3ds/test-journal.txt
#include <3ds.h>
#include <citro3d.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "vshader_shbin.h"

#define COULEUR_FOND 0x203040FF

#define DISPLAY_TRANSFER_FLAGS                                                                      \
    (GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) |              \
     GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
     GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

typedef struct {
    float pos[3];
    u8 couleur[4];
    float tc[2];
} Sommet;

static FILE* g_journal;

// La console de l'écran du bas n'affiche pas les accents : on les retire pour l'écran.
static void sansAccents(const char* src, char* dst, size_t taille) {
    size_t j = 0;
    for (size_t i = 0; src[i] && j + 1 < taille; i++) {
        unsigned char c = (unsigned char)src[i];
        if (c == 0xC3 && src[i + 1]) {
            unsigned char d = (unsigned char)src[++i];
            char r = '?';
            if (d >= 0xA0 && d <= 0xA5) r = 'a';
            else if (d == 0xA7) r = 'c';
            else if (d >= 0xA8 && d <= 0xAB) r = 'e';
            else if (d >= 0xAC && d <= 0xAF) r = 'i';
            else if (d >= 0xB2 && d <= 0xB6) r = 'o';
            else if (d >= 0xB9 && d <= 0xBC) r = 'u';
            else if (d >= 0x80 && d <= 0x85) r = 'A';
            else if (d == 0x87) r = 'C';
            else if (d >= 0x88 && d <= 0x8B) r = 'E';
            dst[j++] = r;
        } else if (c < 0x80) {
            dst[j++] = (char)c;
        }
    }
    dst[j] = 0;
}

static void journal(const char* fmt, ...) {
    char texte[256], ecran[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(texte, sizeof texte, fmt, ap);
    va_end(ap);
    sansAccents(texte, ecran, sizeof ecran);
    printf("%s\n", ecran);
    svcOutputDebugString(texte, strlen(texte));
    if (g_journal) {
        fprintf(g_journal, "%s\n", texte);
        fflush(g_journal);
    }
}

// Ordre des texels de la puce PICA200 : tuiles de 8x8, texels entrelacés (ordre de Morton).
static inline u32 morton8(u32 x, u32 y) {
    return (x & 1) | ((y & 1) << 1) | ((x & 2) << 1) | ((y & 2) << 2) | ((x & 4) << 2) | ((y & 4) << 3);
}

static inline u32 indiceTuile(u32 x, u32 y, u32 largeur) {
    return ((y >> 3) * (largeur >> 3) + (x >> 3)) * 64 + morton8(x & 7, y & 7);
}

// Texture de 64x64 rangée comme sur GameCube : la ligne 0 est en haut de l'image (t = 0).
static void remplirTexture(C3D_Tex* tex) {
    u32* donnees = (u32*)tex->data;
    for (u32 y = 0; y < 64; y++) {
        for (u32 x = 0; x < 64; x++) {
            u32 c;
            if (y < 16) c = 0xFF0000FF;       // rouge en haut
            else if (x < 16) c = 0x00FF00FF;  // vert à gauche
            else c = (((x >> 3) ^ (y >> 3)) & 1) ? 0xFFFFFFFF : 0x0000FFFF;
            donnees[indiceTuile(x, y, 64)] = c;
        }
    }
    C3D_TexFlush(tex);
}

static double mesurerBoucle(void) {
    u64 debut = svcGetSystemTick();
    u32 a = 1;
    for (u32 i = 0; i < 5000000; i++) a += i ^ (a >> 3);
    u64 fin = svcGetSystemTick();
    volatile u32 puits = a;
    (void)puits;
    return (double)(fin - debut) * 1000.0 / SYSCLOCK_ARM11;
}

static volatile u32 g_travailFil[4];

static void filDeTest(void* arg) {
    int coeur = (int)(intptr_t)arg;
    for (u32 i = 0; i < 200000; i++) g_travailFil[coeur]++;
}

// Vérifie sur quels cœurs du processeur le programme peut lancer des fils.
static void testerCoeurs(void) {
    APT_SetAppCpuTimeLimit(30);
    for (int coeur = 0; coeur < 4; coeur++) {
        g_travailFil[coeur] = 0;
        Thread fil = threadCreate(filDeTest, (void*)(intptr_t)coeur, 16 * 1024, 0x31, coeur, false);
        if (fil) {
            threadJoin(fil, U64_MAX);
            threadFree(fil);
            journal("Cœur %d : utilisable (%lu)", coeur, (unsigned long)g_travailFil[coeur]);
        } else {
            journal("Cœur %d : non disponible", coeur);
        }
    }
}

static u32 plusGrosBlocMo(void) {
    // Cherche le plus gros bloc allouable (pas de 1 Mo).
    u32 bas = 0, haut = 256;
    while (bas < haut) {
        u32 milieu = (bas + haut + 1) / 2;
        // volatile : sinon le compilateur supprime la paire malloc/free inutilisée
        char* volatile p = malloc((size_t)milieu << 20);
        if (p) {
            p[0] = 1;
            free(p);
            bas = milieu;
        } else {
            haut = milieu - 1;
        }
    }
    return bas;
}

int main(void) {
    gfxInitDefault();
    consoleInit(GFX_BOTTOM, NULL);

    mkdir("sdmc:/zelda-3ds", 0777);
    g_journal = fopen("sdmc:/zelda-3ds/test-journal.txt", "w");

    journal("=== Zelda 3DS : programme de test ===");
    bool n3ds = false;
    APT_CheckNew3DS(&n3ds);
    journal("Console : %s", n3ds ? "New 3DS" : "3DS (ancien modèle)");
    journal("Lancé comme : %s", envIsHomebrew() ? "3dsx (Homebrew Launcher)" : "CIA (menu HOME)");
    journal("Mémoire de l'application : %lu Ko (libre : %lu Ko)",
            (unsigned long)(osGetMemRegionSize(MEMREGION_APPLICATION) / 1024),
            (unsigned long)(osGetMemRegionFree(MEMREGION_APPLICATION) / 1024));
    journal("Mémoire linéaire libre : %lu Ko, VRAM libre : %lu Ko", (unsigned long)(linearSpaceFree() / 1024),
            (unsigned long)(vramSpaceFree() / 1024));
    journal("Plus gros bloc allouable : %lu Mo", (unsigned long)plusGrosBlocMo());

    double lent = mesurerBoucle();
    osSetSpeedupEnable(true);
    double rapide = mesurerBoucle();
    journal("Boucle de test : %.1f ms (normal), %.1f ms (mode rapide)", lent, rapide);
    testerCoeurs();

    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C3D_RenderTarget* cible = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
    C3D_RenderTargetSetOutput(cible, GFX_TOP, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);

    DVLB_s* dvlb = DVLB_ParseFile((u32*)vshader_shbin, vshader_shbin_size);
    shaderProgram_s programme;
    shaderProgramInit(&programme);
    shaderProgramSetVsh(&programme, &dvlb->DVLE[0]);
    C3D_BindProgram(&programme);
    int uProjection = shaderInstanceGetUniformLocation(programme.vertexShader, "projection");

    C3D_AttrInfo* attributs = C3D_GetAttrInfo();
    AttrInfo_Init(attributs);
    AttrInfo_AddLoader(attributs, 0, GPU_FLOAT, 3);
    AttrInfo_AddLoader(attributs, 1, GPU_UNSIGNED_BYTE, 4);
    AttrInfo_AddLoader(attributs, 2, GPU_FLOAT, 2);

    // Coordonnées d'écran : x vers la droite, y vers le bas (0 en haut), comme sur GameCube.
    static const Sommet modele[] = {
        // Triangle (rouge, vert, bleu)
        {{110, 40, 0.5f}, {255, 0, 0, 255}, {0, 0}},
        {{30, 200, 0.5f}, {0, 255, 0, 255}, {0, 0}},
        {{190, 200, 0.5f}, {0, 0, 255, 255}, {0, 0}},
        // Carré texturé : (s, t) = (0, 0) en haut à gauche
        {{220, 40, 0.5f}, {255, 255, 255, 255}, {0, 0}},
        {{220, 200, 0.5f}, {255, 255, 255, 255}, {0, 1}},
        {{380, 40, 0.5f}, {255, 255, 255, 255}, {1, 0}},
        {{380, 40, 0.5f}, {255, 255, 255, 255}, {1, 0}},
        {{220, 200, 0.5f}, {255, 255, 255, 255}, {0, 1}},
        {{380, 200, 0.5f}, {255, 255, 255, 255}, {1, 1}},
    };
    Sommet* sommets = (Sommet*)linearAlloc(sizeof modele);
    memcpy(sommets, modele, sizeof modele);

    C3D_BufInfo* tampons = C3D_GetBufInfo();
    BufInfo_Init(tampons);
    BufInfo_Add(tampons, sommets, sizeof(Sommet), 3, 0x210);

    C3D_Tex texture;
    C3D_TexInit(&texture, 64, 64, GPU_RGBA8);
    C3D_TexSetFilter(&texture, GPU_NEAREST, GPU_NEAREST);
    remplirTexture(&texture);
    C3D_TexBind(0, &texture);

    C3D_Mtx projection;
    Mtx_OrthoTilt(&projection, 0.0f, 400.0f, 240.0f, 0.0f, 0.0f, 1.0f, true);

    journal("Rendu prêt. START pour quitter.");

    u32 images = 0;
    u64 debut = osGetTime();
    while (aptMainLoop()) {
        hidScanInput();
        if (hidKeysDown() & KEY_START) break;

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C3D_RenderTargetClear(cible, C3D_CLEAR_ALL, COULEUR_FOND, 0);
        C3D_FrameDrawOn(cible);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uProjection, &projection);

        C3D_TexEnv* env = C3D_GetTexEnv(0);
        C3D_TexEnvInit(env);
        C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR, 0, 0);
        C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
        C3D_DrawArrays(GPU_TRIANGLES, 0, 3);

        env = C3D_GetTexEnv(0);
        C3D_TexEnvInit(env);
        C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR, 0);
        C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);
        C3D_DrawArrays(GPU_TRIANGLES, 3, 6);
        C3D_FrameEnd(0);

        images++;
        if (images == 300) {
            u64 duree = osGetTime() - debut;
            journal("OK : %lu images en %llu ms", (unsigned long)images, (unsigned long long)duree);
        }
    }

    journal("Fin du test.");
    if (g_journal) fclose(g_journal);
    C3D_TexDelete(&texture);
    linearFree(sommets);
    shaderProgramFree(&programme);
    DVLB_Free(dvlb);
    C3D_Fini();
    gfxExit();
    return 0;
}
