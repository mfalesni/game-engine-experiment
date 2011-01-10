#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <allegro.h>
#include <string.h>
#include <assert.h>
#include <math.h>


static const int width = 800;
static const int height = 600;
static const uint16_t ms = 15;

// vrstvy
static const uint8_t LAYER_BACK =  1;
static const uint8_t LAYER_FRONT = 2;
static const uint8_t LAYER_GROUND = 0;

char dataPath[] = "data/";
char actorPath[] = "actors/";
char mapPath[] = "maps/";
char tilePath[] = "tiles/";
char soundPath[] = "sounds/";
char musicPath[] = "music/";

volatile int timeVar = 0;

void timer_callback(void)
{
    if(timeVar > 0)
        timeVar--;
}
END_OF_FUNCTION(timer_callback)

// pohyb: e n ne nw s se sw w

typedef enum {D_E, D_N, D_NE, D_NW, D_S, D_SE, D_SW, D_W} TDirections;
typedef unsigned int usint;
typedef unsigned int uint;
typedef short int sint;
typedef unsigned long int ulint;


// OBJEKTY
typedef struct tobject
{
    char ident[16];
    char name[16];
    BITMAP *bitmap;     // Pokud je null, tak uz byla bitmapa nahrana nekde predtim
    uint16_t objWidth;
    uint16_t objHeight;
    int posX;
    int posY;
    // kolizni data
    uint16_t collX;          // souradnice vztazneho bodu pro kolize
    uint16_t collY;
    uint16_t collRadius;     // polomer kolize
    bool ghost;         // da se projit, nebo ne?
    // zvuk pri chuzi
    SAMPLE *step;
    // vykreslovaci vsrtva
    bool ground;
    uint8_t currLayer;

} TObject;

// Struktura Aktora
typedef struct tactor
{
    char ident[16];
    // Props
    uint8_t numDir;
    uint8_t numFrames;
    uint16_t maxHP;
    // Aktualni
    uint8_t currDir;
    int8_t currFrame;
    uint16_t currHP;
    int32_t currX;
    int32_t currY;
    // Bitmapy
    BITMAP **walking;
    BITMAP **standing;
    // Directions vectors
    int *dirX;
    int *dirY;
    // metrics
    int actWidth;
    int actHeight;
    // kolizni data
    uint16_t collX;          // souradnice vztazneho bodu pro kolize
    uint16_t collY;
    uint16_t collRadius;     // polomer kolize


} TActor;

typedef struct tmap
{
    char ident[16];

    uint16_t mapWidth;
    uint16_t mapHeight;
    // pozadi
    BITMAP *background; // bitmapa
    SAMPLE *step;       // zvuk kroku
    MIDI *bgMusic;      // Hudba na pozadi
    // Seznam objektu
    TObject *objects;
    uint16_t numObjects;
} TMap;

// OBJEKTY




TActor loadActor(char *name, char *ident)
{
    printf("Loading actor type '%s' with identificator '%s'\n", name, ident);
    TActor actor;
    char filename[256];
    char base[256];
    strcpy(filename, "");
    strcat(filename, dataPath);
    strcat(filename, actorPath);
    strcat(filename, name);
    strcpy(base, filename);
    strcat(filename, ".act");
    printf("> Config filename: %s\n", filename);
    FILE *file = fopen(filename, "r");
    fscanf(file, "%hhu", &actor.numDir);
    fscanf(file, "%hhu", &actor.numFrames);
    fscanf(file, "%hu", &actor.maxHP);
    strcpy(actor.ident, ident);
    // actor.dirX
    actor.dirX = malloc(actor.numDir * sizeof(int));
    for(int i = 0; i < actor.numDir; i++)
        fscanf(file, "%d", &actor.dirX[i]);
    // actor.dirY
    actor.dirY = malloc(actor.numDir * sizeof(int));
    for(int i = 0; i < actor.numDir; i++)
        fscanf(file, "%d", &actor.dirY[i]);

    // koordinaty kolizniho stredoveho bodu
    fscanf(file, "%hu", &actor.collX);
    fscanf(file, "%hu", &actor.collY);
    fscanf(file, "%hu", &actor.collRadius);

    fclose(file);
    // Nastaveni zakladnich veci
    actor.currDir = 0;
    actor.currFrame = -1;
    actor.currHP = actor.maxHP;
    // props nahrane, ted nacist obrazky (fuj)
    char complete[256];
    // Alokace stojicich obrazku
    actor.standing = malloc(actor.numDir * sizeof(BITMAP *));
    // Nacteni stojicich obrazku
    printf("> Loading standing anims ...\n");
    for(int i = 0; i < actor.numDir; i++)
    {
        snprintf(complete, 255, "%s%s%02d.bmp", base, "s", i);
        actor.standing[i] = NULL;
        actor.standing[i] = load_bitmap(complete, NULL);
        if(actor.standing[i] == NULL)
            printf("F");
        else
            printf(".");
    }
    printf(" ... done!\n");
    // Nacteni stojicich obrazku
    actor.walking = malloc(actor.numDir * actor.numFrames * sizeof(BITMAP *));
    printf("> Loading walking anims ...\n");
    for(int i = 0; i < actor.numDir*actor.numFrames; i++)
    {
        snprintf(complete, 255, "%s%s%02d.bmp", base, "w", i);
        actor.walking[i] = NULL;
        actor.walking[i] = load_bitmap(complete, NULL);
        if(actor.walking[i] == NULL)
            printf("F");
        else
            printf(".");
    }
    printf(" done!\n");
    actor.actWidth = actor.standing[0]->w;
    actor.actHeight = actor.standing[0]->h;
    actor.currX = 500;
    actor.currY = 160;

    printf("Finished loading actor type '%s' with identificator '%s'\n", name, ident);

    return actor;
}

void unloadActor(TActor actor)
{
    printf("Unloading actor '%s'\n", actor.ident);
    for(int i = 0; i < actor.numFrames; i++)
    {
        destroy_bitmap(actor.standing[i]);
    }
    free(actor.standing);
    for(int i = 0; i < actor.numDir * actor.numFrames; i++)
    {
        destroy_bitmap(actor.walking[i]);
    }
    free(actor.walking);
    printf("Actor '%s' unloaded\n", actor.ident);
}


TObject objectLoad(char *name, char *ident, int posX, int posY, BITMAP *loadBitmap, bool ground)
{
    printf("Loading object '%s' with identificator '%s'\n", name, ident);
    TObject result;
    result.ground = ground;
    strncpy(result.ident, ident, 16);
    strncpy(result.name, name, 16);
    char filename[256];
    if(loadBitmap == NULL)
    {
        snprintf(filename, 255, "%s%s%s.bmp", dataPath, tilePath, name);
        printf("> Loading bitmap '%s'\n", filename);
        result.bitmap = load_bitmap(filename, NULL);
        result.objWidth = result.bitmap->w;
        result.objHeight = result.bitmap->h;
    }
    else
    {
        result.bitmap = loadBitmap;
        result.objWidth = result.bitmap->w;
        result.objHeight = result.bitmap->h;

    }
    result.posX = posX;
    result.posY = posY;
    // Nacteni dalsich vlastnosti
    snprintf(filename, 255, "%s%s%s.obj", dataPath, tilePath, name);
    FILE *file = fopen(filename, "r");
    assert(file != NULL);
    // step sample
    fscanf(file, "%s", filename);
    if(strcmp(filename, "NULL") == 0 || strcmp(filename, "null") == 0)
        result.step = NULL;
    else
    {
        char pom[256];
        snprintf(pom, 255, "%s%s%s", dataPath, soundPath, filename);
        printf("> Loading sample '%s'\n", pom);
        result.step = load_sample(pom);
    }
    // ghost flag
    int pom;
    fscanf(file, "%d", &pom);
    if(pom == 0)
        result.ghost = false;
    else
        result.ghost = true;
    // koordinaty kolizniho stredoveho bodu
    fscanf(file, "%hu", &result.collX);
    fscanf(file, "%hu", &result.collY);
    fscanf(file, "%hu", &result.collRadius);
    // hotovo, hell yeah!

    fclose(file);
    result.currLayer = LAYER_BACK;
    printf("Loaded object '%s' with identificator '%s'\n", name, ident);
    return result;
}

void objectUnload(TObject object)
{
    printf("Unloading object '%s'\n", object.ident);
    if(object.bitmap != NULL)
    {
        printf("> Destroying bitmap\n");
        destroy_bitmap(object.bitmap);
        object.bitmap = NULL;
    }

    if(object.step != NULL)
    {
        printf("> Destroying sound\n");
        destroy_sample(object.step);
        object.step = NULL;
    }

    object.posX = 0;
    object.posY = 0;
    object.objHeight = 0;
    object.objWidth = 0;
    printf("Object '%s' unloaded\n", object.ident);
    return;
}

TMap mapLoad(char *name, char *ident)
{
    printf("Loading map '%s' with identificator '%s'\n", name, ident);
    TMap result;
    strncpy(result.ident, ident, 16);
    char filename[256];
    strcpy(filename, "");
    strcat(filename, dataPath);
    strcat(filename, mapPath);
    strcat(filename, name);
    strcat(filename, ".map");
    printf("> Opening '%s'\n", filename);
    FILE *file = fopen(filename, "r");
    fscanf(file, "%s", filename);
    fscanf(file, "%hu", &result.mapWidth);
    fscanf(file, "%hu", &result.mapHeight);
    char target[256];
    snprintf(target, 255, "%s%sbkgd_%s.bmp", dataPath, tilePath, filename);
    printf("> Loading background bitmap '%s'\n", target);
    result.background = load_bitmap(target, NULL);
    snprintf(target, 255, "%s%sdefault_%s.wav", dataPath, soundPath, filename);
    printf("> Loading '%s' step sound\n", target);
    result.step = load_sample(target);
    snprintf(target, 255, "%s%s%s.mid", dataPath, musicPath, name);
    printf("> Loading background music file from '%s'\n", target);
    result.bgMusic = load_midi(target);
    assert(result.bgMusic != NULL);
    assert(result.background != NULL);
    printf("> Loading objects ...\n");
    char objId[128];
    char objName[128];
    int x;
    int y;
    int mallocIncrement = 1;
    int mallocSize = 0;
    result.numObjects = 0;
    result.objects = NULL;
    char layertype[256];
    while(fscanf(file, "%s %s %d %d %s", objId, objName, &x, &y, layertype) == 5)
    {
        printf(">> Loading object '%s' (type '%s')\n", objName, objId);
        if(mallocSize == result.numObjects)
        {
            mallocSize += mallocIncrement++;
            result.objects = realloc(result.objects, mallocSize * sizeof(TObject));
        }
        int i = 0;
        BITMAP *tmp = NULL;
        for(; i < result.numObjects; i++)
            if(strcmp(result.objects[i].name, objId) == 0)
            {
                tmp = result.objects[i].bitmap;
                break;
            }
        bool gndLayer = (strcmp(layertype, "ground") == 0) ? true : false;
        result.objects[result.numObjects++] = objectLoad(objId, objName, x, y, tmp, gndLayer);

    }
    fclose(file);

    return result;
}

void mapUnload(TMap map)
{
    printf("Unloading map '%s'\n", map.ident);
    destroy_bitmap(map.background);
    destroy_midi(map.bgMusic);
    destroy_sample(map.step);
    map.mapHeight = 0;
    map.mapWidth = 0;
    // Odpojeni ukazatelu na bitmapy
    for(int i = 0; i < map.numObjects; i++)
    {
        // prohledame, zda stejny ukazatel nema jeste nekdo jiny
        for(int j = 0; j < map.numObjects; j++)
        {
            if(i == j || map.objects[i].bitmap == NULL || map.objects[j].bitmap == NULL)  // nema smysl kontrolovat sebe sama nebo null
                continue;

            if(map.objects[i].bitmap == map.objects[j].bitmap)
                map.objects[j].bitmap = NULL;
        }
    }

    for(int i = 0; i < map.numObjects; i++)
        objectUnload(map.objects[i]);

    map.numObjects = 0;

    return;
}

void objDraw(BITMAP *buffer, TMap map, int layer,int leftBegin, int topBegin)
{
    // Vykresleni objektu v predni vrstve
    //printf("leftbegin = %u\n", leftBegin);
    for(int i = 0; i < map.numObjects; i++)
    {
        if(map.objects[i].currLayer != layer)
            continue;

        int pozX = map.objects[i].posX - leftBegin - (map.objects[i].objWidth / 2 );
        int pozY = map.objects[i].posY - topBegin - (map.objects[i].objHeight / 2 );
        if(map.objects[i].bitmap != NULL)
            draw_sprite(buffer, map.objects[i].bitmap, pozX, pozY);
        else
        {   // toto bude moc fuj :/
            for(int j = 0; j < map.numObjects; j++)
                if(strcmp(map.objects[j].name, map.objects[j].name) == 0)
                {
                    draw_sprite(buffer, map.objects[j].bitmap, pozX, pozY);
                    break;
                }   // ani nebylo, strasil jsem :D
        }
        //printf("pozX = %u, pozY = %u\n", pozX, pozY);
    }
}

void drawGame(BITMAP *buffer, TMap map, TActor player)
{
    clear_to_color(buffer, makecol(0, 0, 0));
    // nejprve pozadi
    int bgWidth = map.background->w;
    int bgHeight = map.background->h;
    int leftBegin = player.currX - (width / 2);
    int topBegin = player.currY - (height / 2);
    // upravime
    // levy kraj
    leftBegin = (leftBegin > 0) ? leftBegin : 0;
    // pravy kraj
    leftBegin = (leftBegin + width < map.mapWidth) ? leftBegin : map.mapWidth - width;
    // horni kraj
    topBegin = (topBegin > 0) ? topBegin : 0;
    // spodni kraj
    topBegin = (topBegin + height < map.mapHeight) ? topBegin : map.mapHeight - height;


    // ted vime, kde zacina obrazovka
    // dopocitani posuvu pozadi
    int shiftX = 0;
    int shiftY = 0;
    while(shiftX < leftBegin)
        shiftX += bgWidth;
    shiftX -= leftBegin;
    shiftX %= map.background->w;
    while(shiftY < topBegin)
        shiftY += bgHeight;
    shiftY -= topBegin;
    shiftY %= map.background->h;
    // debug info
    //printf("leftBegin = %d topBegin = %d shiftX = %d shiftY = %d\n", leftBegin, topBegin, shiftX, shiftY);

    for(int y = shiftY-bgHeight; y < height; y += bgHeight)
        for(int x = shiftX-bgWidth; x < width; x += bgWidth)
        {
            draw_sprite(buffer, map.background, x, y);
        }
    /* HELL YEAH! */

    // Vykresleni objektu v zadni vrstve
    //printf("leftbegin = %u\n", leftBegin);
    objDraw(buffer, map, LAYER_GROUND, leftBegin, topBegin);
    objDraw(buffer, map, LAYER_BACK, leftBegin, topBegin);
    /*for(int i = 0; i < map.numObjects; i++)
    {
        if(map.objects[i].currLayer != LAYER_BACK)
            continue;

        int pozX = map.objects[i].posX - leftBegin - (map.objects[i].objWidth / 2 );
        int pozY = map.objects[i].posY - topBegin - (map.objects[i].objHeight / 2 );
        if(map.objects[i].bitmap != NULL)
            draw_sprite(buffer, map.objects[i].bitmap, pozX, pozY);
        else
        {   // toto bude moc fuj :/
            for(int j = 0; j < map.numObjects; j++)
                if(strcmp(map.objects[j].name, map.objects[j].name) == 0)
                {
                    draw_sprite(buffer, map.objects[j].bitmap, pozX, pozY);
                    break;
                }
        }
        //printf("pozX = %u, pozY = %u\n", pozX, pozY);
    }*/

    // Vykresleni hrace
    int actorX = player.currX - leftBegin - (player.actWidth / 2);
    int actorY = player.currY - topBegin - (player.actHeight / 2);

    if(player.currFrame == -1)
        draw_sprite(buffer, player.standing[player.currDir], actorX, actorY);
    else
        draw_sprite(buffer, player.walking[player.currDir*player.numFrames + player.currFrame], actorX, actorY);

    // Vykresleni objektu v predni vrstve
    //printf("leftbegin = %u\n", leftBegin);
    objDraw(buffer, map, LAYER_FRONT, leftBegin, topBegin);
    /*for(int i = 0; i < map.numObjects; i++)
    {
        if(map.objects[i].currLayer != LAYER_FRONT)
            continue;

        int pozX = map.objects[i].posX - leftBegin - (map.objects[i].objWidth / 2 );
        int pozY = map.objects[i].posY - topBegin - (map.objects[i].objHeight / 2 );
        if(map.objects[i].bitmap != NULL)
            draw_sprite(buffer, map.objects[i].bitmap, pozX, pozY);
        else
        {   // toto bude moc fuj :/
            for(int j = 0; j < map.numObjects; j++)
                if(strcmp(map.objects[j].name, map.objects[j].name) == 0)
                {
                    draw_sprite(buffer, map.objects[j].bitmap, pozX, pozY);
                    break;
                }   // ani nebylo, strasil jsem :D
        }
        //printf("pozX = %u, pozY = %u\n", pozX, pozY);
    }*/

    return;
}





void Die(char *str)
{
    fprintf(stderr, "\nFatal error: %s\n", str);
    exit(EXIT_FAILURE);
}

int main(void)
{
    printf("MF Engine, compiled %s %s\n", __TIME__, __DATE__);
    printf("Initializing MF Engine ... ");
    allegro_init();
    set_window_title("MF Engine");
    install_keyboard();
    install_mouse();
    install_sound(DIGI_AUTODETECT, MIDI_AUTODETECT, NULL);
    set_color_depth(32);
    set_gfx_mode(GFX_AUTODETECT_WINDOWED, width,height,0,0);
    // Fontz init
    PALETTE palette;
    FONT *font = load_font("data/UNIFONT.pcx", palette, NULL);
    assert(font != NULL);
    textout_ex(screen, font, "LOADING", (width / 2)-30, (height / 2), makecol(255, 255, 255), -1);
    printf("done\nCreate backbuffer ... ");
    BITMAP *back = create_bitmap(width, height);
    if(back == NULL)
        Die("Unable to create back buffer!\n");
    else
        printf("OK\n");

    TActor actor = loadActor("a", "Player");
    TMap map = mapLoad("mapa1", "Base map");

    install_timer();
    LOCK_VARIABLE(timeVar);
    LOCK_FUNCTION(timer_callback);
    install_int(timer_callback, ms);

    printf("Start main game loop\n");
    play_midi(map.bgMusic, 1);
    while(!key[KEY_ESC])
    {
        while(timeVar > 0)
            rest(5);

        timeVar = 4;
        poll_keyboard();

        // Nastaveni smeru
        int direction = -1;

        // klavesa nahoru
        if(key[KEY_W] || key[KEY_UP])
        {
            poll_keyboard();
            if(key[KEY_D] || key[KEY_RIGHT])
                direction = D_NE;
            else if(key[KEY_A] || key[KEY_LEFT])
                direction = D_NW;
            else
                direction = D_N;
        }
        // klavesa dolu
        else if(key[KEY_S] || key[KEY_DOWN])
        {
            poll_keyboard();
            if(key[KEY_D] || key[KEY_RIGHT])
                direction = D_SE;
            else if(key[KEY_A] || key[KEY_LEFT])
                direction = D_SW;
            else
                direction = D_S;
        }
        // klavesa vlevo
        else if(key[KEY_A] || key[KEY_LEFT])
        {
            poll_keyboard();
            if(key[KEY_W] || key[KEY_UP])
                direction = D_NW;
            else if(key[KEY_S] || key[KEY_DOWN])
                direction = D_SW;
            else
                direction = D_W;
        }
        // klavesa vpravo
        else if(key[KEY_D] || key[KEY_RIGHT])
        {
            poll_keyboard();
            if(key[KEY_W] || key[KEY_UP])
                direction = D_NE;
            else if(key[KEY_S] || key[KEY_DOWN])
                direction = D_SE;
            else
                direction = D_E;
        }
        SAMPLE *toPlay = map.step;  // zvuk kroku, ktery se ma prehrat
        if(direction != -1)
        {
            actor.currFrame = (actor.currFrame+1) % actor.numFrames;
            int backX = actor.currX;
            int backY = actor.currY;
            actor.currX += actor.dirX[direction];
            actor.currX = (actor.currX < 0) ? 0 : actor.currX;
            actor.currX = (actor.currX > map.mapWidth) ? map.mapWidth : actor.currX;
            actor.currY += actor.dirY[direction];
            actor.currY = (actor.currY < 0) ? 0 : actor.currY;
            actor.currY = (actor.currY > map.mapHeight) ? map.mapHeight : actor.currY;
            actor.currDir = direction;

            // kontrola na kolize
            for(int i = 0; i < map.numObjects; i++)
            {
                // pokud je to typu ghost, tak to neresime
                map.objects[i].currLayer = LAYER_BACK;
                if(map.objects[i].ground == true)
                {
                    map.objects[i].currLayer = LAYER_GROUND;
                    //continue;
                }

                // vzdalenost stredovych koliznich bodu
                int cpaX = actor.currX + actor.collX;
                int cpaY = actor.currY + actor.collY;

                if(map.objects[i].ghost) // zkontrolujeme, zda neslapeme na neco jineho, kvuli zvuku
                {
                    if(map.objects[i].step == NULL) // pokud neni zvuk definovan, nechame ho byt
                        continue;

                    // vypocitame hranicni body
                    int leftBound = map.objects[i].posX;
                    int rightBound = map.objects[i].posX + map.objects[i].objWidth;
                    int topBound = map.objects[i].posY;
                    int bottomBound = map.objects[i].posY + map.objects[i].objHeight;
                    // Uprava souradnic
                    int plY = cpaY - (actor.collY - ( actor.actHeight / 2));

                    // Zkontrolujeme, zda aktor je v objektu, potom se prehraje jiny zvuk
                    if( (cpaX > leftBound) && (cpaX < rightBound) && (plY > topBound) && (plY < bottomBound) )
                        if(map.objects[i].step != NULL) // a samozrejme nesmi byt null :)
                            toPlay = map.objects[i].step;

                    //printf("O: (%d,%d,%d,%d), A: (%d,%d)\n", leftBound, rightBound, topBound, bottomBound, cpaX, cpaY);

                    continue;
                }


                // vzdalenost stredovych koliznich bodu
                int cpbX = map.objects[i].posX + map.objects[i].collX;
                int cpbY = map.objects[i].posY + map.objects[i].collY;
                int length = round( sqrtf( pow(cpbX - cpaX, 2) + pow(cpbY - cpaY, 2) ) );
                if(length < (actor.collRadius + map.objects[i].collRadius) )
                {   // nastala kolize
                    actor.currX = backX;
                    actor.currY = backY;
                    if( cpaY < cpbY )   // prekreslime jen kdyz jsme ZA objektem
                        map.objects[i].currLayer = LAYER_FRONT;
                }
                // kontrola, jestli jsme za objektem, pokud ano, tak objekt kreslime v popredi
                if( (cpaX > map.objects[i].posX) && (cpaX < map.objects[i].posX+map.objects[i].objWidth) ) // X-ove souradnice
                    if( (cpaY > map.objects[i].posY) && ( cpaY < cpbY)  ) // Y-ove souradnice
                        map.objects[i].currLayer = LAYER_FRONT;

            }
            //printf("pos: X=%d Y=%d\n", actor.currX, actor.currY);
        }
        else
        {
            actor.currFrame = -1;
        }

        // prehrani zvuku
        if(actor.currFrame == 1 || actor.currFrame == 5)
            play_sample(toPlay, 255, 128, 1000, 0);

        toPlay = NULL;

        drawGame(back, map, actor);
        // game info
        textout_ex(back, font, "MF Engine DEMO | Controls: W,S,A,D,arrows | Quit = Esc", 10, 10, makecol(255, 255, 255), -1);
        // draw that!
        draw_sprite(screen, back, 0, 0);
    }
    stop_midi();
    printf("Finished!\n");

    unloadActor(actor);
    mapUnload(map);
    // Splashscreen at end
    clear_to_color(back, makecol(0,0,0));
    textout_ex(back, font, "Thank you for testing!", (width / 2) - 130, (height / 2), makecol(255, 255, 255), -1);
    draw_sprite(screen, back, 0, 0);
    rest(2000);

    destroy_bitmap(back);
    destroy_font(font);

    allegro_exit();


    return EXIT_SUCCESS;
}

END_OF_MAIN()
