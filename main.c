/*
    James William Fletcher (github.com/mrbid)
        November 2022

    To reduce file size the icosphere could be
    generated on program execution by subdividing
    a icosahedron and then snapping the points to
    a unit sphere; and producing an index buffer
    for each triangle from each subdivision.
    
    Get current epoch: date +%s
    Start online game: ./fat <msaa> <future epoch time>

    Every player will start at the same epoch and the
    game simulation should run exactly the same across
    all systems with a small amount of deviation over
    time due to the accuracy of dt (unavoidable) and
    the start time accuracy that hangs on the precision
    of the microtime function.

    Players only transmit a single registration and then
    their positions as a vec3 in byte format.

    Very simple, low bandwidth, requires only a central
    server, no port forwarding required other than port 80
    on the server or port 443 for https, but this is set
    to use http (80) as default.

    I was tempted to go for an IPv4 P2P model, but with the
    state of the web these days, most small games are expected
    to utilise http if they want to eventually run in a
    browser, and you skip all the IPv4/IPv6 mess.

    Game is limited to 60 FPS because it seemed like a nice
    median for all players to keep up with while keeping a
    smooth experience and not thrashing the http protocol
    too badly. This could be decoupled from the rendering
    but there's not a lot of point other than increasing
    the smoothness of view panning and that's not bad at
    60 FPS anyway. It's more efficient to keep them coupled.
    But the latency of the rendering functions could have an
    impact on the deviation of the simulation over time.

    All players have to stick to the same FPS to minimise the
    deviance of the simulation over time.
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <pthread.h>
#include <sys/time.h>
#include <sys/file.h>
#include <unistd.h>
#include <curl/curl.h>
CURL *curl;

#pragma GCC diagnostic ignored "-Wunused-result"

#define uint GLuint
#define sint GLint
#define f32 GLfloat

#include "inc/gl.h"
#define GLFW_INCLUDE_NONE
#include "inc/glfw3.h"

#ifndef __x86_64__
    #define NOSSE
#endif

#define SEIR_RAND

#include "inc/esAux2.h"
#include "inc/res.h"
#include "assets/ncube.h"
#include "assets/exo.h"
#include "assets/inner.h"
#include "assets/rock1.h"
#include "assets/rock2.h"
#include "assets/rock3.h"
#include "assets/rock4.h"
#include "assets/rock5.h"
#include "assets/rock6.h"
#include "assets/rock7.h"
#include "assets/rock8.h"
#include "assets/rock9.h"

//*************************************
// globals
//*************************************
GLFWwindow* window;
uint winw = 1024;
uint winh = 768;
double t = 0;   // time
f32 dt = 0;     // delta time
double fc = 0;  // frame count
double lfct = 0;// last frame count time
f32 aspect;
double rww, ww, rwh, wh, ww2, wh2;
double uw, uh, uw2, uh2; // normalised pixel dpi
double x,y;

// render state id's
GLint projection_id;
GLint modelview_id;
GLint position_id;
GLint lightpos_id;
GLint solidcolor_id;
GLint color_id;
GLint opacity_id;
GLint normal_id; // 

// render state matrices
mat projection;
mat view;
mat model;
mat modelview;

// models
ESModel mdlMenger;
ESModel mdlExo;
ESModel mdlInner;
ESModel mdlRock[9];

// camera vars
#define FAR_DISTANCE 10000.f
vec lightpos = {0.f, 0.f, 0.f};
uint focus_cursor = 0;
double sens = 0.001;
f32 xrot = 0.f, yrot = 0.f;

// game vars
#define GFX_SCALE 0.01f
#define MOVE_SPEED 0.003f
uint keystate[6] = {0};
vec pp = {0.f, 0.f, 0.f};
vec ppr = {0.f, 0.f, -2.3f};
uint hits = 0;
uint brake = 0;
uint damage = 0;
double start_time = 0.0;
uint high_ping = 0;
time_t sepoch = 0;
unsigned short uid = 0;
uint last_update = 0;

#define MAX_PLAYERS 7
float players[MAX_PLAYERS*3] = {0};

typedef struct
{
    vec dir, pos;
    f32 rot, scale, speed;
} comet;
#define NUM_COMETS 64
comet comets[NUM_COMETS];

//*************************************
// utility functions
//*************************************
void timestamp(char* ts)
{
    const time_t tt = time(0);
    strftime(ts, 16, "%H:%M:%S", localtime(&tt));
}
uint64_t microtime()
{
    struct timeval tv;
    struct timezone tz;
    memset(&tz, 0, sizeof(struct timezone));
    gettimeofday(&tv, &tz);
    return 1000000 * tv.tv_sec + tv.tv_usec;
}
inline uint millitime()
{
    return microtime()*1000;
}
unsigned short urand16()
{
    int f = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    unsigned short s = 0;
    read(f, &s, sizeof(unsigned short));
    close(f);
    return s;
}
void scaleBuffer(GLfloat* b, GLsizeiptr s)
{
    for(GLsizeiptr i = 0; i < s; i++)
        b[i] *= GFX_SCALE;
}
void doExoImpact(vec p, float f)
{
    //if(f < 0.003793040058F){return;}
    GLsizeiptr s = exo_numvert*3;
    for(GLsizeiptr i = 0; i < s; i+=3)
    {
        vec v = {exo_vertices[i], exo_vertices[i+1], exo_vertices[i+2]};
        const f32 ds = vDist(v, p);
        if(ds < f)
        {
            vNorm(&v);
            const f32 sr = f-ds;
            vMulS(&v, v, sr);
            if(sr > 0.03f){damage++;}
            exo_vertices[i]   -= v.x;
            exo_vertices[i+1] -= v.y;
            exo_vertices[i+2] -= v.z;
            exo_colors[i]   -= 0.2f;
            exo_colors[i+1] -= 0.2f;
            exo_colors[i+2] -= 0.2f;
        }
    }
    esRebind(GL_ARRAY_BUFFER, &mdlExo.vid, exo_vertices, exo_vertices_size, GL_STATIC_DRAW);
    esRebind(GL_ARRAY_BUFFER, &mdlExo.cid, exo_colors, exo_colors_size, GL_STATIC_DRAW);
}
void randComet(uint i)
{
    vRuvBT(&comets[i].pos);
    vMulS(&comets[i].pos, comets[i].pos, 10.f);

    vec dp;
    vRuvTA(&dp);
    vSub(&comets[i].dir, comets[i].pos, dp);
    vNorm(&comets[i].dir);
    vInv(&comets[i].dir);

    vec of = comets[i].dir;
    vInv(&of);
    vMulS(&of, of, randf()*6.f);
    vAdd(&comets[i].pos, comets[i].pos, of);

    comets[i].rot = randf()*300.f;
    comets[i].scale = 0.01f+(randf()*0.07f);
    comets[i].speed = 0.16f+(randf()*0.08f);
}
void randComets()
{
    for(uint i = 0; i < NUM_COMETS; i++)
        randComet(i);
}
void incrementHits()
{
    hits++;
    char title[256];
    const uint max_damage = exo_numvert/2;
    sprintf(title, "Online Fractal Attack | %u | %.2f%% | %.2f mins", hits, (100.f/(float)max_damage)*(float)damage, (glfwGetTime()-start_time)/60.0);
    glfwSetWindowTitle(window, title);

    if(damage >= max_damage)
    {
        for(uint i = 0; i < NUM_COMETS; i++)
        {
            comets[i].speed = -1.f;
            comets[i].rot = 0.f;
        }

        sprintf(title, "Online Fractal Attack | %u | 100%% | %.2f mins | GAME END", hits, (glfwGetTime()-start_time)/60.0);
        glfwSetWindowTitle(window, title);
    }
}

void curlUpdateGame(const time_t sepoch, const unsigned short uid);
static size_t cb(void *data, size_t size, size_t nmemb, void *p)
{
    if(nmemb > 0 && nmemb <= 84){memcpy(&players, data, nmemb);}
    last_update = millitime();
    curlUpdateGame(sepoch, uid);
    return 0;
}
void curlUpdateGame(const time_t sepoch, const unsigned short uid)
{
    // might want to add a cache buster to the url
    unsigned char d[12];
    memcpy(&d, (unsigned char*)&ppr, 12);
    char url[256];
    sprintf(url, "http://vfcash.co.uk/fat/fat.php?r=%lu&u=%hu&p=%%%02X%%%02X%%%02X%%%02X%%%02X%%%02X%%%02X%%%02X%%%02X%%%02X%%%02X%%%02X", sepoch, uid, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7], d[8], d[9], d[10], d[11]);
    //printf("%s\n", url);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, high_ping);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, cb);
    curl_easy_perform(curl);
}
void curlRegisterGame(const time_t sepoch, const unsigned short uid)
{
    // might want to add a cache buster to the url
    char url[256];
    sprintf(url, "http://vfcash.co.uk/fat/fat.php?r=%lu&u=%hu", sepoch, uid);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, time(0)-sepoch);

    FILE *devnull = fopen("/dev/null", "w+");
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, devnull);
    curl_easy_perform(curl);
    fclose(devnull);
}
void *netThread(void *arg)
{
    useconds_t wait = high_ping*1000; // ms to µs
    if(wait < 10000){wait = 10000;} // 10 ms min loop time (should be 16 but lets try to stay ahead of the curve)
    while(1)
    {
        usleep(wait);
        if(millitime()-last_update > high_ping)
        {
            curlUpdateGame(sepoch, uid);
            last_update = time(0);
        }
    }
}

//*************************************
// update & render
//*************************************
void main_loop()
{
//*************************************
// time delta for frame interpolation
//*************************************
    static double lt = 0;
    if(lt == 0){lt = t;}
    dt = t-lt;
    lt = t;

//*************************************
// keystates
//*************************************

    static vec vd;
    mGetViewDir(&vd, view);

    static uint inverted = 0;
    if(view.m[1][1] < 0.f) // could probably do some bit magic here to transfer the signed part of the view float to a 1 float to avoid the branching and turn it into a multiply op to invert the signs
        inverted = 1;
    else
        inverted = 0;

    static uint locked = 0;
    if(view.m[1][1] > -0.03f && view.m[1][1] < 0.03f)
        locked = 1;
    else
        locked = 0;

    if(keystate[2] == 1) // W
    {
        vec m;
        vMulS(&m, vd, MOVE_SPEED * dt);
        vSub(&pp, pp, m);
    }

    if(keystate[3] == 1) // S
    {
        vec m;
        vMulS(&m, vd, MOVE_SPEED * dt);
        vAdd(&pp, pp, m);
    }

    if(keystate[0] == 1) // A
    {
        vec vdc;
        vCross(&vdc, vd, (vec){0.f, 1.f, 0.f});
        vec m;
        vMulS(&m, vdc, MOVE_SPEED * dt);
        if(inverted == 0)
            vAdd(&pp, pp, m);
        else
            vSub(&pp, pp, m);
    }

    if(keystate[1] == 1) // D
    {
        vec vdc;
        vCross(&vdc, vd, (vec){0.f, 1.f, 0.f});
        vec m;
        vMulS(&m, vdc, MOVE_SPEED * dt);
        if(inverted == 0)
            vSub(&pp, pp, m);
        else
            vAdd(&pp, pp, m);
    }

    if(keystate[4] == 1) // SPACE
    {
        vec vdc;
        vCross(&vdc, vd, (vec){0.f, 1.f, 0.f});
        vCross(&vdc, vd, vdc);
        vec m;
        vMulS(&m, vdc, MOVE_SPEED * dt);
        if(inverted == 0)
            vAdd(&pp, pp, m);
        else
            vSub(&pp, pp, m);
    }

    if(keystate[5] == 1) // SHIFT
    {
        vec vdc;
        vCross(&vdc, vd, (vec){0.f, 1.f, 0.f});
        vCross(&vdc, vd, vdc);
        vec m;
        vMulS(&m, vdc, MOVE_SPEED * dt);
        if(inverted == 0)
            vSub(&pp, pp, m);
        else
            vAdd(&pp, pp, m);
    }

    if(brake == 1)
        vMulS(&pp, pp, 0.99f*(1.f-dt));

    vAdd(&ppr, ppr, pp);

    const f32 pmod = vMod(ppr);
    if(pmod < 1.14f)
    {
        const f32 hf = vMod(pp)*10.f;
        vec n = ppr;
        vNorm(&n);
         vReflect(&pp, pp, (vec){-n.x, -n.y, -n.z}); // better if I don't normalise pp
         vMulS(&pp, pp, 0.3f);
        vMulS(&n, n, 1.14f - pmod);
        vAdd(&ppr, ppr, n);
    }

//*************************************
// camera
//*************************************

    if(focus_cursor == 1)
    {
        glfwGetCursorPos(window, &x, &y);
        
        if(locked == 0)
        {
            if(inverted == 0)
                xrot += (ww2-x)*sens;
            else
                xrot -= (ww2-x)*sens;
        }
        yrot += (wh2-y)*sens;

        // if(yrot > d2PI)
        //     yrot = d2PI;
        // if(yrot < -d2PI)
        //     yrot = -d2PI;

        glfwSetCursorPos(window, ww2, wh2);
    }

    mIdent(&view);
    mRotate(&view, yrot, 1.f, 0.f, 0.f);
    mRotate(&view, xrot, 0.f, 1.f, 0.f);
    // mRotY(&view, yrot);
    // mRotX(&view, xrot);
    mTranslate(&view, ppr.x, ppr.y, ppr.z);

    static f32 ft = 0.f;
    ft += dt*0.03f;
    lightpos.x = sinf(ft) * 6.3f;
    lightpos.y = cosf(ft) * 6.3f;
    lightpos.z = sinf(ft) * 6.3f;

//*************************************
// render
//*************************************
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    ///
    
    shadeLambert2(&position_id, &projection_id, &modelview_id, &lightpos_id, &color_id, &opacity_id);
    glUniformMatrix4fv(projection_id, 1, GL_FALSE, (GLfloat*) &projection.m[0][0]);
    glUniform3f(lightpos_id, lightpos.x, lightpos.y, lightpos.z);
    glUniform1f(opacity_id, 1.f);
    
    ///

    glBindBuffer(GL_ARRAY_BUFFER, mdlExo.vid);
    glVertexAttribPointer(position_id, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(position_id);

    glBindBuffer(GL_ARRAY_BUFFER, mdlExo.cid);
    glVertexAttribPointer(color_id, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(color_id);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mdlExo.iid);

    glUniformMatrix4fv(modelview_id, 1, GL_FALSE, (GLfloat*) &view.m[0][0]);
    glDrawElements(GL_TRIANGLES, exo_numind, GL_UNSIGNED_INT, 0);

    /// the inner wont draw now if occluded by the exo due to depth buffer

    glBindBuffer(GL_ARRAY_BUFFER, mdlInner.vid);
    glVertexAttribPointer(position_id, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(position_id);

    glBindBuffer(GL_ARRAY_BUFFER, mdlInner.cid);
    glVertexAttribPointer(color_id, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(color_id);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mdlExo.iid);

    glUniformMatrix4fv(modelview_id, 1, GL_FALSE, (GLfloat*) &view.m[0][0]);
    glDrawElements(GL_TRIANGLES, exo_numind, GL_UNSIGNED_INT, 0);

    ///

    // lambert
    shadeLambert(&position_id, &projection_id, &modelview_id, &lightpos_id, &color_id, &opacity_id);
    glUniformMatrix4fv(projection_id, 1, GL_FALSE, (GLfloat*) &projection.m[0][0]);
    glUniform3f(lightpos_id, 0.f, 0.f, 0.f);
    glUniform1f(opacity_id, 1.f);

    // bind menger
    glBindBuffer(GL_ARRAY_BUFFER, mdlMenger.vid);
    glVertexAttribPointer(position_id, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(position_id);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mdlMenger.iid);

    // "light source" dummy object
    mIdent(&model);
    mTranslate(&model, lightpos.x, lightpos.y, lightpos.z);
    mScale(&model, 3.4f, 3.4f, 3.4f);
    glUniform3f(color_id, 1.f, 1.f, 0.f);
    mMul(&modelview, &model, &view);
    glUniformMatrix4fv(modelview_id, 1, GL_FALSE, (f32*) &modelview.m[0][0]);
    glDrawElements(GL_TRIANGLES, ncube_numind, GL_UNSIGNED_INT, 0);

    // lambert
    shadeLambert3(&position_id, &projection_id, &modelview_id, &lightpos_id, &normal_id, &color_id, &opacity_id);
    glUniformMatrix4fv(projection_id, 1, GL_FALSE, (GLfloat*) &projection.m[0][0]);
    glUniform3f(lightpos_id, 0.f, 0.f, 0.f);

    // comets
    static const uint rcs = NUM_COMETS / 9;
    static const f32 rrcs = 1.f / (f32)rcs;
    int bindstate = -1;
    int cbs = -1;
    for(uint i = 0; i < NUM_COMETS; i++)
    {
        // simulation
        if(comets[i].speed == 0.f) // explode
        {
            if(cbs != 0)
            {
                glBindBuffer(GL_ARRAY_BUFFER, mdlRock[1].cid);
                glVertexAttribPointer(color_id, 3, GL_FLOAT, GL_FALSE, 0, 0);
                glEnableVertexAttribArray(color_id);
                cbs = 0;
            }

            comets[i].dir.x -= 0.3f*dt;
            comets[i].scale -= 0.03f*dt;
            if(comets[i].dir.x <= 0.f || comets[i].scale <= 0.f)
            {
                randComet(i);
                continue;
            }
            glUniform1f(opacity_id, comets[i].dir.x);
        }
        else if(comets[i].speed != -1.f) // detect impacts
        {
            // increment position
            const f32 pi = comets[i].speed*dt;
            vAdd(&comets[i].pos, comets[i].pos, (vec){comets[i].dir.x*pi,  comets[i].dir.y*pi, comets[i].dir.z*pi});

            // planet impact
            if(vMod(comets[i].pos) < 1.14f)
            {
                doExoImpact((vec){comets[i].pos.x, comets[i].pos.y, comets[i].pos.z}, (comets[i].scale+(comets[i].speed*0.1f))*1.2f);

                vec fwd;
                vMulS(&fwd, comets[i].dir, 0.03f);
                vAdd(&comets[i].pos, comets[i].pos, fwd);
                comets[i].speed = 0.f;
                comets[i].dir.x = 1.f;
                comets[i].scale *= 2.f;

                incrementHits();
                continue;
            }

            // player impact
            const f32 cd = vDist((vec){-ppr.x, -ppr.y, -ppr.z}, comets[i].pos);
            const f32 cs = comets[i].scale+0.06f;
            if(cd < cs)
            {
                if(high_ping == 0)
                {
                    vec n = ppr;
                    vNorm(&n);
                    vMulS(&n, n, cs-cd);
                    vAdd(&ppr, ppr, n);

                    vec ccd = comets[i].pos;
                    vSub(&ccd, ppr, ccd);
                    vNorm(&ccd);

                    vReflect(&pp, pp, ccd);
                    vMulS(&pp, pp, 0.3f);
                }

                comets[i].speed = 0.f;
                comets[i].dir.x = 1.f;
                //comets[i].scale *= 2.f;
            }

            // flip to grey if red
            if(cbs != 1)
            {
                glBindBuffer(GL_ARRAY_BUFFER, mdlRock[0].cid);
                glVertexAttribPointer(color_id, 3, GL_FLOAT, GL_FALSE, 0, 0);
                glEnableVertexAttribArray(color_id);
                cbs = 1;
            }
        }
        else
        {
            if(cbs != 1)
            {
                glBindBuffer(GL_ARRAY_BUFFER, mdlRock[0].cid);
                glVertexAttribPointer(color_id, 3, GL_FLOAT, GL_FALSE, 0, 0);
                glEnableVertexAttribArray(color_id);
                cbs = 1;
            }
        }

        // translate comet
        mIdent(&model);
        mTranslate(&model, comets[i].pos.x, comets[i].pos.y, comets[i].pos.z);

        // rotate comet
        const f32 mag = comets[i].rot*0.01f*t;
        if(comets[i].rot < 100.f)
            mRotY(&model, mag);
        if(comets[i].rot < 200.f)
            mRotZ(&model, mag);
        if(comets[i].rot < 300.f)
            mRotX(&model, mag);
        
        // scale comet
        mScale(&model, comets[i].scale, comets[i].scale, comets[i].scale);

        // make modelview
        mMul(&modelview, &model, &view);
        glUniformMatrix4fv(modelview_id, 1, GL_FALSE, (f32*) &modelview.m[0][0]);

        // bind one of the 8 rock models
        uint nbs = i * rrcs;
        if(nbs > 8){nbs = 8;}
        if(nbs != bindstate)
        {
            glBindBuffer(GL_ARRAY_BUFFER, mdlRock[nbs].vid);
            glVertexAttribPointer(position_id, 3, GL_FLOAT, GL_FALSE, 0, 0);
            glEnableVertexAttribArray(position_id);

            glBindBuffer(GL_ARRAY_BUFFER, mdlRock[nbs].nid);
            glVertexAttribPointer(normal_id, 3, GL_FLOAT, GL_FALSE, 0, 0);
            glEnableVertexAttribArray(normal_id);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mdlRock[nbs].iid);
            bindstate = nbs;
        }

        // draw it
        if(comets[i].speed == 0.f)
        {
            glEnable(GL_BLEND);
            glDrawElements(GL_TRIANGLES, rock1_numind, GL_UNSIGNED_SHORT, 0);
            glDisable(GL_BLEND);
        }
        else
            glDrawElements(GL_TRIANGLES, rock1_numind, GL_UNSIGNED_SHORT, 0);
    }

    // players
    glBindBuffer(GL_ARRAY_BUFFER, mdlRock[2].cid);
    glVertexAttribPointer(color_id, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(color_id);
    for(uint i = 0; i < MAX_PLAYERS; i++)
    {
        const uint j = i*3;
        if(players[j] != 0.f || players[j+1] != 0.f || players[j+2] != 0.f)
        {
            for(uint k = 0; k < NUM_COMETS; k++)
            {
                const f32 cd = vDist((vec){-players[j], -players[j+1], -players[j+2]}, comets[k].pos);
                const f32 cs = comets[i].scale+0.06f;
                if(cd < cs)
                {
                    comets[k].speed = 0.f;
                    comets[k].dir.x = 1.f;
                }
            }

            mIdent(&model);
            mTranslate(&model, -players[j], -players[j+1], -players[j+2]);
            mScale(&model, 0.01f, 0.01f, 0.01f);
            mMul(&modelview, &model, &view);
            glUniformMatrix4fv(modelview_id, 1, GL_FALSE, (f32*) &modelview.m[0][0]);
            glDrawElements(GL_TRIANGLES, rock1_numind, GL_UNSIGNED_SHORT, 0);
        }
    }
    
    // swap
    glfwSwapBuffers(window);
}

//*************************************
// Input Handelling
//*************************************
static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if(action == GLFW_PRESS)
    {
        if(key == GLFW_KEY_A){ keystate[0] = 1; }
        else if(key == GLFW_KEY_D){ keystate[1] = 1; }
        else if(key == GLFW_KEY_W){ keystate[2] = 1; }
        else if(key == GLFW_KEY_S){ keystate[3] = 1; }
        else if(key == GLFW_KEY_SPACE){ keystate[4] = 1; }
        else if(key == GLFW_KEY_LEFT_SHIFT){ keystate[5] = 1; }
        else if(key == GLFW_KEY_LEFT_CONTROL){ brake = 1; }
        else if(key == GLFW_KEY_F)
        {
            if(t-lfct > 2.0)
            {
                char strts[16];
                timestamp(&strts[0]);
                printf("[%s] FPS: %g\n", strts, fc/(t-lfct));
                lfct = t;
                fc = 0;
            }
        }
        else if(key == GLFW_KEY_V)
        {
            // view inversion
            printf(":: %+.2f\n", view.m[1][1]);

            // dump view matrix
            printf("%+.2f %+.2f %+.2f %+.2f\n", view.m[0][0], view.m[0][1], view.m[0][2], view.m[0][3]);
            printf("%+.2f %+.2f %+.2f %+.2f\n", view.m[1][0], view.m[1][1], view.m[1][2], view.m[1][3]);
            printf("%+.2f %+.2f %+.2f %+.2f\n", view.m[2][0], view.m[2][1], view.m[2][2], view.m[2][3]);
            printf("%+.2f %+.2f %+.2f %+.2f\n", view.m[3][0], view.m[3][1], view.m[3][2], view.m[3][3]);
            printf("---\n");

            // randomly dump an online player at your position
            // const uint j = esRand(0, 6)*3;
            // players[j] = ppr.x;
            // players[j+1] = ppr.y;
            // players[j+2] = ppr.z;
        }
    }
    else if(action == GLFW_RELEASE)
    {
        if(key == GLFW_KEY_A){ keystate[0] = 0; }
        else if(key == GLFW_KEY_D){ keystate[1] = 0; }
        else if(key == GLFW_KEY_W){ keystate[2] = 0; }
        else if(key == GLFW_KEY_S){ keystate[3] = 0; }
        else if(key == GLFW_KEY_SPACE){ keystate[4] = 0; }
        else if(key == GLFW_KEY_LEFT_SHIFT){ keystate[5] = 0; }
        else if(key == GLFW_KEY_LEFT_CONTROL){ brake = 0; }
    }
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    if(action == GLFW_PRESS)
    {
        if(button == GLFW_MOUSE_BUTTON_LEFT)
        {
            focus_cursor = 1 - focus_cursor;
            if(focus_cursor == 0)
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            else
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
            glfwSetCursorPos(window, ww2, wh2);
            glfwGetCursorPos(window, &ww2, &wh2);
        }
        else if(button == GLFW_MOUSE_BUTTON_RIGHT)
            brake = 1;
    }
    else if(action == GLFW_RELEASE)
        brake = 0;
}

void window_size_callback(GLFWwindow* window, int width, int height)
{
    winw = width;
    winh = height;

    glViewport(0, 0, winw, winh);
    aspect = (f32)winw / (f32)winh;
    ww = (double)winw;
    wh = (double)winh;
    rww = 1.0/ww;
    rwh = 1.0/wh;
    ww2 = ww/2.0;
    wh2 = wh/2.0;
    uw = (double)aspect/ww;
    uh = 1.0/wh;
    uw2 = (double)aspect/ww2;
    uh2 = 1.0/wh2;

    mIdent(&projection);
    mPerspective(&projection, 60.0f, aspect, 0.01f, FAR_DISTANCE);
    glUniformMatrix4fv(projection_id, 1, GL_FALSE, (GLfloat*) &projection.m[0][0]);
}

//*************************************
// Process Entry Point
//*************************************
int main(int argc, char** argv)
{
    // allow custom msaa level
    int msaa = 16;
    if(argc >= 2){msaa = atoi(argv[1]);}

    // start at epoch
    sepoch = time(0);
    if(argc >= 3){sepoch = atoll(argv[2]);}

    // high ping flag
    if(argc >= 4){high_ping = atoi(argv[3]);}
    if(high_ping > 0 && (high_ping <= 16 || high_ping > 333)){high_ping = 333;}

    // gen client UID
    uid = urand16();

    // help
    printf("----\n");
    printf("Online Fractal Attack\n");
    printf("----\n");
    printf("James William Fletcher (github.com/mrbid)\n");
    printf("----\n");
    printf("Argv(2): msaa 0-16, start epoch, high ping 0-333\n");
    printf("F = FPS to console.\n");
    printf("W, A, S, D, SPACE, LEFT SHIFT\n");
    printf("Right Click to Brake\n");
    printf("----\n");
    printf("epotch: %lu\n", sepoch);
    printf("uid: %hu\n", uid);
    printf("high ping: %u\n", high_ping);
    printf("----\n");

    // init glfw
    if(!glfwInit()){printf("glfwInit() failed.\n"); exit(EXIT_FAILURE);}
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_SAMPLES, msaa);
    window = glfwCreateWindow(winw, winh, "Fractal Attack", NULL, NULL);
    if(!window)
    {
        printf("glfwCreateWindow() failed.\n");
        glfwTerminate();
        exit(EXIT_FAILURE);
    }
    const GLFWvidmode* desktop = glfwGetVideoMode(glfwGetPrimaryMonitor());
    glfwSetWindowPos(window, (desktop->width/2)-(winw/2), (desktop->height/2)-(winh/2)); // center window on desktop
    glfwSetWindowSizeCallback(window, window_size_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwMakeContextCurrent(window);
    gladLoadGL(glfwGetProcAddress);
    glfwSwapInterval(0); // 0 for immediate updates, 1 for updates synchronized with the vertical retrace, -1 for adaptive vsync

    // set icon
    glfwSetWindowIcon(window, 1, &(GLFWimage){16, 16, (unsigned char*)&icon_image.pixel_data});

    // init curl
    curl = curl_easy_init();
    if(curl == NULL)
    {
        printf("curl_easy_init() failed.\n");
        exit(EXIT_FAILURE);
    }
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 0);
    curl_easy_setopt(curl, CURLOPT_TCP_FASTOPEN, 1);
    curl_easy_setopt(curl, CURLOPT_TCP_NODELAY, 1);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "fractalattack-agent/1.0");

    // create network thread
    pthread_t tid;
    if(pthread_create(&tid, NULL, netThread, NULL) != 0)
    {
        printf("pthread_create() failed.");
        return 0;
    }

    // register game
    curlRegisterGame(sepoch, uid);

//*************************************
// projection
//*************************************

    window_size_callback(window, winw, winh);

//*************************************
// bind vertex and index buffers
//*************************************

    // ***** BIND MENGER *****
    scaleBuffer(ncube_vertices, ncube_numvert*3);
    esBind(GL_ARRAY_BUFFER, &mdlMenger.vid, ncube_vertices, sizeof(ncube_vertices), GL_STATIC_DRAW);
    esBind(GL_ELEMENT_ARRAY_BUFFER, &mdlMenger.iid, ncube_indices, sizeof(ncube_indices), GL_STATIC_DRAW);

    // ***** BIND INNER *****
    scaleBuffer(exo_vertices, exo_numvert*3);
    esBind(GL_ARRAY_BUFFER, &mdlInner.vid, exo_vertices, exo_vertices_size, GL_STATIC_DRAW);
    esBind(GL_ARRAY_BUFFER, &mdlInner.cid, inner_colors, sizeof(inner_colors), GL_STATIC_DRAW);

    // ***** BIND EXO *****
    GLsizeiptr s = exo_numvert*3;
    for(GLsizeiptr i = 0; i < s; i+=3)
    {
        const f32 g = (exo_colors[i] + exo_colors[i+1] + exo_colors[i+2]) / 3;
        const f32 h = (1.f-g)*0.01f;
        vec v = {exo_vertices[i], exo_vertices[i+1], exo_vertices[i+2]};
        vNorm(&v);
        vMulS(&v, v, h);
        exo_vertices[i]   -= v.x;
        exo_vertices[i+1] -= v.y;
        exo_vertices[i+2] -= v.z;
        exo_vertices[i] *= 1.03f;
        exo_vertices[i+1] *= 1.03f;
        exo_vertices[i+2] *= 1.03f;
    }
    esBind(GL_ARRAY_BUFFER, &mdlExo.vid, exo_vertices, exo_vertices_size, GL_STATIC_DRAW);
    esBind(GL_ARRAY_BUFFER, &mdlExo.cid, exo_colors, exo_colors_size, GL_STATIC_DRAW);
    esBind(GL_ELEMENT_ARRAY_BUFFER, &mdlExo.iid, exo_indices, exo_indices_size, GL_STATIC_DRAW);

    // ***** BIND ROCK1 *****
    esBind(GL_ARRAY_BUFFER, &mdlRock[0].vid, rock1_vertices, sizeof(rock1_vertices), GL_STATIC_DRAW);
    esBind(GL_ARRAY_BUFFER, &mdlRock[0].nid, rock1_normals, sizeof(rock1_normals), GL_STATIC_DRAW);
    esBind(GL_ARRAY_BUFFER, &mdlRock[0].cid, rock1_colors, sizeof(rock1_colors), GL_STATIC_DRAW);
    esBind(GL_ARRAY_BUFFER, &mdlRock[0].iid, rock1_indices, sizeof(rock1_indices), GL_STATIC_DRAW);

    // ***** BIND ROCK2 *****
    esBind(GL_ARRAY_BUFFER, &mdlRock[1].vid, rock2_vertices, sizeof(rock2_vertices), GL_STATIC_DRAW);
    esBind(GL_ARRAY_BUFFER, &mdlRock[1].nid, rock2_normals, sizeof(rock2_normals), GL_STATIC_DRAW);
    s = rock2_numvert*3;
    for(GLsizeiptr i = 0; i < s; i+=3)
    {
        rock2_colors[i] = randf();
        if(rock2_colors[i] > 0.5f)
            rock2_colors[i+1] = randf()*0.5f;
        else
            rock2_colors[i+1] = 0.f;
        rock2_colors[i+2] = 0.f;
    }
    esBind(GL_ARRAY_BUFFER, &mdlRock[1].cid, rock2_colors, sizeof(rock2_colors), GL_STATIC_DRAW);
    esBind(GL_ARRAY_BUFFER, &mdlRock[1].iid, rock2_indices, sizeof(rock2_indices), GL_STATIC_DRAW);

    // ***** BIND ROCK3 *****
    esBind(GL_ARRAY_BUFFER, &mdlRock[2].vid, rock3_vertices, sizeof(rock3_vertices), GL_STATIC_DRAW);
    esBind(GL_ARRAY_BUFFER, &mdlRock[2].nid, rock3_normals, sizeof(rock3_normals), GL_STATIC_DRAW);
    s = rock3_numvert*3;
    for(GLsizeiptr i = 0; i < s; i+=3)
    {
        rock3_colors[i] = 0.f;
        rock3_colors[i+1] = randf();
        rock3_colors[i+2] = rock3_colors[i+1]+(randf()*(1.f-rock3_colors[i+1]));
    }
    esBind(GL_ARRAY_BUFFER, &mdlRock[2].cid, rock3_colors, sizeof(rock3_colors), GL_STATIC_DRAW);
    esBind(GL_ARRAY_BUFFER, &mdlRock[2].iid, rock3_indices, sizeof(rock3_indices), GL_STATIC_DRAW);

    // ***** BIND ROCK4 *****
    esBind(GL_ARRAY_BUFFER, &mdlRock[3].vid, rock4_vertices, sizeof(rock4_vertices), GL_STATIC_DRAW);
    esBind(GL_ARRAY_BUFFER, &mdlRock[3].nid, rock4_normals, sizeof(rock4_normals), GL_STATIC_DRAW);
    //esBind(GL_ARRAY_BUFFER, &mdlRock[3].cid, rock4_colors, sizeof(rock4_colors), GL_STATIC_DRAW);
    esBind(GL_ARRAY_BUFFER, &mdlRock[3].iid, rock4_indices, sizeof(rock4_indices), GL_STATIC_DRAW);

    // ***** BIND ROCK5 *****
    esBind(GL_ARRAY_BUFFER, &mdlRock[4].vid, rock5_vertices, sizeof(rock5_vertices), GL_STATIC_DRAW);
    esBind(GL_ARRAY_BUFFER, &mdlRock[4].nid, rock5_normals, sizeof(rock5_normals), GL_STATIC_DRAW);
    //esBind(GL_ARRAY_BUFFER, &mdlRock[4].cid, rock5_colors, sizeof(rock5_colors), GL_STATIC_DRAW);
    esBind(GL_ARRAY_BUFFER, &mdlRock[4].iid, rock5_indices, sizeof(rock5_indices), GL_STATIC_DRAW);

    // ***** BIND ROCK6 *****
    esBind(GL_ARRAY_BUFFER, &mdlRock[5].vid, rock6_vertices, sizeof(rock6_vertices), GL_STATIC_DRAW);
    esBind(GL_ARRAY_BUFFER, &mdlRock[5].nid, rock6_normals, sizeof(rock6_normals), GL_STATIC_DRAW);
    //esBind(GL_ARRAY_BUFFER, &mdlRock[5].cid, rock6_colors, sizeof(rock6_colors), GL_STATIC_DRAW);
    esBind(GL_ARRAY_BUFFER, &mdlRock[5].iid, rock6_indices, sizeof(rock6_indices), GL_STATIC_DRAW);

    // ***** BIND ROCK7 *****
    esBind(GL_ARRAY_BUFFER, &mdlRock[6].vid, rock7_vertices, sizeof(rock7_vertices), GL_STATIC_DRAW);
    esBind(GL_ARRAY_BUFFER, &mdlRock[6].nid, rock7_normals, sizeof(rock7_normals), GL_STATIC_DRAW);
    //esBind(GL_ARRAY_BUFFER, &mdlRock[6].cid, rock7_colors, sizeof(rock7_colors), GL_STATIC_DRAW);
    esBind(GL_ARRAY_BUFFER, &mdlRock[6].iid, rock7_indices, sizeof(rock7_indices), GL_STATIC_DRAW);

    // ***** BIND ROCK8 *****
    esBind(GL_ARRAY_BUFFER, &mdlRock[7].vid, rock8_vertices, sizeof(rock8_vertices), GL_STATIC_DRAW);
    esBind(GL_ARRAY_BUFFER, &mdlRock[7].nid, rock8_normals, sizeof(rock8_normals), GL_STATIC_DRAW);
    //esBind(GL_ARRAY_BUFFER, &mdlRock[7].cid, rock8_colors, sizeof(rock8_colors), GL_STATIC_DRAW);
    esBind(GL_ARRAY_BUFFER, &mdlRock[7].iid, rock8_indices, sizeof(rock8_indices), GL_STATIC_DRAW);

    // ***** BIND ROCK9 *****
    esBind(GL_ARRAY_BUFFER, &mdlRock[8].vid, rock9_vertices, sizeof(rock9_vertices), GL_STATIC_DRAW);
    esBind(GL_ARRAY_BUFFER, &mdlRock[8].nid, rock9_normals, sizeof(rock9_normals), GL_STATIC_DRAW);
    //esBind(GL_ARRAY_BUFFER, &mdlRock[8].cid, rock9_colors, sizeof(rock9_colors), GL_STATIC_DRAW);
    esBind(GL_ARRAY_BUFFER, &mdlRock[8].iid, rock9_indices, sizeof(rock9_indices), GL_STATIC_DRAW);

//*************************************
// compile & link shader programs
//*************************************

    makeLambert();
    makeLambert2();
    makeLambert3();

//*************************************
// configure render options
//*************************************

    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.f, 0.f, 0.f, 0.f);

//*************************************
// execute update / render loop
//*************************************

    // wait until epoch
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glfwSwapBuffers(window);
    glfwSetWindowTitle(window, "Please wait...");
    //printf("%lu\n%lu\n-\n", (time_t)((double)microtime()*0.000001), time(0));
    while((time_t)((double)microtime()*0.000001) < sepoch)
    {
        usleep(1000); // this reduces the accuracy by the range in microseconds
        char title[256];
        sprintf(title, "Please wait... %lu seconds.", sepoch-time(0));
        glfwSetWindowTitle(window, title);
    }
    glfwSetWindowTitle(window, "Online Fractal Attack");

    // set start time
    start_time = glfwGetTime();

    // seed random
    srandf(sepoch);

    // set comets
    randComets();

    // init
    t = glfwGetTime();
    lfct = t;
    
    // fps accurate event loop
    const double maxfps = 60.0;
    useconds_t wait_interval = 1000000 / maxfps;
    useconds_t wait = wait_interval;
    while(!glfwWindowShouldClose(window))
    {
        usleep(wait);
        t = glfwGetTime();
        
        // tick internal state
        glfwPollEvents();
        main_loop();

        // accurate fps
        wait = wait_interval - (useconds_t)((glfwGetTime() - t) * 1000000.0);
        if(wait > wait_interval)
            wait = wait_interval;
        
        fc++;
    }

    // done
    glfwDestroyWindow(window);
    glfwTerminate();
    exit(EXIT_SUCCESS);
    return 0;
}